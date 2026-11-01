from __future__ import annotations

import json
import queue
import struct
import threading
import time
import tkinter as tk
import zlib
from pathlib import Path
from tkinter import messagebox, ttk
from typing import Any, Callable

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None


BAUD_RATE = 115200
REQUEST_TIMEOUT = 2.0
MAGIC = b"LS"
PROTOCOL_VERSION = 1
PACKET_COMMAND = 0
PACKET_RESPONSE = 1
PACKET_DATA = 2
MAX_FRAME_PAYLOAD_BYTES = 2048
MAX_JSON_PAYLOAD_BYTES = 256
INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1


def load_schema() -> dict[str, dict[str, Any]]:
    config_path = Path(__file__).resolve().parent.parent.parent / "config.json"
    with config_path.open("r", encoding="utf-8") as config_file:
        document = json.load(config_file)
    fields = document.get("fields")
    if not isinstance(fields, dict) or not fields:
        raise ValueError("В config.json отсутствует непустой объект fields")
    for name, definition in fields.items():
        if not isinstance(name, str) or not isinstance(definition, dict):
            raise ValueError("Некорректное описание поля в config.json")
        if definition.get("type") not in {"int32", "bool", "string", "shunt_res"}:
            raise ValueError(f"Поле {name}: неподдерживаемый тип")
        if "default" not in definition:
            raise ValueError(f"Поле {name}: отсутствует значение default")
    return fields


class SerialClient:
    def __init__(self) -> None:
        self._port: Any = None
        self._write_lock = threading.Lock()
        self._responses: queue.Queue[dict[str, Any]] = queue.Queue()
        self._sweeps: queue.Queue[list[tuple[int, int]]] = queue.Queue()
        self._reader: threading.Thread | None = None
        self._telemetry_lock = threading.Lock()
        self._latest_telemetry: dict[str, int] | None = None

    @property
    def connected(self) -> bool:
        return self._port is not None and self._port.is_open

    def connect(self, port_name: str) -> None:
        if serial is None:
            raise RuntimeError("Не установлен pyserial. Выполните: pip install -r laptop/python/requirements.txt")
        self.close()
        self._port = serial.Serial(
            port=port_name,
            baudrate=BAUD_RATE,
            timeout=0.1,
            write_timeout=REQUEST_TIMEOUT,
        )
        # Many USB-UART adapters reset ESP32 when the port is opened.
        time.sleep(1.5)
        self._port.reset_input_buffer()
        self._responses = queue.Queue()
        self._sweeps = queue.Queue()
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def close(self) -> None:
        if self._port is not None:
            try:
                self._port.close()
            finally:
                self._port = None
        self._reader = None

    @staticmethod
    def _encode_frame(packet_type: int, payload: bytes) -> bytes:
        if len(payload) > 0xFFFF:
            raise ValueError("Слишком большой размер полезной нагрузки")
        header = MAGIC + bytes(((packet_type << 5) | PROTOCOL_VERSION,)) + struct.pack("<H", len(payload))
        body = header + payload
        return body + struct.pack("<I", zlib.crc32(body))

    def _read_exactly(self, size: int, deadline: float, port: Any = None) -> bytes:
        result = bytearray()
        source = self._port if port is None else port
        while len(result) < size and time.monotonic() < deadline:
            source.timeout = max(0.05, deadline - time.monotonic())
            chunk = source.read(size - len(result))
            if chunk:
                result.extend(chunk)
        if len(result) != size:
            raise TimeoutError("ESP32 не передала кадр полностью")
        return bytes(result)

    def _read_frame(self, deadline: float, port: Any = None) -> tuple[int, bytes]:
        matched = 0
        while time.monotonic() < deadline:
            byte = self._read_exactly(1, deadline, port)[0]
            if byte == MAGIC[matched]:
                matched += 1
                if matched == len(MAGIC):
                    break
            else:
                matched = 1 if byte == MAGIC[0] else 0
        header_tail = self._read_exactly(3, deadline, port)
        type_and_version, payload_size = struct.unpack("<BH", header_tail)
        if (type_and_version & 0x1F) != PROTOCOL_VERSION:
            raise RuntimeError("Неподдерживаемая версия протокола ESP32")
        if payload_size > MAX_FRAME_PAYLOAD_BYTES:
            raise RuntimeError("ESP32 объявила слишком большой кадр")
        payload = self._read_exactly(payload_size, deadline, port)
        received_crc = struct.unpack("<I", self._read_exactly(4, deadline, port))[0]
        if zlib.crc32(MAGIC + header_tail + payload) != received_crc:
            raise RuntimeError("Ошибка CRC кадра от ESP32")
        return type_and_version >> 5, payload

    def _read_loop(self) -> None:
        port = self._port
        while port is not None and self._port is port and port.is_open:
            try:
                packet_type, payload = self._read_frame(time.monotonic() + 0.5, port)
            except TimeoutError:
                continue
            except Exception:
                if self._port is port and port.is_open:
                    time.sleep(0.05)
                continue

            if packet_type == PACKET_RESPONSE:
                try:
                    response = json.loads(payload.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError):
                    continue
                if isinstance(response, dict):
                    if response.get("type") == "telemetry":
                        with self._telemetry_lock:
                            self._latest_telemetry = response
                    else:
                        self._responses.put(response)
            elif packet_type == PACKET_DATA:
                if len(payload) < 2:
                    continue
                count = struct.unpack_from("<H", payload)[0]
                if count > 100 or len(payload) != 2 + count * 4:
                    continue
                points = [
                    struct.unpack_from("<hh", payload, 2 + index * 4)
                    for index in range(count)
                ]
                self._sweeps.put(points)

    def latest_telemetry(self) -> dict[str, int] | None:
        with self._telemetry_lock:
            result = self._latest_telemetry
            self._latest_telemetry = None
            return result

    def request(self, payload: dict[str, Any]) -> dict[str, Any]:
        encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        if len(encoded) > MAX_JSON_PAYLOAD_BYTES:
            raise ValueError(
                f"Запрос занимает {len(encoded)} байт, предел — {MAX_JSON_PAYLOAD_BYTES}"
            )
        if not self.connected:
            raise RuntimeError("Нет соединения с ESP32")
        with self._write_lock:
            self._port.write(self._encode_frame(PACKET_COMMAND, encoded))
            self._port.flush()
        try:
            response = self._responses.get(timeout=REQUEST_TIMEOUT)
        except queue.Empty as error:
            raise TimeoutError("ESP32 не ответила за отведённое время") from error
        if not response.get("ok", False):
            raise RuntimeError(
                f"ESP32 вернула ошибку: {response.get('error', 'unknown_error')}"
            )
        return response

    def receive_sweep(self, timeout: float = 300.0) -> list[tuple[int, int]]:
        try:
            return self._sweeps.get(timeout=timeout)
        except queue.Empty as error:
            raise TimeoutError("Sweep не завершился за отведённое время") from error




class Application(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Lengmur Socket")
        self.minsize(640, 480)
        self.geometry("760x560")
        self.client = SerialClient()
        self.schema = load_schema()
        self.field_vars: dict[str, tk.Variable] = {}
        self.busy = False
        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Не подключено")
        self.telemetry_var = tk.StringVar(value="U: —    I: — (сырые коды АЦП)")
        self.sweep_points: list[tuple[int, int]] = []
        self._build_ui()
        self.refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self.on_close)
        self.after(250, self._poll_telemetry)

    def _build_ui(self) -> None:
        connection = ttk.Frame(self, padding=(10, 10, 10, 4))
        connection.pack(fill=tk.X)
        ttk.Label(connection, text="Порт:").pack(side=tk.LEFT)
        self.port_combo = ttk.Combobox(
            connection, textvariable=self.port_var, width=20, state="readonly"
        )
        self.port_combo.pack(side=tk.LEFT, padx=(6, 6))
        ttk.Button(connection, text="Обновить", command=self.refresh_ports).pack(side=tk.LEFT)
        self.connect_button = ttk.Button(connection, text="Подключить", command=self.toggle_connection)
        self.connect_button.pack(side=tk.LEFT, padx=(6, 0))

        self.notebook = ttk.Notebook(self)
        self.settings_tab = ttk.Frame(self.notebook, padding=14)
        self.measurement_tab = ttk.Frame(self.notebook, padding=12)
        self.notebook.add(self.settings_tab, text="Настройки")
        self.notebook.add(self.measurement_tab, text="Измерения")
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=6)
        self.notebook.bind("<<NotebookTabChanged>>", self.on_tab_changed)

        measurement_header = ttk.Frame(self.measurement_tab)
        measurement_header.pack(fill=tk.X)
        ttk.Label(measurement_header, textvariable=self.telemetry_var).pack(side=tk.LEFT)
        self.sweep_button = ttk.Button(
            measurement_header, text="Снять sweep", command=self.start_sweep,
            state=tk.DISABLED,
        )
        self.sweep_button.pack(side=tk.RIGHT)
        self.graph = tk.Canvas(
            self.measurement_tab, background="white", highlightthickness=1,
            highlightbackground="#b8b8b8",
        )
        self.graph.pack(fill=tk.BOTH, expand=True, pady=(12, 0))
        self.graph.bind("<Configure>", lambda _event: self._draw_sweep())

        form = ttk.Frame(self.settings_tab)
        form.pack(fill=tk.BOTH, expand=True)
        for row, (name, definition) in enumerate(self.schema.items()):
            ttk.Label(form, text=name).grid(row=row, column=0, sticky=tk.W, padx=(0, 14), pady=5)
            if definition["type"] == "bool":
                variable: tk.Variable = tk.BooleanVar(value=definition["default"])
                widget = ttk.Checkbutton(form, variable=variable)
            elif definition["type"] == "shunt_res":
                variable = tk.StringVar(value=str(definition["default"]))
                widget = ttk.Combobox(
                    form,
                    textvariable=variable,
                    values=("0 — авто", "200", "1000", "10000", "100000", "1000000", "10000000"),
                    state="readonly",
                )
            else:
                variable = tk.StringVar(value=str(definition["default"]))
                widget = ttk.Entry(form, textvariable=variable)
            widget.grid(row=row, column=1, sticky=tk.EW, pady=5)
            self.field_vars[name] = variable
        form.columnconfigure(1, weight=1)

        actions = ttk.Frame(self.settings_tab)
        actions.pack(fill=tk.X, pady=(12, 0))
        self.reload_button = ttk.Button(
            actions, text="Перечитать", command=self.load_settings, state=tk.DISABLED
        )
        self.reload_button.pack(side=tk.LEFT)
        self.save_button = ttk.Button(
            actions, text="Записать", command=self.save_settings, state=tk.DISABLED
        )
        self.save_button.pack(side=tk.RIGHT)
        ttk.Label(self, textvariable=self.status_var, anchor=tk.W, padding=(10, 4, 10, 10)).pack(fill=tk.X)

    def refresh_ports(self) -> None:
        if list_ports is None:
            self.status_var.set("Для работы с портом установите pyserial")
            return
        current = self.port_var.get()
        ports = [port.device for port in list_ports.comports()]
        self.port_combo["values"] = ports
        if current in ports:
            self.port_var.set(current)
        elif ports:
            self.port_var.set(ports[0])
        else:
            self.port_var.set("")
            self.status_var.set("Последовательные порты не найдены")

    def toggle_connection(self) -> None:
        if self.client.connected:
            self.client.close()
            self.connect_button.configure(text="Подключить")
            self.status_var.set("Не подключено")
            self._set_actions_enabled(True)
            return
        port_name = self.port_var.get()
        if not port_name:
            messagebox.showwarning("Подключение", "Выберите последовательный порт")
            return

        def connected(_: None) -> None:
            self.connect_button.configure(text="Отключить")
            self.status_var.set(f"Подключено: {port_name}")
            self.load_settings()

        self._run_background(
            "Подключение…", lambda: self.client.connect(port_name), connected
        )

    def on_tab_changed(self, _event: tk.Event) -> None:
        if self.notebook.select() == str(self.settings_tab) and self.client.connected:
            self.load_settings()

    def _poll_telemetry(self) -> None:
        telemetry = self.client.latest_telemetry()
        if telemetry is not None:
            self.telemetry_var.set(
                f"U: {telemetry.get('voltage', '—')}    "
                f"I: {telemetry.get('current', '—')} (сырые коды АЦП)"
            )
        self.after(250, self._poll_telemetry)

    def start_sweep(self) -> None:
        def measure() -> list[tuple[int, int]]:
            self.client.request({"op": "start_sweep"})
            return self.client.receive_sweep()

        def measured(points: list[tuple[int, int]]) -> None:
            self.sweep_points = points
            self._draw_sweep()
            self.status_var.set(f"Sweep завершён: {len(points)} точек")

        self._run_background("Ожидание диапазона и сбор sweep…", measure, measured)

    def _draw_sweep(self) -> None:
        if not hasattr(self, "graph"):
            return
        self.graph.delete("all")
        width = max(1, self.graph.winfo_width())
        height = max(1, self.graph.winfo_height())
        left, right, top, bottom = 58, width - 20, 24, height - 48
        if right <= left or bottom <= top:
            return
        self.graph.create_line(left, top, left, bottom, fill="#404040")
        self.graph.create_line(left, bottom, right, bottom, fill="#404040")
        self.graph.create_text((left + right) // 2, height - 18,
                               text="Напряжение (код АЦП)")
        self.graph.create_text(17, (top + bottom) // 2,
                               text="Ток (код АЦП)", angle=90)
        if not self.sweep_points:
            self.graph.create_text((left + right) // 2, (top + bottom) // 2,
                                   text="Нажмите «Снять sweep» для измерения")
            return
        ordered_points = sorted(self.sweep_points, key=lambda point: point[0])
        voltages = [point[0] for point in ordered_points]
        currents = [point[1] for point in ordered_points]
        min_v, max_v = min(voltages), max(voltages)
        min_i, max_i = min(currents), max(currents)
        span_v = max(1, max_v - min_v)
        span_i = max(1, max_i - min_i)
        coords: list[float] = []
        for voltage, current in ordered_points:
            x = left + (voltage - min_v) * (right - left) / span_v
            y = bottom - (current - min_i) * (bottom - top) / span_i
            coords.extend((x, y))
        if len(coords) >= 4:
            self.graph.create_line(*coords, fill="#1769aa", width=2)
        else:
            self.graph.create_oval(coords[0] - 3, coords[1] - 3,
                                   coords[0] + 3, coords[1] + 3,
                                   fill="#1769aa", outline="")
        self.graph.create_text(left, top - 8, anchor=tk.W,
                               text=f"U: {min_v}…{max_v}; I: {min_i}…{max_i}")

    def load_settings(self) -> None:
        def read() -> dict[str, Any]:
            response = self.client.request({"op": "get"})
            config = response.get("config")
            if not isinstance(config, dict):
                raise RuntimeError("В ответе ESP32 отсутствует объект config")
            return config

        def loaded(config: dict[str, Any]) -> None:
            for name, variable in self.field_vars.items():
                if name not in config:
                    raise RuntimeError(f"ESP32 не вернула поле {name}")
                variable.set(config[name])
            self.status_var.set("Настройки прочитаны с ESP32")

        self._run_background("Чтение настроек…", read, loaded)

    def _collect_config(self) -> dict[str, Any]:
        config: dict[str, Any] = {}
        for name, definition in self.schema.items():
            value = self.field_vars[name].get()
            if definition["type"] == "int32":
                try:
                    value = int(value)
                except ValueError as error:
                    raise ValueError(f"Поле {name} должно быть целым числом") from error
                if not INT32_MIN <= value <= INT32_MAX:
                    raise ValueError(f"Поле {name} должно находиться в диапазоне int32")
            elif definition["type"] == "string":
                value = str(value)
            elif definition["type"] == "shunt_res":
                value = 0 if value.startswith("0") else int(value)
            else:
                value = bool(value)
            config[name] = value
        return config

    def save_settings(self) -> None:
        try:
            config = self._collect_config()
        except ValueError as error:
            messagebox.showerror("Некорректные настройки", str(error))
            return

        def saved(_: None) -> None:
            self.status_var.set("Настройки записаны в ESP32")

        self._run_background(
            "Запись настроек…",
            lambda: self.client.request({"op": "set", "config": config}),
            saved,
        )

    def _run_background(
        self,
        status: str,
        operation: Callable[[], Any],
        on_success: Callable[[Any], None],
    ) -> None:
        if self.busy:
            return
        self.busy = True
        self.status_var.set(status)
        self._set_actions_enabled(False)

        def worker() -> None:
            try:
                result = operation()
            except Exception as error:
                self.after(0, lambda error=error: self._operation_failed(error))
            else:
                self.after(0, lambda: self._operation_succeeded(result, on_success))

        threading.Thread(target=worker, daemon=True).start()

    def _operation_succeeded(self, result: Any, callback: Callable[[Any], None]) -> None:
        self.busy = False
        self._set_actions_enabled(True)
        try:
            callback(result)
        except Exception as error:
            self._operation_failed(error)

    def _operation_failed(self, error: Exception) -> None:
        self.busy = False
        self._set_actions_enabled(True)
        self.status_var.set(f"Ошибка: {error}")
        messagebox.showerror("Ошибка", str(error))

    def _set_actions_enabled(self, enabled: bool) -> None:
        state = tk.NORMAL if enabled else tk.DISABLED
        self.connect_button.configure(state=state)
        settings_state = (
            tk.NORMAL if enabled and self.client.connected else tk.DISABLED
        )
        self.sweep_button.configure(state=settings_state)
        self.reload_button.configure(state=settings_state)
        self.save_button.configure(state=settings_state)

    def on_close(self) -> None:
        self.client.close()
        self.destroy()


def main() -> None:
    try:
        app = Application()
    except Exception as error:
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("Не удалось запустить приложение", str(error))
        root.destroy()
        return
    app.mainloop()


if __name__ == "__main__":
    main()
