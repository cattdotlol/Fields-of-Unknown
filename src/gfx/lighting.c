#include "gfx/lighting.h"
#include "world/terrain.h"

#include <math.h>
#include <stddef.h>

#define SHADOW_LENGTH 2600.0f    /* far enough to leave the screen */
#define MAX_CASTERS      64

static RenderTexture2D sMap;
static bool  sReady;
static int   sWidth;
static int   sHeight;
static bool  sFrameReady;

void LightingLoad(void)
{
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    if (width <= 0 || height <= 0) return;

    /* Allocate before releasing the old map: a failed resize must not
       discard the last usable GPU resource. */
    RenderTexture2D replacement = LoadRenderTexture(width, height);
    if (replacement.id == 0 || replacement.texture.id == 0)
    {
        if (replacement.id != 0) UnloadRenderTexture(replacement);
        return;
    }

    LightingUnload();
    sMap = replacement;
    sWidth = width;
    sHeight = height;
    sReady = true;
    SetTextureFilter(sMap.texture, TEXTURE_FILTER_BILINEAR);
}

void LightingUnload(void)
{
    if (!sReady) return;

    UnloadRenderTexture(sMap);
    sReady = false;
    sFrameReady = false;
}

/* The window can be resized at any time; the lightmap has to follow. */
static void EnsureSize(void)
{
    if (sReady && sWidth == GetScreenWidth() && sHeight == GetScreenHeight()) return;

    LightingLoad();
}

void LightingBegin(float ambient)
{
    sFrameReady = false;
    if (GetScreenWidth() <= 0 || GetScreenHeight() <= 0) return;
    EnsureSize();
    if (!sReady || sWidth != GetScreenWidth() || sHeight != GetScreenHeight()) return;
    sFrameReady = true;

    if (ambient < 0.0f) ambient = 0.0f;
    if (ambient > 1.0f) ambient = 1.0f;

    unsigned char level = (unsigned char)(ambient * 255.0f);

    BeginTextureMode(sMap);
        ClearBackground((Color){ level, level, level, 255 });
    EndTextureMode();
}

/* Two triangles covering everything behind `solid` from the light's point
   of view: the shadow of one box. */
static void CastShadow(Vector2 light, Rectangle solid)
{
    Vector2 corner[4] = {
        { solid.x,               solid.y },
        { solid.x + solid.width, solid.y },
        { solid.x + solid.width, solid.y + solid.height },
        { solid.x,               solid.y + solid.height },
    };

    /* The silhouette is the pair of corners with the widest angle between
       them as seen from the light. */
    int a = 0, b = 0;
    float widest = -1.0f;

    for (int i = 0; i < 4; i++)
    {
        for (int j = i + 1; j < 4; j++)
        {
            float ax = corner[i].x - light.x, ay = corner[i].y - light.y;
            float bx = corner[j].x - light.x, by = corner[j].y - light.y;

            float la = sqrtf(ax * ax + ay * ay);
            float lb = sqrtf(bx * bx + by * by);
            if (la < 0.001f || lb < 0.001f) continue;

            float cosang = (ax * bx + ay * by) / (la * lb);
            float ang = acosf((cosang < -1.0f) ? -1.0f : (cosang > 1.0f ? 1.0f : cosang));

            if (ang > widest) { widest = ang; a = i; b = j; }
        }
    }

    Vector2 near1 = corner[a];
    Vector2 near2 = corner[b];

    Vector2 dir1 = { near1.x - light.x, near1.y - light.y };
    Vector2 dir2 = { near2.x - light.x, near2.y - light.y };

    float l1 = sqrtf(dir1.x * dir1.x + dir1.y * dir1.y);
    float l2 = sqrtf(dir2.x * dir2.x + dir2.y * dir2.y);
    if (l1 < 0.001f || l2 < 0.001f) return;

    Vector2 far1 = { near1.x + dir1.x / l1 * SHADOW_LENGTH,
                     near1.y + dir1.y / l1 * SHADOW_LENGTH };
    Vector2 far2 = { near2.x + dir2.x / l2 * SHADOW_LENGTH,
                     near2.y + dir2.y / l2 * SHADOW_LENGTH };

    /* raylib's triangles are wound counter-clockwise; flip if this quad
       came out the other way round or nothing draws. */
    float cross = (near2.x - near1.x) * (far1.y - near1.y) -
                  (near2.y - near1.y) * (far1.x - near1.x);

    if (cross > 0.0f)
    {
        DrawTriangle(near1, near2, far2, BLACK);
        DrawTriangle(near1, far2, far1, BLACK);
    }
    else
    {
        DrawTriangle(near1, far2, near2, BLACK);
        DrawTriangle(near1, far1, far2, BLACK);
    }
}

void LightingAddLight(Camera2D camera, Vector2 world, float radius,
                      Color colour, float intensity)
{
    if (!sFrameReady || radius <= 0.0f || camera.zoom <= 0.0f || intensity <= 0.0f) return;

    Vector2 screen = GetWorldToScreen2D(world, camera);
    float screenRadius = radius * camera.zoom;

    /* Off screen by more than its own reach: nothing to contribute. */
    if (screen.x + screenRadius < 0.0f || screen.x - screenRadius > (float)sWidth) return;
    if (screen.y + screenRadius < 0.0f || screen.y - screenRadius > (float)sHeight) return;

    BeginTextureMode(sMap);
        BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient((int)screen.x, (int)screen.y, screenRadius,
                               Fade(colour, intensity), Fade(colour, 0.0f));
        EndBlendMode();

        /* Then take back everything the light cannot actually see. */
        BeginBlendMode(BLEND_SUBTRACT_COLORS);
            int cast = 0;

            for (int i = 0; i < TerrainCount() && cast < MAX_CASTERS; i++)
            {
                Rectangle r = TerrainSolid(i);

                /* Only things inside the light's reach can shadow it. */
                float cx = r.x + r.width * 0.5f;
                float cy = r.y + r.height * 0.5f;
                float dx = cx - world.x;
                float dy = cy - world.y;

                if (dx * dx + dy * dy > (radius + r.width + r.height) *
                                        (radius + r.width + r.height)) continue;

                Rectangle onScreen = {
                    GetWorldToScreen2D((Vector2){ r.x, r.y }, camera).x,
                    GetWorldToScreen2D((Vector2){ r.x, r.y }, camera).y,
                    r.width * camera.zoom,
                    r.height * camera.zoom,
                };

                CastShadow(screen, onScreen);
                cast++;
            }
        EndBlendMode();
    EndTextureMode();
}

void LightingEnd(void)
{
    if (!sFrameReady) return;

    /* Multiply: the lightmap dims the scene rather than painting over it. */
    BeginBlendMode(BLEND_MULTIPLIED);
        DrawTexturePro(sMap.texture,
                       (Rectangle){ 0.0f, 0.0f, (float)sWidth, -(float)sHeight },
                       (Rectangle){ 0.0f, 0.0f, (float)sWidth, (float)sHeight },
                       (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    EndBlendMode();
    sFrameReady = false;
}
