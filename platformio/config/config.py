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

def process_nested_config(data, prefix=""):
    """Recursively process nested configuration dictionaries."""
    lines = []
    
    for key, value in data.items():
        # Skip None values
        if value is None:
            continue
        
        # Build the macro key
        key_upper = upper_snake(key)
        macro_key = f"{prefix}_{key_upper}" if prefix else key_upper
        
        # Handle nested dictionaries (nested BaseModel objects)
        if isinstance(value, dict):
            lines.append(f"// {key} sub-configuration")
            lines.extend(process_nested_config(value, macro_key))
        # Special handling for BSSID
        elif key == "bssid":
            bssid_array = format_bssid(value)
            lines.append(f"#define {macro_key} {bssid_array}")
        # Handle all other values
        else:
            lines.append(format_cpp_define(macro_key, value))
    
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
        if k == "leftPanelLayout":
            for name, idx in v.items():
                header_lines.append(f"#define POS_{name.upper()} {idx}")
        elif hasattr(v, "name"):
            if k == "locale":
                # For locale take its value
                header_lines.append(f"#define LOCALE {v.value}")
            elif k == "font":
                # Font is taken from the font_files dictionary
                header_lines.append(f'#define FONT_HEADER "{font_files[v]}"')
            else:
                # For enums, use config key as prefix and enum value as suffix
                config_key = upper_snake(k)
                enum_value = v.name
                header_lines.append(f"#define {config_key}_{enum_value}")
        elif isinstance(v, BaseModel):
            # Convert Pydantic models to dict and process recursively
            header_lines.append(f"// {k} configuration")
            config_key = upper_snake(k)
            nested_lines = process_nested_config(v.model_dump(), config_key)
            header_lines.extend(nested_lines)
            header_lines.append("")
        else:
            header_lines.append(format_cpp_define(upper_snake(k), v))

# Write header file to include directory
header_path = os.path.join("include", "defines.h")
os.makedirs(os.path.dirname(header_path), exist_ok=True)

with open(header_path, "w", encoding="utf-8") as header_file:
    header_file.write("\n".join(header_lines))

print(f"Generated configuration header: {header_path}")
print(f"Total defines: {len([l for l in header_lines if l.startswith('#define')])}")
