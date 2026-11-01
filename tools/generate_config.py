"""Generate the ESP32 config table from the shared root config.json."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "config.json"
OUTPUT = ROOT / "esp" / "src" / "application" / "configuration" / "config_generated.h"
TYPES = {"int32": "Int32", "bool": "Bool", "string": "String", "shunt_res": "ShuntResistance"}


def main() -> None:
    schema = json.loads(SOURCE.read_text(encoding="utf-8"))
    if set(schema) != {"fields"} or not isinstance(schema["fields"], dict) or not schema["fields"]:
        raise ValueError("config.json must contain a nonempty 'fields' object")

    defaults = {}
    rows = []
    for name, definition in schema["fields"].items():
        if not isinstance(name, str) or not name or not name.isascii():
            raise ValueError(f"invalid field name: {name!r}")
        if not isinstance(definition, dict) or set(definition) != {"type", "default"}:
            raise ValueError(f"invalid definition for {name}")
        kind, default = definition["type"], definition["default"]
        if kind not in TYPES:
            raise ValueError(f"unsupported type for {name}: {kind!r}")
        if kind == "int32" and (type(default) is not int or not -(2**31) <= default < 2**31):
            raise ValueError(f"invalid int32 default for {name}")
        if kind == "bool" and type(default) is not bool:
            raise ValueError(f"invalid bool default for {name}")
        if kind == "string" and type(default) is not str:
            raise ValueError(f"invalid string default for {name}")
        if kind == "shunt_res" and type(default) is not int:
            raise ValueError(f"invalid shunt_res default for {name}")
        if kind == "shunt_res" and default != 0 and default not in {
            200, 1000, 10000, 100000, 1000000, 10000000
        }:
            raise ValueError(f"invalid shunt_res default for {name}")
        defaults[name] = default
        rows.append(f"    {{{json.dumps(name)}, FieldType::{TYPES[kind]}}},")

    default_json = json.dumps(defaults, ensure_ascii=True, separators=(",", ":"))
    content = (
        "// Generated from config.json by tools/generate_config.py. Do not edit.\n"
        "#pragma once\n\n#include <stddef.h>\n\n"
        "enum class FieldType { Int32, Bool, String, ShuntResistance };\n"
        "struct FieldDefinition { const char *name; FieldType type; };\n\n"
        "constexpr FieldDefinition kFields[] = {\n"
        + "\n".join(rows)
        + "\n};\n"
        "constexpr size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);\n"
        f"constexpr char kDefaultConfig[] = {json.dumps(default_json)};\n"
    )
    OUTPUT.write_text(content, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
