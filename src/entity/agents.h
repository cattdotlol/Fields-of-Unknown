#ifndef ENTITY_AGENTS_H
#define ENTITY_AGENTS_H

/* Every creature module, in one list, in the order they belong in.

   Before this the gameplay screen kept three parallel lists of the same
   three modules - one to reset them, one to tick them, one to draw them -
   and a fourth thing to remember was that the sea has to be drawn behind
   the land animals. Adding an animal meant editing all of that, in a
   screen that has no business knowing what animals exist. Forgetting one
   of the three does not fail to build; it fails quietly, months later,
   as a creature that never despawns or never appears.

   So the screen asks for creatures once and the entity layer answers for
   all of them. Adding an animal is now its own module plus one line of
   MODULES in agents.c.

   Behaviour stays where it is. A rat's nerve and a jellyfish's pulse have
   nothing in common, and collapsing them into one update function would
   buy a shorter file and a worse game. What is shared here is only the
   lifecycle - the part that genuinely is the same for all of them. */

typedef struct CreatureModule {
    const char *name;

    void (*reset)(void);
    void (*tick)(float dt);

    /* Draw order is the order of MODULES in agents.c: the sea has to go
       behind anything standing on the roof above it. */
    void (*draw)(float alpha, float left, float right);
} CreatureModule;

void AgentsReset(void);
void AgentsFixedUpdate(float dt);
void AgentsDraw(float alpha, float left, float right);

int         AgentsModuleCount(void);
const char *AgentsModuleName(int index);

#endif /* ENTITY_AGENTS_H */
