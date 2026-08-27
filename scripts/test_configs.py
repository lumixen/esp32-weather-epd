"""Load and validate the registry of committed QEMU test configurations.

Copyright (C) 2026  Max Bodaniuk

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
from typing import Any

import yaml


REGISTRY_PATH = Path("test") / "configs" / "index.yml"
_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]*$")
_SUITE_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]*$")


class TestConfigError(ValueError):
    """Raised when the test configuration registry is invalid."""


@dataclass(frozen=True)
class TestConfig:
    id: str
    file: str
    suite: str
    root: Path

    @property
    def config_path(self) -> str:
        """Return the config path in the form expected by ESP32_EPD_CONFIG."""
        return (Path("test") / "configs" / self.file).as_posix()

    @property
    def absolute_config_path(self) -> Path:
        return self.root / self.config_path

    @property
    def suite_path(self) -> Path:
        return self.root / "test" / "src" / self.suite


@dataclass(frozen=True)
class TestConfigRegistry:
    default: str
    configs: tuple[TestConfig, ...]

    @property
    def by_id(self) -> dict[str, TestConfig]:
        return {config.id: config for config in self.configs}

    @property
    def default_config(self) -> TestConfig:
        return self.by_id[self.default]


def _repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _safe_relative_path(value: Any, base: Path, root: Path, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise TestConfigError(f"{field} must be a non-empty relative path")
    path = Path(value)
    if path.is_absolute():
        raise TestConfigError(f"{field} must be relative: {value!r}")
    resolved = (base / path).resolve()
    try:
        resolved.relative_to(root.resolve())
    except ValueError as exc:
        raise TestConfigError(f"{field} must stay inside the repository: {value!r}") from exc
    return path.as_posix()


def load_registry(root: Path | None = None) -> TestConfigRegistry:
    """Load and validate test/configs/index.yml."""
    root = (root or _repository_root()).resolve()
    registry_path = root / REGISTRY_PATH
    try:
        with registry_path.open("r", encoding="utf-8") as registry_file:
            data = yaml.safe_load(registry_file)
    except FileNotFoundError as exc:
        raise TestConfigError(f"test configuration registry not found: {registry_path}") from exc
    except yaml.YAMLError as exc:
        raise TestConfigError(f"invalid YAML in {registry_path}: {exc}") from exc

    if not isinstance(data, dict):
        raise TestConfigError("test configuration registry must be a YAML mapping")

    default = data.get("default")
    if not isinstance(default, str) or not default:
        raise TestConfigError("registry default must be a non-empty configuration ID")

    entries = data.get("configs")
    if not isinstance(entries, list) or not entries:
        raise TestConfigError("registry configs must be a non-empty list")

    configs: list[TestConfig] = []
    seen_ids: set[str] = set()
    config_dir = registry_path.parent
    for index, entry in enumerate(entries):
        prefix = f"configs[{index}]"
        if not isinstance(entry, dict):
            raise TestConfigError(f"{prefix} must be a mapping")

        config_id = entry.get("id")
        if not isinstance(config_id, str) or not _ID_RE.fullmatch(config_id):
            raise TestConfigError(
                f"{prefix}.id must match {_ID_RE.pattern!r}: {config_id!r}"
            )
        if config_id in seen_ids:
            raise TestConfigError(f"duplicate test configuration ID: {config_id}")
        seen_ids.add(config_id)

        config_file = _safe_relative_path(entry.get("file"), config_dir, root, f"{prefix}.file")
        suite = entry.get("suite")
        if not isinstance(suite, str) or not _SUITE_RE.fullmatch(suite):
            raise TestConfigError(
                f"{prefix}.suite must be a simple PlatformIO suite name: {suite!r}"
            )

        config = TestConfig(config_id, config_file, suite, root)
        if not config.absolute_config_path.is_file():
            raise TestConfigError(
                f"{prefix}.file does not exist: {config.absolute_config_path}"
            )
        if not config.suite_path.is_dir():
            raise TestConfigError(f"{prefix}.suite directory does not exist: {config.suite_path}")
        if not any(config.suite_path.glob("*.cpp")):
            raise TestConfigError(f"{prefix}.suite has no C++ test driver: {config.suite_path}")
        configs.append(config)

    if default not in seen_ids:
        valid = ", ".join(seen_ids)
        raise TestConfigError(
            f"registry default {default!r} is not registered (valid IDs: {valid})"
        )

    return TestConfigRegistry(default, tuple(configs))


def _main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    output = parser.add_mutually_exclusive_group(required=True)
    output.add_argument("--list", action="store_true", help="print IDs, one per line")
    output.add_argument("--json", action="store_true", help="print IDs as a JSON array")
    output.add_argument(
        "--shell",
        action="store_true",
        help="print tab-separated ID, config path, and suite for shell callers",
    )
    args = parser.parse_args()

    try:
        registry = load_registry()
    except TestConfigError as exc:
        parser.error(str(exc))

    if args.list:
        print("\n".join(config.id for config in registry.configs))
    elif args.json:
        print(json.dumps([config.id for config in registry.configs], separators=(",", ":")))
    else:
        for config in registry.configs:
            print(f"{config.id}\t{config.config_path}\t{config.suite}")
    return 0


if __name__ == "__main__":
    raise SystemExit(_main())
