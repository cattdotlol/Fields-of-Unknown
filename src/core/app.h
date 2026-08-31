#ifndef CORE_APP_H
#define CORE_APP_H

#include "screens/screens.h"

#include <stdbool.h>

void AppInit(void);
void AppRun(void);
void AppShutdown(void);

/* Screens call these; the transition is faded, so the swap happens
   at the midpoint rather than immediately. */
void AppGoTo(ScreenId id);

/* Fraction of a fixed tick left over this frame, for render
   interpolation. 0..1. */
float AppRenderAlpha(void);
void AppQuit(void);

#endif /* CORE_APP_H */
