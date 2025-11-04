#include "moon_tools.h"

moonPhase moonPhase;

moon_state_t getMoonState(float latitude, float longitude)
{
    MoonRise mr;
    time_t now = time(nullptr);
    mr.calculate(latitude, longitude, now);
    time_t moonrise = mr.riseTime;
    // Calculate moonset time after the next moonrise.
    mr.calculate(latitude, longitude, moonrise + 1);
    time_t moonset_after_rise = mr.setTime;
#if DEBUG_LEVEL >= 1
    Serial.println("[debug] Moon rise azimuth: " + String(mr.riseAz) + " Moon set azimuth: " + String(mr.setAz));
    Serial.println("[debug] Moonrise: " + String(moonrise) + " Moonset: " + String(moonset_after_rise));
#endif
    moonData_t moon = moonPhase.getPhase(now);
#if DEBUG_LEVEL >= 1
    Serial.println("[debug] Moon phase percent lit: " + String(moon.percentLit));
#endif
    return moon_state_t{moonrise, moonset_after_rise, moon.percentLit};
}
