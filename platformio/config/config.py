Import("env", "projenv")

try:
    import pydantic
except ImportError:
    env.Execute("$PYTHONEXE -m pip install pydantic")

try:
    import yaml
except ImportError:
    env.Execute("$PYTHONEXE -m pip install pyyaml")

from schema import ConfigSchema, defined_enums
from pydantic import BaseModel
from re import sub
from datetime import datetime
import os


def upper_snake(s: str):
    return "_".join(
        sub("([A-Z][a-z]+)", r" \1", sub("([A-Z]+)", r" \1", s)).split()
    ).upper()


def escape_c_string(value):
    """Escape a string for C++ string literal."""
    s = str(value)
    s = s.replace("\\", "\\\\")
    s = s.replace('"', '\\"')
    s = s.replace("\n", "\\n")
    s = s.replace("\r", "\\r")
    return s


def format_cpp_define(key, value):
    """Format a C++ #define macro."""
    if isinstance(value, str):
        escaped = escape_c_string(value)
        return f'#define D_{key} "{escaped}"'
    elif isinstance(value, bool):
        return f"#define {key} {1 if value else 0}"
    elif isinstance(value, int):
        return f"#define {key} {value}"
    elif isinstance(value, float):
        return f"#define {key} {value}f"
    elif value is None:
        return f'#define D_{key} ""'
    else:
        return f"#define {key} {value}"


def format_bssid(bssid_str):
    """Convert BSSID string to C++ uint8_t array format."""
    # Remove colon separators and convert to uppercase
    cleaned = bssid_str.replace(":", "").upper()

    # Split into pairs of hex digits
    hex_pairs = [cleaned[i : i + 2] for i in range(0, len(cleaned), 2)]

    # Format as C++ array initializer
    formatted = ", ".join([f"0x{pair}" for pair in hex_pairs])
    return f"{{{formatted}}}"


def process_config_item(k, v, prefix="", font_files=None, parent_obj=None):
    """Recursively process config items and return header lines."""
    lines = []
    key_upper = upper_snake(k)
    macro_key = f"{prefix}_{key_upper}" if prefix else key_upper

    # Handle leftPanelLayout specially at the top level
    if k == "leftPanelLayout" and prefix == "":
        for name, idx in v.items():
            lines.append(f"#define POS_{name.upper()} {idx}")
    else:
        func_name = f"{k.lower()}_to_config_value"

        # 1. Check if the parent object has a specific converter for this field
        # (e.g., Wifi.bssid_to_config_value handles the bssid field)
        if (
            parent_obj
            and hasattr(parent_obj, func_name)
            and callable(getattr(parent_obj, func_name))
        ):
            lines.append(f"#define {macro_key} {getattr(parent_obj, func_name)()}")

        # 2. Check if the value itself has a generic converter
        # (e.g., Color.to_config_value handles the Color enum)
        elif hasattr(v, "to_config_value") and callable(
            getattr(v, "to_config_value", None)
        ):
            lines.append(f"#define {macro_key} {v.to_config_value()}")

        elif hasattr(v, "name"):
            if k == "locale":
                lines.append(f"#define LOCALE {v.value}")
            elif k == "font" and font_files is not None:
                lines.append(f'#define FONT_HEADER "{font_files[v]}"')
            else:
                enum_value = v.name
                lines.append(f"#define {macro_key}_{enum_value}")
        elif isinstance(v, BaseModel):
            lines.append(f"// {k} configuration")
            for sub_k, sub_v in v.model_dump().items():
                if sub_v is not None:
                    lines.extend(
                        process_config_item(sub_k, sub_v, macro_key, font_files, v)
                    )
            lines.append("")
        elif isinstance(v, dict):
            lines.append(f"// {k} sub-configuration")
            for sub_k, sub_v in v.items():
                if sub_v is not None:
                    lines.extend(
                        process_config_item(sub_k, sub_v, macro_key, font_files, v)
                    )
        else:
            lines.append(format_cpp_define(macro_key, v))
    return lines


# Generate header file
header_lines = [
    "// Auto-generated configuration header",
    "// DO NOT EDIT - Generated from config.yml",
    "",
    "#pragma once",
    "",
]

# Add build version with current date/time
build_version = datetime.now().strftime("%Y.%m.%d %H:%M")
header_lines.append("// Build Information")
header_lines.append(f'#define D_BUILD_VERSION "{build_version}"')
header_lines.append("")

with open("./config.yml", "r", encoding="utf-8") as config_file:
    user_config = yaml.safe_load(config_file)
    config = ConfigSchema(**user_config)

    # Font name to header mappings
    font_files = {
        "FreeMono": "fonts/FreeMono.h",
        "FreeSans": "fonts/FreeSans.h",
        "FreeSerif": "fonts/FreeSerif.h",
        "Lato": "fonts/Lato_Regular.h",
        "Montserrat": "fonts/Montserrat_Regular.h",
        "Open Sans": "fonts/OpenSans_Regular.h",
        "Poppins": "fonts/Poppins_Regular.h",
        "Quicksand": "fonts/Quicksand_Regular.h",
        "Raleway": "fonts/Raleway_Regular.h",
        "Roboto": "fonts/Roboto_Regular.h",
        "Roboto Mono": "fonts/RobotoMono_Regular.h",
        "Roboto Slab": "fonts/RobotoSlab_Regular.h",
        "Ubuntu": "fonts/Ubuntu_R.h",
        "Ubuntu Mono": "fonts/UbuntuMono_R.h",
    }

    # Add configuration defines
    header_lines.append("// Configuration")
    for k, v in config:
        header_lines.extend(
            process_config_item(k, v, font_files=font_files, parent_obj=config)
        )

# Write header file to include directory
header_path = os.path.join("include", "defines.h")
os.makedirs(os.path.dirname(header_path), exist_ok=True)

with open(header_path, "w", encoding="utf-8") as header_file:
    header_file.write("\n".join(header_lines))

print(f"Generated configuration header: {header_path}")
print(f"Total defines: {len([l for l in header_lines if l.startswith('#define')])}")
