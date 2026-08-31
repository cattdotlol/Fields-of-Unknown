#ifndef UI_HUD_H
#define UI_HUD_H

/* A view onto gVitals, and nothing more - it owns no state of its own.
   Toggleable, because reading the cat's condition off the cat is the
   other way to play this. */
void HudDraw(void);

#endif /* UI_HUD_H */
