#ifndef ENTITY_VITALS_H
#define ENTITY_VITALS_H

#include <stdbool.h>

/* What the cat is running on. Kept separate from both the cat and the
   HUD: the cat spends these, the world drains them, and the interface is
   only ever a view onto them. */

typedef struct Vitals {
    float health;    /* 0..1 */
    float hunger;    /* 1 = fed, 0 = starving */
    float stamina;   /* 0..1, gates running */
    float warmth;    /* 0..1, drained by rain, water and cold seasons */
    bool  dead;
} Vitals;

extern Vitals gVitals;

void VitalsReset(void);
void VitalsUpdate(float dt);

bool VitalsHasStamina(float amount);
void VitalsSpendStamina(float amount);

void VitalsFeed(float amount);
void VitalsHurt(float amount);

#endif /* ENTITY_VITALS_H */
