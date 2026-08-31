#ifndef UI_CURSOR_H
#define UI_CURSOR_H

/* Hand-drawn pixel cursor. The OS cursor is hidden and this is drawn
   last, on top of everything including the transition fade. */
void CursorLoad(void);
void CursorUpdate(float dt);
void CursorDraw(void);

/* Widgets call this while the pointer is over something clickable; it
   resets every frame, so it only needs setting, never clearing. */
void CursorHint(void);

#endif /* UI_CURSOR_H */
