# Build-time ownership declarations for configured providers.
# Copyright (C) 2026  Max Bodaniuk
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

from dataclasses import dataclass, field
from typing import Any, Callable, Mapping


@dataclass(frozen=True)
class CapabilitySpec:
    """Static and configuration-dependent capabilities for one provider."""

    always: frozenset[str]
    enabled_by: Mapping[str, str] = field(default_factory=dict)
    resolver: Callable[[Any], set[str]] | None = None

    def resolve(self, provider: Any) -> set[str]:
        capabilities = set(self.always)
        for capability, config_field in self.enabled_by.items():
            if getattr(provider, config_field, False):
                capabilities.add(capability)
        if self.resolver is not None:
            capabilities.update(self.resolver(provider))
        return capabilities


CAPABILITIES: dict[str, CapabilitySpec] = {
    "open_meteo_forecast": CapabilitySpec(
        always=frozenset({"current_forecast", "hourly_forecast", "daily_forecast"}),
    ),
    "noaa_forecast": CapabilitySpec(
        always=frozenset({"current_forecast", "hourly_forecast", "daily_forecast"}),
    ),
    "meteoswiss_forecast": CapabilitySpec(
        always=frozenset({"current_forecast", "hourly_forecast", "daily_forecast"}),
    ),
    "open_meteo_air_quality": CapabilitySpec(
        always=frozenset({"air_quality"}),
    ),
    "openweathermap_onecall_v3": CapabilitySpec(
        always=frozenset({"current_forecast", "hourly_forecast", "daily_forecast", "alerts"}),
    ),
    "openweathermap_onecall_v4": CapabilitySpec(
        always=frozenset({"current_forecast", "hourly_forecast", "daily_forecast"}),
        enabled_by={"alerts": "alerts"},
    ),
    "openweathermap_air_quality": CapabilitySpec(
        always=frozenset({"air_quality"}),
    ),
    "meteoalarm_alert": CapabilitySpec(
        always=frozenset({"alerts"}),
    ),
    "bme280": CapabilitySpec(
        always=frozenset({"in_temperature", "in_humidity", "in_pressure"}),
    ),
}

REQUIRED_FORECAST = {"current_forecast", "hourly_forecast", "daily_forecast"}


def effective_capabilities(provider: Any) -> set[str]:
    spec = CAPABILITIES.get(provider.provider)
    return spec.resolve(provider) if spec is not None else set()


# Precipitation display settings require a corresponding value in the
# forecast response. Probability is represented by the provider-independent
# `pop` model field; amount is represented by rain/snow model fields.
PRECIPITATION_SUPPORT = {
    "open_meteo_forecast": {
        "hourly": {"probability", "amount"},
        "daily": {"probability", "amount"},
    },
    "noaa_forecast": {
        "hourly": {"probability"},
        "daily": {"probability"},
    },
    "meteoswiss_forecast": {
        "hourly": {"probability", "amount"},
        "daily": {"amount"},
    },
    "openweathermap_onecall_v3": {
        "hourly": {"probability", "amount"},
        "daily": {"probability", "amount"},
    },
    "openweathermap_onecall_v4": {
        "hourly": {"probability", "amount"},
        "daily": {"probability", "amount"},
    },
}


def precipitation_kind(unit):
    return "probability" if unit.value == "probability of precipitation" else "amount"


def validate_precipitation_support(config, errors):
    forecast_providers = [
        provider.provider
        for provider in config.providers
        if "current_forecast" in effective_capabilities(provider)
    ]
    if len(forecast_providers) != 1:
        return

    support = PRECIPITATION_SUPPORT.get(forecast_providers[0], {})
    requested = {
        "hourly": precipitation_kind(config.unitsHourlyPrecip),
        "daily": precipitation_kind(config.unitsDailyPrecip),
    }
    for period, kind in requested.items():
        if kind not in support.get(period, set()):
            errors.append(f"{period} precipitation {kind} is not supported by the selected forecast provider")


def validate_capabilities(config):
    """Reject missing required groups and duplicate ownership.

    The list order is intentionally irrelevant. Each configured entry is one
    logical provider instance and must exclusively own its effective tags.
    """
    owners = {}
    errors = []
    for index, provider in enumerate(config.providers):
        provider_id = provider.provider
        if provider_id not in CAPABILITIES:
            errors.append(f"unknown provider '{provider_id}'")
            continue
        tags = effective_capabilities(provider)
        owner = f"{provider_id} (providers[{index}])"
        for tag in tags:
            previous = owners.get(tag)
            if previous is not None and previous != owner:
                errors.append(f"duplicate ownership of '{tag}': {previous} and {owner}")
            else:
                owners[tag] = owner

    required = set(REQUIRED_FORECAST)
    if "AIR_QUALITY" in config.leftPanelLayout:
        required.add("air_quality")
    missing = sorted(required - owners.keys())
    if missing:
        errors.append("missing required provider data capability/capabilities: " + ", ".join(missing))
    validate_precipitation_support(config, errors)
    if errors:
        raise ValueError("; ".join(errors))
    return owners
