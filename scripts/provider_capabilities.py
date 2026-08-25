# Build-time ownership declarations for configured providers.
# Copyright (C) 2026  Max Bodaniuk
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

CAPABILITIES = {
    "open_meteo_forecast": {"current_forecast", "hourly_forecast", "daily_forecast"},
    "open_meteo_air_quality": {"air_quality"},
    "openweathermap_onecall_v3": {"current_forecast", "hourly_forecast", "daily_forecast", "alerts"},
    "openweathermap_air_quality": {"air_quality"},
    "meteoalarm_alert": {"alerts"},
    "bme280": {"in_temperature", "in_humidity", "in_pressure"},
}

REQUIRED_FORECAST = {"current_forecast", "hourly_forecast", "daily_forecast"}


def validate_capabilities(config):
    """Reject missing required groups and duplicate ownership.

    The list order is intentionally irrelevant. Each configured entry is one
    logical provider instance and must exclusively own its effective tags.
    """
    owners = {}
    errors = []
    for index, provider in enumerate(config.providers):
        provider_id = provider.provider
        tags = CAPABILITIES.get(provider_id)
        if tags is None:
            errors.append(f"unknown provider '{provider_id}'")
            continue
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
    if errors:
        raise ValueError("; ".join(errors))
    return owners
