#include "gfx/filmfx.h"
#include "ui/theme.h"

#include "raylib.h"

#include <math.h>

#define BAR_HEIGHT 92.0f    /* design pixels, per bar */

void FilmLetterbox(float amount)
{
    if (amount <= 0.0f) return;

    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int bar = (int)floorf(BAR_HEIGHT * ThemeScale() * amount);

    DrawRectangle(0, 0, w, bar, BLACK);
    DrawRectangle(0, h - bar, w, bar, BLACK);
}

void FilmScanlines(float intensity)
{
    if (intensity <= 0.0f) return;

    int w = GetScreenWidth();
    int h = GetScreenHeight();

    float s = PixelScale();
    int step = (int)(s * 3.0f);
    int thickness = (int)s;

    for (int y = 0; y < h; y += step)
    {
        DrawRectangle(0, y, w, thickness, Fade(BLACK, 0.35f * intensity));
    }
}

void FilmGrain(float intensity)
{
    if (intensity <= 0.0f) return;

    int w = GetScreenWidth();
    int h = GetScreenHeight();

    int side = (int)PixelScale();
    int count = (int)(600.0f * intensity);

    for (int i = 0; i < count; i++)
    {
        int x = GetRandomValue(0, w - 1);
        int y = GetRandomValue(0, h - 1);
        float a = (float)GetRandomValue(4, 16) / 100.0f;

        DrawRectangle(x, y, side, side, Fade(RAYWHITE, a * intensity));
    }
}

void FilmVignette(float intensity)
{
    if (intensity <= 0.0f) return;

    int w = GetScreenWidth();
    int h = GetScreenHeight();
    int band = (int)(w * 0.22f);
    int vband = (int)(h * 0.24f);

    Color dark = Fade(BLACK, 0.75f * intensity);
    Color none = Fade(BLACK, 0.0f);

    DrawRectangleGradientH(0, 0, band, h, dark, none);
    DrawRectangleGradientH(w - band, 0, band, h, none, dark);
    DrawRectangleGradientV(0, 0, w, vband, dark, none);
    DrawRectangleGradientV(0, h - vband, w, vband, none, dark);
}
