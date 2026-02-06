#include "defines.h"
#include "moon_tools.h"

MoonPhase moonPhase;

moon_state_t getMoonState(float latitude, float longitude)
{
    MoonRise mr;
    time_t now = time(nullptr);
    mr.calculate(latitude, longitude, now);
    time_t moonrise = mr.riseTime;
    time_t moonset = mr.setTime;
#if DEBUG_LEVEL >= 1
    Serial.println("[debug] Moon rise azimuth: " + String(mr.riseAz) + " Moon set azimuth: " + String(mr.setAz));
    Serial.println("[debug] Moonrise: " + String(moonrise) + " Moonset: " + String(moonset));
#endif
    moonData_t moon = moonPhase.getPhase(now);
    // Convert angle (0-360) to phase cycle (0.0-1.0)
    // 0 deg = 0.0 (New)
    // 90 deg = 0.25 (First Quarter)
    // 180 deg = 0.5 (Full)
    // 270 deg = 0.75 (Third Quarter)
    float moonPhase = moon.angleDeg / 360.0f;
#if DEBUG_LEVEL >= 1
    Serial.println("[debug] Moon phase: " + String(moonPhase, 4));
#endif
    return moon_state_t{moonrise, moonset, moonPhase};
}
