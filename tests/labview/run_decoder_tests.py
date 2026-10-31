#!/usr/bin/env python3
"""Run the decoder test vectors against Decode Frames.vi through LabVIEW VI Server."""

import argparse
import json
import sys
import time
from pathlib import Path


def require_pywin32():
    try:
        import pythoncom
        import win32com.client
    except ImportError as exc:
        raise SystemExit(
            "Missing pywin32. Install it with: "
            "python -m pip install -r tests/labview/requirements.txt"
        ) from exc
    return pythoncom, win32com.client


def bytes_to_lv_string(hex_value):
    return bytes.fromhex(hex_value).decode("latin-1")


def lv_string_to_hex(value):
    if isinstance(value, bytes):
        return value.hex().upper()
    if not isinstance(value, str):
        raise TypeError(f"Expected LabVIEW string, got {type(value).__name__}")
    try:
        return value.encode("latin-1").hex().upper()
    except UnicodeEncodeError as exc:
        raise ValueError("LabVIEW string returned characters outside byte range") from exc


def parse_hex_text(value, label):
    if not isinstance(value, str):
        raise TypeError(f"Expected ASCII hex string for {label}, got {type(value).__name__}")
    normalized = "".join(value.split()).upper()
    try:
        bytes.fromhex(normalized)
    except ValueError as exc:
        raise ValueError(f"{label} is not valid ASCII hex: {value!r}") from exc
    return normalized


def as_sequence(value):
    if isinstance(value, (tuple, list)):
        return list(value)
    try:
        return list(value)
    except TypeError:
        return [value]


def parse_error_cluster(value):
    values = as_sequence(value)
    if len(values) != 3:
        raise ValueError(f"Expected error cluster as 3-field array, got {values!r}")
    return {"status": bool(values[0]), "code": int(values[1]), "source": str(values[2])}


def parse_frames(value):
    outer = as_sequence(value)
    frames = []
    for frame in outer:
        fields = as_sequence(frame)
        if len(fields) != 2:
            raise ValueError(
                "Expected each Decoded Frame cluster as [packet type, payload]; "
                f"got {fields!r}"
            )
        frames.append({
            "packet_type": int(fields[0]),
            "payload_hex": parse_hex_text(fields[1], "decoded frame payload"),
        })
    return frames


def compare(name, call_index, label, expected, actual):
    if expected != actual:
        raise AssertionError(
            f"{name} call {call_index}: {label} mismatch\n"
            f"  expected: {expected!r}\n"
            f"  actual:   {actual!r}"
        )


def run_vi(vi, timeout_seconds):
    """Start asynchronously so a broken VI cannot block this test process forever."""
    vi.Run(True)
    started = time.monotonic()
    saw_running = False
    while time.monotonic() - started < timeout_seconds:
        state = int(vi.ExecState)
        if state in (2, 3):
            saw_running = True
        elif state == 0:
            raise RuntimeError("Decode Frames.vi is broken and cannot execute")
        elif state == 1 and (saw_running or time.monotonic() - started >= 0.1):
            return
        time.sleep(0.02)
    try:
        vi.Abort()
    except Exception:
        pass
    raise TimeoutError(f"Decode Frames.vi did not finish within {timeout_seconds:g} seconds")


def run(args):
    pythoncom, win32_client = require_pywin32()
    vectors_path = Path(args.vectors).resolve()
    vi_path = Path(args.vi).resolve()
    vectors = json.loads(vectors_path.read_text(encoding="utf-8"))["cases"]

    pythoncom.CoInitialize()
    app = None
    vi = None
    passed = 0
    failed = 0
    try:
        app = win32_client.Dispatch("LabVIEW.Application")
        vi = app.GetVIReference(str(vi_path))
        vi._FlagAsMethod("Run")
        vi._FlagAsMethod("Abort")
        vi._FlagAsMethod("GetControlValue")
        vi._FlagAsMethod("SetControlValue")
        exec_state = int(vi.ExecState)
        if exec_state == 0:
            raise RuntimeError(
                "The test adapter VI is not executable in the VI Server application "
                "instance. Open the project in that LabVIEW instance, fix any "
                "broken dependencies, and rerun the tester."
            )
        print(f"Connected to {vi_path}; ExecState={exec_state}", flush=True)

        # VI Server exposes only connector-pane terminals by name.
        for control in (
            args.pending_bytes, args.data_in, args.error_in, args.decoded_frames,
            args.remaining_bytes, args.error_out,
        ):
            try:
                vi.GetControlValue(control)
            except Exception as exc:
                raise RuntimeError(
                    f"VI connector pane has no parameter named {control!r}; "
                    "check the terminal label and assign the indicator to a connector terminal"
                ) from exc

        # The adapter accepts ASCII hex, decodes it to bytes, and returns
        # remaining bytes as ASCII hex. Include high-bit bytes in this probe.
        binary_probe = "4C53C6FF"
        vi.SetControlValue(args.pending_bytes, "")
        vi.SetControlValue(args.data_in, binary_probe)
        vi.SetControlValue(args.error_in, (False, 0, ""))
        run_vi(vi, args.call_timeout)
        probe_error = parse_error_cluster(vi.GetControlValue(args.error_out))
        probe_remaining = parse_hex_text(
            vi.GetControlValue(args.remaining_bytes), "round-trip remaining bytes"
        )
        if probe_error["status"] or probe_remaining != binary_probe:
            raise RuntimeError(
                "The hex adapter round-trip failed: "
                f"sent {binary_probe}, got remaining {probe_remaining}, "
                f"error={probe_error!r}."
            )

        for case in vectors:
            for call_index, call in enumerate(case["calls"], 1):
                try:
                    error_in = call["error_in"] or {"status": False, "code": 0, "source": ""}
                    vi.SetControlValue(args.pending_bytes, call["pending_hex"])
                    vi.SetControlValue(args.data_in, call["data_hex"])
                    vi.SetControlValue(args.error_in, (
                        bool(error_in["status"]), int(error_in["code"]), str(error_in["source"]),
                    ))
                    print(f"RUN {case['name']} call {call_index}", flush=True)
                    run_vi(vi, args.call_timeout)

                    expected = call["expected"]
                    actual_error = parse_error_cluster(vi.GetControlValue(args.error_out))
                    expected_error = expected["error"] or {"status": False, "code": 0, "source": ""}
                    compare(case["name"], call_index, "error.status", expected_error["status"], actual_error["status"])
                    compare(case["name"], call_index, "error.code", expected_error["code"], actual_error["code"])
                    if expected["error"] and expected_error["code"] != 5004:
                        compare(case["name"], call_index, "error.source", expected_error["source"], actual_error["source"])

                    # For fatal 5004, inspect only frames decoded before the
                    # unsupported-version frame. For any other incoming or
                    # returned error, data outputs are unspecified and ignored.
                    if actual_error["status"]:
                        if actual_error["code"] == 5004:
                            actual_frames = parse_frames(vi.GetControlValue(args.decoded_frames))
                            compare(case["name"], call_index, "decoded frames before fatal stop", expected["frames"], actual_frames)
                            print(f"PASS {case['name']} call {call_index} (fatal 5004)")
                        else:
                            print(f"PASS {case['name']} call {call_index} (error {actual_error['code']}; data outputs ignored)")
                        passed += 1
                        continue

                    actual_frames = parse_frames(vi.GetControlValue(args.decoded_frames))
                    compare(case["name"], call_index, "decoded frames", expected["frames"], actual_frames)
                    if expected["remaining_hex"] is not None:
                        actual_remaining = parse_hex_text(
                            vi.GetControlValue(args.remaining_bytes), "remaining bytes"
                        )
                        compare(case["name"], call_index, "remaining bytes", expected["remaining_hex"], actual_remaining)
                    print(f"PASS {case['name']} call {call_index}")
                    passed += 1
                except Exception as exc:
                    failed += 1
                    print(f"FAIL {case['name']} call {call_index}: {exc}", file=sys.stderr, flush=True)
    except Exception as exc:
        failed += 1
        print(f"FAIL: {exc}", file=sys.stderr)
    finally:
        # Do not close the LabVIEW application: the user may already have it open.
        vi = None
        app = None
        pythoncom.CoUninitialize()

    print(f"Result: {passed} passed, {failed} failed")
    return 1 if failed else 0


def main():
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vi", default=str(root.parent.parent / "laptop/labview/tests/Diagnostics.vi"))
    parser.add_argument("--vectors", default=str(root / "decoder-test-vectors.json"))
    parser.add_argument("--pending-bytes", default="pending bytes")
    parser.add_argument("--data-in", default="data in")
    parser.add_argument("--error-in", default="error in (no error)")
    parser.add_argument("--decoded-frames", default="decoded_frames")
    parser.add_argument("--remaining-bytes", default="remaining bytes")
    parser.add_argument("--error-out", default="error out")
    parser.add_argument("--call-timeout", type=float, default=15.0,
                        help="maximum seconds allowed for one VI call")
    raise SystemExit(run(parser.parse_args()))


if __name__ == "__main__":
    main()
