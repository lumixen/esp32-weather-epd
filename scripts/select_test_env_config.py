"""Pin the configuration file used by the QEMU test build environments.

The test env (lolin_d32_qemu) must build against a committed test config
(test/configs/openmeteo.yml) by default: test builds must never depend on
the untracked local config.yml. test/run_tests.sh overrides ESP32_EPD_CONFIG
per run for the other config variants (test/configs/owm.yml).

For every other env the variable is left alone — except when it still holds
a config planted by this script for a test env (extra_scripts run in the
same process for every env of an invocation), in which case it is removed so
a regular build cannot accidentally pick up a test config.

Run before scripts/config.py in the same process, hence the os.environ
hand-off.
"""

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