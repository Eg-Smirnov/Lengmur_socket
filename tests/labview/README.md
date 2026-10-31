# LabVIEW decoder tests

These vectors exercise `Decode Frames.vi` through the hex-safe
`laptop/labview/tests/Diagnostics.vi` adapter and LabVIEW's local VI Server
ActiveX interface. The runner sends ASCII hex strings, starts the adapter
asynchronously with a timeout, then compares decoded frames, remaining bytes,
and error cluster values. The adapter converts the hex text to binary strings
inside LabVIEW, avoiding ActiveX corruption of bytes above `0x7F`.

## Dependency

- Python 3.8 or newer, 64-bit.
- LabVIEW 2015, 64-bit, installed on the same Windows computer.
- Python package `pywin32` (listed in `requirements.txt`).

Install the package from the repository root:

```powershell
python -m pip install -r tests/labview/requirements.txt
```

## Run

Generate the Markdown and machine-readable vectors, then run them against the
VI:

```powershell
python tests/labview/generate_decoder_vectors.py
python tests/labview/run_decoder_tests.py
```

The runner uses `laptop/labview/tests/Diagnostics.vi` and assumes these
connector-pane terminal names: `pending bytes`, `data in`, `error in (no
error)`, `decoded_frames`, `remaining bytes`, and `error out`. The two input
strings are ASCII hex; `decoded_frames` payloads and `remaining bytes` must also
be ASCII hex. Override any name with its matching
`--pending-bytes`, `--data-in`, `--error-in`, `--decoded-frames`,
`--remaining-bytes`, or `--error-out` option if the labels differ.

The runner connects directly to `Diagnostics.vi` using
`LabVIEW.Application.GetVIReference`; it does not try to open the project.
`Diagnostics.vi` calls `Decode Frames.vi` and converts the ASCII hex inputs and
outputs around the binary strings.

Before the vectors run, the script sends an incomplete candidate containing
`C6 FF` through the adapter and checks that the same bytes return in hex. This
confirms the input and output hex conversions preserve high-bit bytes.

The runner attaches to LabVIEW through `LabVIEW.Application`; it does not close
the LabVIEW application when it finishes. For any error, it checks the error
cluster and ignores `remaining bytes`. For fatal error 5004 it also inspects
`decoded_frames` to ensure no frame after the unsupported-version frame was
appended. The application must ignore all data outputs from calls whose error
cluster reports an error.

The adapter's connector pane must keep these six terminals connected. The
runner uses the connector-pane labels as parameter names; note that
`decoded_frames` contains an underscore.

Expected output is one `PASS` line per VI call and a final pass/fail count. The
process exits with code 1 if the VI cannot be called or any comparison fails.
