#include <time.h>
#include <MoonRise.h>
#include <moonPhase.h>

#ifndef MOON_TOOLS_H
#define MOON_TOOLS_H

typedef struct moon_state
{
    time_t moonrise;
    time_t moonset;
    double phase; // 0.0 - 1.0
} moon_state_t;

moon_state_t getMoonState(float latitude, float longitude);

#endif // MOON_TOOLS_H
