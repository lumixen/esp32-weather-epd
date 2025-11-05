#include "moon_tools.h"

moonPhase moonPhase;

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
#if DEBUG_LEVEL >= 1
    Serial.println("[debug] Moon phase percent lit: " + String(moon.percentLit));
#endif
    return moon_state_t{moonrise, moonset, moon.percentLit};
}
