#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "raylib.h"

#include <stdbool.h>

/* Vertical keyboard/mouse navigation shared by the menu screens.
   Returns true when the focused row changed. */
bool NavVertical(int *focus, int count);

/* Each widget draws itself and reports interaction. `focused` drives the
   highlight; the caller owns focus so navigation stays in one place. */
bool WidgetButton(Rectangle bounds, const char *label, bool focused);
bool WidgetToggle(Rectangle bounds, const char *label, bool *value, bool focused);
bool WidgetSlider(Rectangle bounds, const char *label, float *value, bool focused);

/* Returns the index activated this frame, or -1. Updates *selected on
   hover and arrow keys. */
int WidgetMenu(const char **items, int count, int *selected, float centerX, float startY, float itemSize);

#endif /* UI_WIDGETS_H */
