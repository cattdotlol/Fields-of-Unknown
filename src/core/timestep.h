#ifndef CORE_TIMESTEP_H
#define CORE_TIMESTEP_H

#define SIM_TICK_DT (1.0 / 60.0)
#define SIM_MAX_CATCHUP 5
#define SIM_MAX_FRAME 0.25

typedef struct TimeStep {
    double remainder;
    double dropped;  /* seconds discarded to keep simulation work bounded */
    float alpha;
} TimeStep;

/* Returns the number of fixed ticks to run. Keeps the fractional tick
   even when whole ticks must be dropped after a stall. Zero-init to reset. */
int TimeStepAdvance(TimeStep *clock, double elapsed);

#endif
