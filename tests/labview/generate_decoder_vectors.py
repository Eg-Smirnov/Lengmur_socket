#!/usr/bin/env python3
"""Generate Decode Frames.vi test vectors and expected results.

Uses only the Python standard library. Produces Markdown and JSON vectors for
the Python ActiveX runner.
"""

import json
from pathlib import Path
import zlib


MAGIC = b"\x4c\x53"
MAX_PAYLOAD = 2048
ERROR_UNSUPPORTED_VERSION = 5004


def make_frame(packet_type, payload=b"", version=1):
    if not 0 <= packet_type <= 7:
        raise ValueError("packet_type must be in 0..7")
    if not 0 <= version <= 31:
        raise ValueError("version must be in 0..31")
    if len(payload) > 0xffff:
        raise ValueError("payload length must fit U16")
    header = MAGIC + bytes([(packet_type << 5) | version]) + len(payload).to_bytes(2, "little")
    crc = zlib.crc32(header + payload) & 0xffffffff
    return header + payload + crc.to_bytes(4, "little")


def reference_decode(pending, data, error_in=None):
    """Apply the agreed framing/error contract to one Decode Frames.vi call."""
    stream = pending + data
    if error_in is not None:
        return [], stream, error_in

    frames = []
    remaining = b""
    error_out = None
    cursor = 0

    while cursor < len(stream):
        start = stream.find(b"\x4c", cursor)
        if start < 0:
            break
        if start + 1 == len(stream):
            remaining = stream[start:]
            break
        if stream[start + 1] != 0x53:
            cursor = start + 1
            continue

        available = len(stream) - start
        if available < 5:
            remaining = stream[start:]
            break

        type_version = stream[start + 2]
        payload_length = int.from_bytes(stream[start + 3:start + 5], "little")
        if payload_length > MAX_PAYLOAD:
            cursor = start + 1
            continue

        frame_length = 5 + payload_length + 4
        if available < frame_length:
            remaining = stream[start:]
            break

        crc_end = start + 5 + payload_length
        received_crc = int.from_bytes(stream[crc_end:crc_end + 4], "little")
        calculated_crc = zlib.crc32(stream[start:crc_end]) & 0xffffffff
        if calculated_crc != received_crc:
            cursor = start + 1
            continue

        if (type_version & 0x1f) != 1:
            error_out = (ERROR_UNSUPPORTED_VERSION, "unsupported protocol version")
            break

        frames.append((type_version >> 5, stream[start + 5:crc_end]))
        cursor = start + frame_length

    return frames, remaining, error_out


def grouped_hex(data, width=16):
    if not data:
        return "(empty)"
    return "\n".join(
        " ".join(f"{byte:02X}" for byte in data[index:index + width])
        for index in range(0, len(data), width)
    )


def frame_summary(frames):
    if not frames:
        return "(none)"
    lines = []
    for index, (packet_type, payload) in enumerate(frames):
        summary = f"[{index}] packet_type={packet_type}, payload_length={len(payload)}"
        if len(payload) <= 24:
            summary += f", payload_hex={grouped_hex(payload, 24)}"
        else:
            summary += (
                f", first_8={grouped_hex(payload[:8], 8)}"
                f", last_8={grouped_hex(payload[-8:], 8)}"
            )
        lines.append(summary)
    return "\n".join(lines)


def add_case(cases, name, purpose, chunks, error_in=None):
    cases.append({
        "name": name,
        "purpose": purpose,
        "chunks": chunks,
        "error_in": error_in,
    })


def build_cases():
    cases = []
    empty = make_frame(0)
    text = make_frame(0, b"123456789")
    binary = make_frame(2, b"\x00\x4c\x53\xff\x00\x39")
    embedded_magic = make_frame(2, b"prefix\x4c\x53\x00\x00suffix")
    type_seven = make_frame(7, b"reserved-type-is-still-a-frame")
    bad_crc = bytearray(text)
    bad_crc[-1] ^= 0x01
    bad_crc = bytes(bad_crc)
    unsupported = make_frame(0, b"other-version", version=2)
    unsupported_bad_crc = bytearray(unsupported)
    unsupported_bad_crc[-1] ^= 0x01
    unsupported_bad_crc = bytes(unsupported_bad_crc)
    oversized_header = b"\x4c\x53\x01\x01\x08"  # length 0x0801 = 2049
    payload_256 = bytes(range(256))
    frame_256 = make_frame(2, payload_256)
    payload_max = bytes(range(256)) * 8
    frame_max = make_frame(2, payload_max)
    inner = make_frame(1, b"inside-bad-frame")
    bad_outer = bytearray(make_frame(0, b"AAA" + inner + b"ZZZ"))
    bad_outer[-1] ^= 0x01
    bad_outer = bytes(bad_outer)

    add_case(cases, "T01_empty_input", "Пустой вызов не выдаёт кадры или остаток.", [b""])
    add_case(cases, "T02_noise_only", "Мусор без сигнатуры отбрасывается.", [b"\x00\xff\x00\x7e\x10"])
    add_case(cases, "T03_trailing_L", "Одинокий первый байт сигнатуры сохраняется.", [b"\x00\xff\x4c"])
    add_case(cases, "T04_partial_signature", "Неполная сигнатура LS сохраняется целиком.", [b"\x00\x4c\x53"])
    add_case(cases, "T05_partial_header", "Неполный заголовок сохраняется от сигнатуры.", [b"\x4c\x53\x01\x09"])
    add_case(cases, "T06_valid_empty_payload", "Кадр типа 0 с пустым payload.", [empty])
    add_case(cases, "T07_valid_ascii_123456789", "Контрольный непустой кадр типа 0.", [text])
    add_case(cases, "T08_binary_payload", "Payload содержит NUL и произвольные бинарные байты.", [binary])
    add_case(cases, "T09_magic_inside_valid_payload", "Сигнатура внутри валидного payload не начинает новый кадр.", [embedded_magic])
    add_case(cases, "T10_reserved_type_7", "Тип 7 декодируется как поле кадра; версия остаётся 1.", [type_seven])
    add_case(cases, "T11_two_frames_back_to_back", "Два кадра без разделителя.", [empty + text])
    add_case(cases, "T12_noise_before_valid", "Мусор перед корректной сигнатурой пропускается.", [b"\x00\x99\x4c\x00\x7f" + text])
    add_case(cases, "T13_overlap_and_oversize_then_valid", "Ложный LS с длиной 2049 не блокирует следующий кадр.", [b"\x4c\x4c\x53\x01\x01\x08" + text])
    add_case(cases, "T14_bad_crc_only", "Кадр с неверной CRC отбрасывается без error cluster.", [bad_crc])
    add_case(cases, "T15_bad_crc_then_valid", "После неверной CRC декодер находит следующий кадр.", [bad_crc + text])
    add_case(cases, "T16_bad_outer_crc_contains_valid_frame", "При восстановлении после CRC ошибки находится вложенный валидный кадр.", [bad_outer])
    add_case(cases, "T17_valid_crc_unsupported_version", "CRC проверяется до версии; валидный кадр версии 2 даёт фатальную ошибку 5004.", [unsupported])
    add_case(cases, "T18_unsupported_version_bad_crc", "Неверная CRC не должна порождать 5004; следующий кадр декодируется.", [unsupported_bad_crc + text])
    add_case(cases, "T19_unsupported_stops_before_later_valid_frame", "После 5004 декодер останавливается; следующий корректный кадр не декодируется.", [unsupported + text])
    add_case(cases, "T20_valid_before_unsupported_stops_call", "Кадр до версии 2 может быть найден, затем 5004 останавливает разбор до следующего кадра.", [empty + unsupported + text])
    add_case(cases, "T21_partial_unsupported_version", "Неполный кадр версии 2 не выставляет 5004, пока не получен и проверен CRC.", [unsupported[:-1]])
    add_case(cases, "T22_length_256_little_endian", "Длина 256 кодируется байтами 00 01.", [frame_256])
    add_case(cases, "T23_max_payload_2048", "Граничный допустимый payload размером 2048 байт.", [frame_max])
    add_case(cases, "T24_oversize_2049_then_valid", "Длина 2049 — помеха; ошибка не выставляется, следующий кадр декодируется.", [oversized_header + text])
    add_case(cases, "T25_oversize_header_only", "Один oversized-заголовок не создаёт 5003/5004 и не остаётся pending.", [oversized_header])

    partial_payload = text[:5 + 4]
    add_case(cases, "T26_incomplete_payload", "Payload не пришёл целиком: весь кандидат остаётся в remaining bytes.", [partial_payload])
    add_case(cases, "T27_missing_crc", "Payload полный, CRC ещё не пришла.", [text[:-4]])
    add_case(cases, "T28_partial_crc_1_byte", "В буфере только один байт CRC.", [text[:-3]])
    add_case(cases, "T29_partial_crc_2_bytes", "В буфере только два байта CRC.", [text[:-2]])
    add_case(cases, "T30_partial_crc_3_bytes", "В буфере только три байта CRC.", [text[:-1]])

    for name, split in [
        ("T31_split_after_first_magic_byte", 1),
        ("T32_split_after_signature", 2),
        ("T33_split_inside_header", 4),
        ("T34_split_inside_payload", 10),
        ("T35_split_inside_crc", len(text) - 2),
    ]:
        add_case(
            cases,
            name,
            "Два последовательных вызова: первый возвращает хвост, второй завершает кадр.",
            [text[:split], text[split:]],
        )

    add_case(
        cases,
        "T36_valid_frame_then_partial_magic",
        "Полный кадр выдаётся, а завершающий 4C сохраняется до следующего чтения.",
        [text + b"\x4c"],
    )
    add_case(
        cases,
        "T37_existing_error_in",
        "Входная ошибка проходит без замены; байты сохраняются, декодирование не выполняется.",
        [text],
        error_in=(12345, "upstream test error"),
    )
    return cases


def render_case(case, index):
    lines = [f"### {case['name']}", "", case["purpose"], ""]
    pending = b""
    for call_index, chunk in enumerate(case["chunks"], 1):
        error_in = case["error_in"] if call_index == 1 else None
        frames, remaining, error_out = reference_decode(pending, chunk, error_in)
        lines.extend([
            f"**Вызов {call_index} — вход**",
            "",
            f"- `pending bytes`: `{grouped_hex(pending)}`",
        ])
        if error_in is None:
            lines.append("- `error in`: `status=FALSE`, `code=0`")
        else:
            lines.append(
                f"- `error in`: `status=TRUE`, `code={error_in[0]}`, "
                f"`source={error_in[1]}`"
            )
        lines.extend([
            "- `data in`:",
            "",
            "```text",
            grouped_hex(chunk),
            "```",
            "",
            f"**Ожидается после вызова {call_index}**",
            "",
            "- `decoded frames`:",
            "",
            "```text",
            frame_summary(frames),
            "```",
            f"- `remaining bytes`: `{grouped_hex(remaining)}`",
        ])
        if error_out is None:
            lines.append("- `error out`: `status=FALSE`, `code=0`")
        else:
            lines.append(
                f"- `error out`: `status=TRUE`, `code={error_out[0]}`, "
                f"`source={error_out[1]}`"
            )
        lines.append("")
        pending = remaining
    return "\n".join(lines)


def machine_case(case):
    calls = []
    pending = b""
    for call_index, chunk in enumerate(case["chunks"], 1):
        error_in = case["error_in"] if call_index == 1 else None
        frames, remaining, error_out = reference_decode(pending, chunk, error_in)
        calls.append({
            "pending_hex": pending.hex().upper(),
            "data_hex": chunk.hex().upper(),
            "error_in": None if error_in is None else {
                "status": True, "code": error_in[0], "source": error_in[1],
            },
            "expected": {
                "frames": [
                    {"packet_type": packet_type, "payload_hex": payload.hex().upper()}
                    for packet_type, payload in frames
                ],
                "remaining_hex": None if error_out else remaining.hex().upper(),
                "error": None if error_out is None else {
                    "status": True, "code": error_out[0], "source": error_out[1],
                },
            },
        })
        pending = remaining
    return {"name": case["name"], "purpose": case["purpose"], "calls": calls}


def machine_case(case):
    calls = []
    pending = b""
    for call_index, chunk in enumerate(case["chunks"], 1):
        error_in = case["error_in"] if call_index == 1 else None
        frames, remaining, error_out = reference_decode(pending, chunk, error_in)
        calls.append({
            "pending_hex": pending.hex().upper(),
            "data_hex": chunk.hex().upper(),
            "error_in": None if error_in is None else {
                "status": True, "code": error_in[0], "source": error_in[1],
            },
            "expected": {
                "frames": [
                    {"packet_type": packet_type, "payload_hex": payload.hex().upper()}
                    for packet_type, payload in frames
                ],
                "remaining_hex": None if error_out else remaining.hex().upper(),
                "error": None if error_out is None else {
                    "status": True, "code": error_out[0], "source": error_out[1],
                },
            },
        })
        pending = remaining
    return {"name": case["name"], "purpose": case["purpose"], "calls": calls}


def main():
    cases = build_cases()
    output = [
        "# Тест-векторы для `Decode Frames.vi`",
        "",
        "Сгенерировано `generate_decoder_vectors.py` (Python 3, только стандартная библиотека).",
        "Скрипт генерирует входные данные и ожидаемые результаты для запуска через `run_decoder_tests.py`.",
        "",
        "## Принятый контракт в этих векторах",
        "",
        "- CRC-32 — стандартный reflected CRC-32, trailer little-endian.",
        "- Неполный заголовок/кадр — штатный остаток, не ошибка.",
        "- Неверная CRC и payload length > 2048 — отбросить кандидата, продолжить поиск с `found index + 1`, не выставлять error cluster.",
        "- Версия проверяется только после полного кадра и совпадения CRC. Версия не 1 выдаёт фатальную ошибку декодера: `5004`, `unsupported protocol version`; разбор этого вызова останавливается на таком кадре.",
        "- При 5004 кадры после кандидата не декодируются. Выходные данные вызова с ошибкой не используются приложением.",
        "- При уже установленном `error in` этот error cluster проходит без замены, данные не декодируются и сохраняются в `remaining bytes` как `pending + data`.",
        "",
        "Для ручного прогона подавайте `data in` как бинарную строку (например, через `Byte Array To String`). Между вызовами передавайте `remaining bytes` предыдущего вызова во вход `pending bytes` следующего.",
        "",
        "## Векторы",
        "",
    ]
    for index, case in enumerate(cases, 1):
        output.append(render_case(case, index))
    target = Path(__file__).with_name("decoder-test-vectors.md")
    target.write_text("\n".join(output).rstrip("\n") + "\n", encoding="utf-8")
    json_target = Path(__file__).with_name("decoder-test-vectors.json")
    json_target.write_text(
        json.dumps({"cases": [machine_case(case) for case in cases]}, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {len(cases)} cases to {target}")
    print(f"Wrote machine-readable vectors to {json_target}")


if __name__ == "__main__":
    main()
