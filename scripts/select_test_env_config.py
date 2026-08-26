"""Pin the configuration file used by the QEMU test build environments.

The test env (lolin_d32_qemu) must build against a committed test config
(test/configs/openmeteo.yml) by default: test builds must never depend on
the untracked local config.yml. test/run_tests.sh overrides ESP32_EPD_CONFIG
per run for the other config variants: test/configs/owm.yml and
test/configs/noaa.yml.

For every other env the variable is left alone — except when it still holds
a config planted by this script for a test env (extra_scripts run in the
same process for every env of an invocation), in which case it is removed so
a regular build cannot accidentally pick up a test config.

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
import os

Import("env")  # noqa: F401 - provided by the PlatformIO build system

TEST_ENV_CONFIGS = {
    "lolin_d32_qemu": os.path.join("test", "configs", "openmeteo.yml"),
}

env_name = env.subst("$PIOENV")
test_config = TEST_ENV_CONFIGS.get(env_name)
if test_config is not None:
    if "ESP32_EPD_CONFIG" not in os.environ:
        os.environ["ESP32_EPD_CONFIG"] = test_config
elif os.environ.get("ESP32_EPD_CONFIG") in TEST_ENV_CONFIGS.values():
    os.environ.pop("ESP32_EPD_CONFIG", None)