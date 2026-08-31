#include "entity/vitals.h"
#include "entity/cat.h"
#include "world/season.h"
#include "world/weather.h"

Vitals gVitals;

/* Per second. Hunger is the clock everything else hangs off: about
   fifteen minutes of ordinary movement from full to empty. */
#define HUNGER_BASE     0.0011f

#define STAMINA_RUN     0.30f
#define STAMINA_SWIM    0.12f
#define STAMINA_REGEN   0.22f
#define STAMINA_JUMP    0.06f

#define WARMTH_BASE     0.0025f
#define WARMTH_REGEN    0.0060f

/* Sixteen seconds under, two seconds to get it back. Drowning is meant
   to be faster than starving: it is the one that should panic you. */
#define BREATH_DRAIN    0.0620f
#define BREATH_REFILL   0.5000f
#define HEALTH_DROWNING 0.1000f

#define HEALTH_STARVING 0.0200f
#define HEALTH_FREEZING 0.0100f
#define HEALTH_REGEN    0.0060f

static float Clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

void VitalsReset(void)
{
    gVitals.health = 1.0f;
    gVitals.hunger = 0.85f;
    gVitals.stamina = 1.0f;
    gVitals.warmth = 0.75f;
    gVitals.breath = 1.0f;
    gVitals.dead = false;
}

bool VitalsHasStamina(float amount)
{
    return gVitals.stamina >= amount;
}

void VitalsSpendStamina(float amount)
{
    gVitals.stamina = Clamp01(gVitals.stamina - amount);
}

void VitalsFeed(float amount)
{
    gVitals.hunger = Clamp01(gVitals.hunger + amount);
}

/* Signed on every axis, so one call covers a meal and a poisoning. */
void VitalsApply(float hunger, float health, float warmth)
{
    gVitals.hunger = Clamp01(gVitals.hunger + hunger);
    gVitals.warmth = Clamp01(gVitals.warmth + warmth);
    gVitals.health = Clamp01(gVitals.health + health);

    if (gVitals.health <= 0.0f) gVitals.dead = true;
}

void VitalsHurt(float amount)
{
    gVitals.health = Clamp01(gVitals.health - amount);
    if (gVitals.health <= 0.0f) gVitals.dead = true;
}

void VitalsUpdate(float dt)
{
    if (gVitals.dead) return;

    CatState state = CatCurrentState();
    bool swimming = CatIsSwimming();

    float cold = 1.0f - SeasonTemperature();
    float rain = WeatherRain();

    /* --- warmth ------------------------------------------------------
       Loss and recovery run at the same time and settle at an
       equilibrium, rather than draining whenever it is even slightly
       wet. Without this, light rain was eventually fatal no matter what
       the cat did, and freezing beat starving to the punch. */
    float wet = swimming ? 1.0f : (rain * 0.55f);

    float loss  = wet * (0.006f + cold * 0.010f);
    float regen = (1.0f - wet) * WARMTH_REGEN * SeasonTemperature();

    gVitals.warmth = Clamp01(gVitals.warmth + (regen - loss) * dt);

    /* --- hunger: effort costs, and being cold costs more -------------- */
    float effort = 0.7f;
    switch (state)
    {
        case CAT_RUN:    effort = 2.4f; break;
        case CAT_WALK:   effort = 1.3f; break;
        case CAT_SWIM:   effort = 2.0f; break;
        case CAT_AIR:    effort = 1.4f; break;
        case CAT_CROUCH: effort = 0.9f; break;
        default:         effort = 0.7f; break;
    }

    float chill = 1.0f + (1.0f - gVitals.warmth) * 0.8f;
    gVitals.hunger = Clamp01(gVitals.hunger - HUNGER_BASE * effort * chill * dt);

    /* --- stamina ------------------------------------------------------ */
    if (state == CAT_RUN)        gVitals.stamina = Clamp01(gVitals.stamina - STAMINA_RUN * dt);
    else if (swimming)           gVitals.stamina = Clamp01(gVitals.stamina - STAMINA_SWIM * dt);
    else
    {
        /* Recovery is worse on an empty stomach. */
        float rate = STAMINA_REGEN * (0.35f + gVitals.hunger * 0.65f);
        gVitals.stamina = Clamp01(gVitals.stamina + rate * dt);
    }

    /* --- breath -------------------------------------------------------
       Only the head being under counts; swimming along the surface is
       free. */
    if (CatIsSubmerged())
    {
        gVitals.breath = Clamp01(gVitals.breath - BREATH_DRAIN * dt);
    }
    else
    {
        gVitals.breath = Clamp01(gVitals.breath + BREATH_REFILL * dt);
    }

    /* --- health follows from the rest --------------------------------- */
    float drain = 0.0f;

    if (gVitals.breath <= 0.0f) drain += HEALTH_DROWNING;
    if (gVitals.hunger <= 0.0f) drain += HEALTH_STARVING;
    if (gVitals.warmth <= 0.0f) drain += HEALTH_FREEZING;

    if (drain > 0.0f)
    {
        VitalsHurt(drain * dt);
    }
    else if (gVitals.hunger > 0.5f && gVitals.warmth > 0.4f)
    {
        gVitals.health = Clamp01(gVitals.health + HEALTH_REGEN * dt);
    }
}
