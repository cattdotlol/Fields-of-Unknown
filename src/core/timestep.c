#include "core/timestep.h"

#include <math.h>

int TimeStepAdvance(TimeStep *clock, double elapsed)
{
    if (!isfinite(elapsed) || elapsed < 0.0) elapsed = 0.0;
    if (elapsed > SIM_MAX_FRAME)
    {
        clock->dropped += elapsed - SIM_MAX_FRAME;
        elapsed = SIM_MAX_FRAME;
    }

    clock->remainder += elapsed;
    int available = (int)(clock->remainder / SIM_TICK_DT);
    clock->remainder -= (double)available * SIM_TICK_DT;
    int ticks = available;
    if (ticks > SIM_MAX_CATCHUP)
    {
        clock->dropped += (double)(ticks - SIM_MAX_CATCHUP) * SIM_TICK_DT;
        ticks = SIM_MAX_CATCHUP;
    }
    clock->alpha = (float)(clock->remainder / SIM_TICK_DT);
    return ticks;
}
