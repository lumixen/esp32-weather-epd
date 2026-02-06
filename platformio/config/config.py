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
        return f"#define {key} {'true' if value else 'false'}"
    elif isinstance(value, int):
        return f"#define {key} {value}"
    elif isinstance(value, float):
        return f"#define {key} {value}f"
    elif value is None:
        return f'#define D_{key} ""'
    else:
        return f"#define {key} {value}"


# Generate header file
header_lines = [
    "// Auto-generated configuration header",
    "// DO NOT EDIT - Generated from config.yml",
    "",
    "#pragma once",
    "",
]

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
        if hasattr(v, "name"):
            if k == "locale":
                # For locale take its value
                header_lines.append(f"#define LOCALE {v.value}")
            elif k == "font":
                # Font is taken from the font_files dictionary
                header_lines.append(f'#define FONT_HEADER "{font_files[v]}"')
            else:
                # For enums, use config key as prefix and enum value as suffix
                # Don't convert enum value since it's already in correct format
                config_key = upper_snake(k)
                enum_value = v.name  # Use as-is, already uppercase with underscores
                header_lines.append(f"#define {config_key}_{enum_value}")
        elif isinstance(v, BaseModel):
            # Convert Pydantic models to dict
            header_lines.append(f"// {k} configuration")
            for nested_k, nested_v in v.model_dump().items():
                config_key = upper_snake(k)
                nested_key = upper_snake(nested_k)
                if isinstance(nested_v, str):
                    # Check if nested value is an enum-like string
                    macro_key = f"{config_key}_{nested_key}"
                    header_lines.append(format_cpp_define(macro_key, nested_v))
                else:
                    macro_key = f"{config_key}_{nested_key}"
                    header_lines.append(format_cpp_define(macro_key, nested_v))
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
