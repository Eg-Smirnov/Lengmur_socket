from __future__ import annotations

import json
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
MAX_PAYLOAD_BYTES = 2048
INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1


def load_schema() -> dict[str, dict[str, Any]]:
    config_path = Path(__file__).resolve().parent.parent / "config.json"
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
        self._lock = threading.Lock()

    @property
    def connected(self) -> bool:
        return self._port is not None and self._port.is_open

    def connect(self, port_name: str) -> None:
        if serial is None:
            raise RuntimeError("Не установлен pyserial. Выполните: pip install -r laptop/requirements.txt")
        with self._lock:
            self.close()
            self._port = serial.Serial(
                port=port_name,
                baudrate=BAUD_RATE,
                timeout=REQUEST_TIMEOUT,
                write_timeout=REQUEST_TIMEOUT,
            )
            # Many USB-UART adapters reset ESP32 when the port is opened.
            time.sleep(1.5)
            self._port.reset_input_buffer()

    def close(self) -> None:
        if self._port is not None:
            try:
                self._port.close()
            finally:
                self._port = None

    @staticmethod
    def _encode_frame(packet_type: int, payload: bytes) -> bytes:
        if len(payload) > 0xFFFF:
            raise ValueError("Слишком большой размер полезной нагрузки")
        header = MAGIC + bytes(((packet_type << 5) | PROTOCOL_VERSION,)) + struct.pack("<H", len(payload))
        body = header + payload
        return body + struct.pack("<I", zlib.crc32(body))

    def _read_exactly(self, size: int, deadline: float) -> bytes:
        result = bytearray()
        while len(result) < size and time.monotonic() < deadline:
            self._port.timeout = max(0.05, deadline - time.monotonic())
            chunk = self._port.read(size - len(result))
            if chunk:
                result.extend(chunk)
        if len(result) != size:
            raise TimeoutError("ESP32 не передала кадр полностью")
        return bytes(result)

    def _read_frame(self, deadline: float) -> tuple[int, bytes]:
        matched = 0
        while time.monotonic() < deadline:
            byte = self._read_exactly(1, deadline)[0]
            if byte == MAGIC[matched]:
                matched += 1
                if matched == len(MAGIC):
                    break
            else:
                matched = 1 if byte == MAGIC[0] else 0
        header_tail = self._read_exactly(3, deadline)
        type_and_version, payload_size = struct.unpack("<BH", header_tail)
        if (type_and_version & 0x1F) != PROTOCOL_VERSION:
            raise RuntimeError("Неподдерживаемая версия протокола ESP32")
        if payload_size > MAX_PAYLOAD_BYTES:
            raise RuntimeError("ESP32 объявила слишком большой кадр")
        payload = self._read_exactly(payload_size, deadline)
        received_crc = struct.unpack("<I", self._read_exactly(4, deadline))[0]
        if zlib.crc32(MAGIC + header_tail + payload) != received_crc:
            raise RuntimeError("Ошибка CRC кадра от ESP32")
        return type_and_version >> 5, payload

    def request(self, payload: dict[str, Any]) -> dict[str, Any]:
        encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        if len(encoded) > MAX_PAYLOAD_BYTES:
            raise ValueError(
                f"Запрос занимает {len(encoded)} байт, предел — {MAX_PAYLOAD_BYTES}"
            )
        with self._lock:
            if not self.connected:
                raise RuntimeError("Нет соединения с ESP32")
            self._port.write(self._encode_frame(PACKET_COMMAND, encoded))
            self._port.flush()
            deadline = time.monotonic() + REQUEST_TIMEOUT
            while time.monotonic() < deadline:
                packet_type, packet_payload = self._read_frame(deadline)
                try:
                    response = json.loads(packet_payload.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError):
                    continue
                if packet_type == PACKET_RESPONSE and isinstance(response, dict):
                    if not response.get("ok", False):
                        raise RuntimeError(
                            f"ESP32 вернула ошибку: {response.get('error', 'unknown_error')}"
                        )
                    return response
            raise TimeoutError("ESP32 не ответила за отведённое время")




class Application(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Lengmur Socket")
        self.minsize(520, 360)
        self.geometry("640x440")
        self.client = SerialClient()
        self.schema = load_schema()
        self.field_vars: dict[str, tk.Variable] = {}
        self.busy = False
        self.port_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Не подключено")
        self.voltage_raw_var = tk.StringVar(value="—")
        self.current_raw_var = tk.StringVar(value="—")
        self.shunt_res_var = tk.StringVar(value="—")
        self.out_enabled = False
        self._build_ui()
        self.refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self.on_close)

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
        self.main_tab = ttk.Frame(self.notebook, padding=14)
        self.settings_tab = ttk.Frame(self.notebook, padding=14)
        self.notebook.add(self.main_tab, text="Главная")
        self.notebook.add(self.settings_tab, text="Настройки")
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=6)
        self.notebook.bind("<<NotebookTabChanged>>", self.on_tab_changed)

        measurement = ttk.LabelFrame(
            self.main_tab, text="Однократное измерение", padding=12
        )
        measurement.pack(fill=tk.X)
        ttk.Label(measurement, text="Код напряжения:").grid(
            row=0, column=0, sticky=tk.W, pady=3
        )
        ttk.Label(measurement, textvariable=self.voltage_raw_var).grid(
            row=0, column=1, sticky=tk.E, pady=3
        )
        ttk.Label(measurement, text="Код тока:").grid(
            row=1, column=0, sticky=tk.W, pady=3
        )
        ttk.Label(measurement, textvariable=self.current_raw_var).grid(
            row=1, column=1, sticky=tk.E, pady=3
        )
        ttk.Label(measurement, text="Фактический шунт, Ом:").grid(
            row=2, column=0, sticky=tk.W, pady=3
        )
        ttk.Label(measurement, textvariable=self.shunt_res_var).grid(
            row=2, column=1, sticky=tk.E, pady=3
        )
        measurement.columnconfigure(1, weight=1)
        self.measure_button = ttk.Button(
            self.main_tab, text="Измерить", command=self.measure_once
        )
        self.measure_button.pack(anchor=tk.E, pady=(12, 0))

        output = ttk.LabelFrame(self.main_tab, text="Силовой выход", padding=12)
        output.pack(fill=tk.X, pady=(14, 0))
        self.output_state_var = tk.StringVar(value="OUT_EN: неизвестно")
        ttk.Label(output, textvariable=self.output_state_var).pack(side=tk.LEFT)
        self.output_button = ttk.Button(
            output, text="Включить", command=self.toggle_output, state=tk.DISABLED
        )
        self.output_button.pack(side=tk.RIGHT)

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
                    values=("auto", "200", "1000", "10000", "100000", "1000000", "10000000"),
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
        self.reload_button = ttk.Button(actions, text="Перечитать", command=self.load_settings)
        self.reload_button.pack(side=tk.LEFT)
        self.save_button = ttk.Button(actions, text="Записать", command=self.save_settings)
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
            self.out_enabled = False
            self.output_state_var.set("OUT_EN: неизвестно")
            self.output_button.configure(text="Включить", state=tk.DISABLED)
            self.status_var.set("Не подключено")
            return
        port_name = self.port_var.get()
        if not port_name:
            messagebox.showwarning("Подключение", "Выберите последовательный порт")
            return

        def connected(_: None) -> None:
            self.connect_button.configure(text="Отключить")
            self.status_var.set(f"Подключено: {port_name}")
            self.load_output_state(
                load_settings_after=self.notebook.select() == str(self.settings_tab)
            )

        self._run_background(
            "Подключение…", lambda: self.client.connect(port_name), connected
        )

    def on_tab_changed(self, _event: tk.Event) -> None:
        if self.notebook.select() == str(self.settings_tab) and self.client.connected:
            self.load_settings()

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

    def measure_once(self) -> None:
        def read() -> dict[str, Any]:
            response = self.client.request({"op": "measure_once"})
            sample = response.get("sample")
            if not isinstance(sample, dict):
                raise RuntimeError("В ответе ESP32 отсутствует объект sample")
            for field in ("voltage_raw", "current_raw", "shunt_res"):
                if not isinstance(sample.get(field), int):
                    raise RuntimeError(f"В результате измерения отсутствует поле {field}")
            return sample

        def measured(sample: dict[str, Any]) -> None:
            self.voltage_raw_var.set(str(sample["voltage_raw"]))
            self.current_raw_var.set(str(sample["current_raw"]))
            self.shunt_res_var.set(str(sample["shunt_res"]))
            self.status_var.set("Однократное измерение выполнено")

        self._run_background("Выполнение измерения…", read, measured)

    def load_output_state(self, load_settings_after: bool = False) -> None:
        def loaded(response: dict[str, Any]) -> None:
            self._output_state_received(response)
            if load_settings_after:
                self.load_settings()

        self._run_background(
            "Чтение состояния OUT_EN…",
            lambda: self.client.request({"op": "get_out_state"}),
            loaded,
        )

    def toggle_output(self) -> None:
        self._run_background(
            "Переключение OUT_EN…",
            lambda: self.client.request({"op": "toggle_out"}),
            self._output_state_received,
        )

    def _output_state_received(self, response: dict[str, Any]) -> None:
        enabled = response.get("out_enabled")
        if not isinstance(enabled, bool):
            raise RuntimeError("В ответе ESP32 отсутствует поле out_enabled")
        self.out_enabled = enabled
        self.output_state_var.set(f"OUT_EN: {'HIGH' if enabled else 'LOW'}")
        self.output_button.configure(text="Выключить" if enabled else "Включить")
        self.status_var.set(f"OUT_EN установлен в {'HIGH' if enabled else 'LOW'}")

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
                value = str(value)
                if value != "auto":
                    value = int(value)
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
        self.measure_button.configure(state=state)
        self.output_button.configure(
            state=tk.NORMAL if enabled and self.client.connected else tk.DISABLED
        )
        self.reload_button.configure(state=state)
        self.save_button.configure(state=state)

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
