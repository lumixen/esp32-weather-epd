# PlatformIO sdkconfig environment setup helper.
# Copyright (C) 2026  Max Bodaniuk
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import os
from pathlib import Path
Import("env")

# Access the PlatformIO config object
config = env.GetProjectConfig()
pioenv = env.subst("$PIOENV")
project_dir = Path(env.subst("$PROJECT_DIR"))
build_dir = Path(env.subst("$BUILD_DIR"))
sdkconfig_path = project_dir / f"sdkconfig.{pioenv}"

# Read base defaults
base_defaults = project_dir / "sdkconfig.defaults"
base_content = base_defaults.read_text(encoding="utf-8") if base_defaults.exists() else ""

# Read custom_sdkconfig if present in platformio.ini
raw_custom = ""
if config.has_option(f"env:{pioenv}", "custom_sdkconfig"):
    raw_custom = config.get(f"env:{pioenv}", "custom_sdkconfig")
    # Remove custom_sdkconfig option from config so pioarduino's arduino.py
    # does not erroneously append duplicate linker flags (-T esp32.rom.libc-funcs.ld)
    config.remove_option(f"env:{pioenv}", "custom_sdkconfig")

if raw_custom.strip() or base_content.strip():
    build_dir.mkdir(parents=True, exist_ok=True)
    combined_file = build_dir / "sdkconfig.defaults.combined"
    
    combined_content = (
        "# Auto-generated merged sdkconfig defaults\n"
        + base_content
        + "\n# Environment custom_sdkconfig overrides\n"
        + raw_custom.strip()
        + "\n"
    )
    
    # Check if content changed
    content_changed = (
        not combined_file.exists()
        or combined_file.read_text(encoding="utf-8") != combined_content
    )
    
    if content_changed:
        combined_file.write_text(combined_content, encoding="utf-8")
        if sdkconfig_path.exists():
            sdkconfig_path.unlink()
            
    board = env.BoardConfig()
    existing_args = board.get("build.cmake_extra_args", "")
    
    if "SDKCONFIG_DEFAULTS" not in existing_args:
        new_arg = f"-DSDKCONFIG_DEFAULTS={combined_file.resolve().as_posix()}"
        updated_args = f"{existing_args} {new_arg}".strip()
        board.update("build.cmake_extra_args", updated_args)
