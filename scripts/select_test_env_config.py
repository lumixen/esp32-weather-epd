"""Pin the configuration file used by the QEMU test build environments.

The QEMU test environment must build against a committed test config by
default: test builds must never depend on the untracked local config.yml.
The registry in test/configs/index.yml provides the default and the list of
paths that this script may inject or later remove.

For every other env the variable is left alone — except when it still holds a
config planted by this script for a test env (extra_scripts run in the same
process for every env of an invocation), in which case it is removed so a
regular build cannot accidentally pick up a test config.

Run before scripts/config.py in the same process, hence the os.environ
hand-off.
"""

# Select the committed test configuration for PlatformIO.
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

from pathlib import Path
import os
import sys

Import("env")  # noqa: F401 - provided by the PlatformIO build system

try:
    import yaml
except ImportError:
    yaml = None
if yaml is None:
    env.Execute("$PYTHONEXE -m pip install pyyaml")
    import yaml

project_root = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(project_root / "scripts"))
from test_configs import TestConfigError, load_registry  # noqa: E402


try:
    registry = load_registry(project_root)
except TestConfigError as exc:
    raise SystemExit(f"Invalid QEMU test configuration registry: {exc}") from exc

TEST_ENV_NAMES = {"lolin_d32_qemu"}
TEST_CONFIG_PATHS = {config.absolute_config_path.resolve() for config in registry.configs}

env_name = env.subst("$PIOENV")
if env_name in TEST_ENV_NAMES:
    if "ESP32_EPD_CONFIG" not in os.environ:
        os.environ["ESP32_EPD_CONFIG"] = registry.default_config.config_path
elif "ESP32_EPD_CONFIG" in os.environ:
    configured_path = Path(os.environ["ESP32_EPD_CONFIG"])
    if not configured_path.is_absolute():
        configured_path = project_root / configured_path
    if configured_path.resolve() in TEST_CONFIG_PATHS:
        os.environ.pop("ESP32_EPD_CONFIG", None)
