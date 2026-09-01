#include "entity/agents.h"

#include "entity/aquatic.h"
#include "entity/creatures.h"
#include "entity/rat.h"
#include "entity/stalker.h"

#include <stddef.h>

static const CreatureModule AQUATIC = {
    .name = "aquatic",
    .reset = AquaticReset,
    .tick = AquaticFixedUpdate,
    .draw = AquaticDraw,
};

static const CreatureModule RATS = {
    .name = "rats",
    .reset = RatsReset,
    .tick = RatsFixedUpdate,
    .draw = RatsDraw,
};

static const CreatureModule STALKERS = {
    .name = "stalkers",
    .reset = StalkersReset,
    .tick = StalkersFixedUpdate,
    .draw = StalkersDraw,
};

/* The list. Order is draw order, back to front: the sea first, because
   everything on land stands on the roof above it. */
static const CreatureModule *const MODULES[] = {
    &AQUATIC,
    &RATS,
    &STALKERS,
};

#define MODULE_COUNT ((int)(sizeof(MODULES) / sizeof(MODULES[0])))

void AgentsReset(void)
{
    /* The census first: a module's reset registers what to call when one
       of its animals is eaten, and clearing it afterwards would throw
       those away. */
    CreaturesReset();

    for (int i = 0; i < MODULE_COUNT; i++)
    {
        if (MODULES[i]->reset) MODULES[i]->reset();
    }
}

void AgentsFixedUpdate(float dt)
{
    for (int i = 0; i < MODULE_COUNT; i++)
    {
        if (MODULES[i]->tick) MODULES[i]->tick(dt);
    }
}

void AgentsDraw(float alpha, float left, float right)
{
    for (int i = 0; i < MODULE_COUNT; i++)
    {
        if (MODULES[i]->draw) MODULES[i]->draw(alpha, left, right);
    }
}

int AgentsModuleCount(void)
{
    return MODULE_COUNT;
}

const char *AgentsModuleName(int index)
{
    if (index < 0 || index >= MODULE_COUNT) return "?";

    return MODULES[index]->name;
}
