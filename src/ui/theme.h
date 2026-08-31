#ifndef UI_THEME_H
#define UI_THEME_H

#include "raylib.h"

typedef struct Theme {
    Font  font;
    Color text;
    Color textDim;
    Color accent;
    Color accentDim;
    Color panel;
    Color border;
} Theme;

extern Theme gTheme;

void ThemeLoad(void);
void ThemeUnload(void);

/* Uniform scale so layouts authored at DESIGN_HEIGHT hold up at any size. */
float ThemeScale(void);

/* Screen pixels per art pixel, never below 1. The single source of truth
   for how chunky the game looks - tune ART_PIXEL, not call sites. */
float PixelScale(void);

/* Rounds a requested size to a whole multiple of the bitmap font's base
   size. Every glyph then lands on an integer pixel grid, which is what
   keeps the pixel font crisp instead of resampled. */
float UiSnap(float size);

float   UiLineHeight(float size);
Vector2 UiMeasure(const char *text, float size);
void    UiText(const char *text, float x, float y, float size, Color color);
void    UiTextCentered(const char *text, float cx, float y, float size, Color color);

/* Word-wraps within maxWidth. Returns the height consumed. */
float UiTextWrapped(const char *text, float x, float y, float maxWidth, float size, Color color);

/* Resolves an asset path whether run from the project root or build/. */
const char *AssetPath(const char *relative);

#endif /* UI_THEME_H */
