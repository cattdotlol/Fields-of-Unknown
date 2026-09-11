#include "tests.h"
#include "core/timestep.h"

#include <math.h>

void SuiteTimeStep(void)
{
    TimeStep clock = {0};
    Check("half a tick waits", TimeStepAdvance(&clock, SIM_TICK_DT * 0.5) == 0, true);
    Check("the second half advances once", TimeStepAdvance(&clock, SIM_TICK_DT * 0.5) == 1, true);

    clock = (TimeStep){0};
    Check("catch-up work is capped", TimeStepAdvance(&clock, SIM_TICK_DT * 8.5) == 5, true);
    Check("catch-up preserves interpolation", fabsf(clock.alpha - 0.5f) < 0.0001f, true);
    Check("only whole overdue ticks are dropped",
          fabs(clock.dropped - SIM_TICK_DT * 3.0) < 0.0001, true);
    Check("a stall cannot create an endless backlog", TimeStepAdvance(&clock, 10.0) == 5, true);
    Check("invalid time cannot advance simulation", TimeStepAdvance(&clock, NAN) == 0, true);
    Check("negative time cannot rewind simulation", TimeStepAdvance(&clock, -1.0) == 0, true);

    clock = (TimeStep){0};
    int ticks = 0;
    for (int i = 0; i < 240; i++) ticks += TimeStepAdvance(&clock, SIM_TICK_DT * 0.25);
    Check("240 render frames produce 60 simulation ticks", ticks == 60, true);
}
