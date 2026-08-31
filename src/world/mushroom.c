#include "world/mushroom.h"

#include <math.h>

#define HARVEST_MAX      192
#define REGROW_SECONDS   300.0f

/* Deliberately not ordered good-to-bad, and the two dangerous ones are
   not the two strangest looking. */
static const MushroomEffect EFFECTS[MUSHROOM_SPECIES] = {
    {  0.18f,  0.00f,  0.00f },   /* pale cap     - plain food      */
    {  0.12f,  0.00f,  0.10f },   /* rust knob    - food and warmth */
    { -0.05f, -0.18f,  0.00f },   /* blue veil    - poison          */
    {  0.08f,  0.02f,  0.00f },   /* ash gill     - small, harmless */
    {  0.32f,  0.00f,  0.00f },   /* violet crown - a real meal     */
    { -0.10f, -0.30f, -0.15f },   /* grey slime   - badly poisonous */
};

static const Color CAPS[MUSHROOM_SPECIES] = {
    { 198, 186, 168, 255 },   /* pale cap     */
    { 168,  92,  48, 255 },   /* rust knob    */
    {  86, 116, 176, 255 },   /* blue veil    */
    { 118, 116, 110, 255 },   /* ash gill     */
    { 128,  74, 158, 255 },   /* violet crown */
    { 132, 140, 118, 255 },   /* grey slime   */
};

typedef struct Harvest {
    int   chunk;
    int   slot;
    float at;
    bool  used;
} Harvest;

static Harvest sHarvest[HARVEST_MAX];
static int     sNext;
static float   sNow;

MushroomEffect MushroomEffectOf(unsigned char species)
{
    if (species >= MUSHROOM_SPECIES) return (MushroomEffect){ 0.0f, 0.0f, 0.0f };

    return EFFECTS[species];
}

Color MushroomCapColor(unsigned char species)
{
    if (species >= MUSHROOM_SPECIES) return CAPS[0];

    return CAPS[species];
}

void MushroomClearHarvests(void)
{
    for (int i = 0; i < HARVEST_MAX; i++) sHarvest[i].used = false;

    sNext = 0;
    sNow = 0.0f;
}

void MushroomTick(float dt)
{
    sNow += dt;
}

bool MushroomIsHarvested(int chunkIndex, int slot)
{
    for (int i = 0; i < HARVEST_MAX; i++)
    {
        if (!sHarvest[i].used) continue;
        if (sHarvest[i].chunk != chunkIndex || sHarvest[i].slot != slot) continue;

        if (sNow - sHarvest[i].at >= REGROW_SECONDS)
        {
            sHarvest[i].used = false;   /* grown back */
            return false;
        }

        return true;
    }

    return false;
}

void MushroomHarvest(int chunkIndex, int slot)
{
    if (MushroomIsHarvested(chunkIndex, slot)) return;

    /* Ring buffer: the oldest record is dropped, so a long walk quietly
       forgets what you picked hours ago. */
    sHarvest[sNext].chunk = chunkIndex;
    sHarvest[sNext].slot = slot;
    sHarvest[sNext].at = sNow;
    sHarvest[sNext].used = true;

    sNext = (sNext + 1) % HARVEST_MAX;
}

void MushroomDraw(const Mushroom *m, bool highlight)
{
    Color cap = MushroomCapColor(m->species);
    Color capDark = (Color){ (unsigned char)(cap.r * 0.6f),
                             (unsigned char)(cap.g * 0.6f),
                             (unsigned char)(cap.b * 0.6f), 255 };
    Color stem = (Color){ 206, 198, 180, 255 };
    Color stemDark = (Color){ 150, 144, 130, 255 };

    /* Size varies a little per mushroom so a patch is not a row of clones. */
    float scale = 2.0f + (float)(m->variant & 3u) * 0.5f;
    float capW = 7.0f * scale;
    float capH = 3.0f * scale;
    float stemW = 2.0f * scale;
    float stemH = 3.0f * scale;

    float x = m->x;
    float y = m->baseY;

    if (highlight)
    {
        /* Under the cat: a soft ring, no text. */
        float pulse = 0.35f + 0.25f * sinf((float)GetTime() * 5.0f);
        DrawRectangleRec((Rectangle){ x - capW * 0.75f, y - capH - stemH - scale,
                                      capW * 1.5f, capH + stemH + scale * 2.0f },
                         Fade(cap, pulse * 0.35f));
    }

    DrawRectangleRec((Rectangle){ x - stemW * 0.5f, y - stemH, stemW, stemH }, stem);
    DrawRectangleRec((Rectangle){ x - stemW * 0.5f, y - stemH, scale, stemH }, stemDark);

    DrawRectangleRec((Rectangle){ x - capW * 0.5f, y - stemH - capH, capW, capH }, cap);
    DrawRectangleRec((Rectangle){ x - capW * 0.5f + scale, y - stemH - capH - scale,
                                  capW - scale * 2.0f, scale }, cap);
    DrawRectangleRec((Rectangle){ x - capW * 0.5f, y - stemH - capH + capH - scale,
                                  capW, scale }, capDark);
}
