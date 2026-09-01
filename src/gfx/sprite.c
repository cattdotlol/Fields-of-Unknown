#include "gfx/sprite.h"

/* Where the grid's top-left lands when the sprite is standing at `at`. */
static Vector2 StandingOrigin(Sprite s, Vector2 at, float pixel)
{
    return (Vector2){ at.x - (float)s.w * pixel * 0.5f,
                      at.y - (float)s.h * pixel };
}

static bool Mirrored(Sprite s, float facing)
{
    /* Only mirror when the wanted facing disagrees with the art's. A
       sprite authored facing left and asked to face left is drawn as
       written. */
    return (facing * s.authored) < 0.0f;
}

float SpriteHalfWidth(Sprite s, float pixel)
{
    return (float)s.w * pixel * 0.5f;
}

void SpriteDrawTopLeft(Sprite s, SpritePalette palette, const void *ctx,
                       float x, float y, SpriteStyle style)
{
    bool mirror = Mirrored(s, style.facing);

    for (int row = 0; row < s.h; row++)
    {
        for (int col = 0; col < s.w; col++)
        {
            int read = mirror ? (s.w - 1 - col) : col;
            Color c = palette(s.rows[row][read], ctx);

            if (c.a == 0) continue;

            float px = x + (float)col * style.cellW;
            float py = y + (float)row * style.cellH;

            /* Used for the waterline: nothing under the surface is drawn. */
            if (style.clipBelowY > 0.0f && py > style.clipBelowY) continue;

            /* The palette's own alpha survives, rather than being
               replaced - Fade on its own would flatten a deliberately
               translucent cell to fully opaque. */
            float a = style.fade * ((float)c.a / 255.0f);

            /* Float rects, NOT DrawRectangle with int casts: cells are
               fractional (1.5 units is normal), and truncating puts
               neighbouring columns 1 or 2 units apart at random, which
               shears the sprite. */
            DrawRectangleRec((Rectangle){ px, py, style.cellW, style.cellH },
                             Fade(c, a));
        }
    }
}

void SpriteDrawStanding(Sprite s, SpritePalette palette, const void *ctx,
                        Vector2 at, float pixel, float facing, float fade)
{
    Vector2 origin = StandingOrigin(s, at, pixel);

    SpriteStyle style = {
        .cellW = pixel,
        .cellH = pixel,
        .facing = facing,
        .fade = fade,
        .clipBelowY = 0.0f,
    };

    SpriteDrawTopLeft(s, palette, ctx, origin.x, origin.y, style);
}

int SpriteCells(Sprite s, Vector2 at, float pixel, float facing,
                char cell, Vector2 *out, int max)
{
    Vector2 origin = StandingOrigin(s, at, pixel);
    bool mirror = Mirrored(s, facing);

    int found = 0;

    for (int row = 0; row < s.h; row++)
    {
        for (int col = 0; col < s.w; col++)
        {
            int read = mirror ? (s.w - 1 - col) : col;

            if (s.rows[row][read] != cell) continue;

            if (out && found < max)
            {
                out[found] = (Vector2){ origin.x + (float)col * pixel,
                                        origin.y + (float)row * pixel };
            }

            found++;
        }
    }

    return found;
}
