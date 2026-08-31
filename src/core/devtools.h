#ifndef CORE_DEVTOOLS_H
#define CORE_DEVTOOLS_H

#include <stdbool.h>

/* An in-game dev menu on the tilde key.

   Compiled out entirely in release builds: the whole implementation sits
   behind NDEBUG and the functions become no-ops, so a shipped binary
   cannot be talked into opening it. */

void DevToolsToggle(void);
bool DevToolsOpen(void);

/* Returns true when it has taken the input, so gameplay should not also
   act on the same keys. */
bool DevToolsUpdate(float dt);
void DevToolsDraw(void);

/* Things the menu switches on that the rest of the game has to respect. */
bool DevGodMode(void);
bool DevFrozen(void);
bool DevShowHitboxes(void);
bool DevShowAI(void);

#endif /* CORE_DEVTOOLS_H */
