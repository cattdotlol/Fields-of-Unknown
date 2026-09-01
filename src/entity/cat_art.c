#include "entity/cat_art.h"

#include "gfx/sprite.h"

const char *const CatArtSit[CAT_ART_H] = {
    "...D......D..........",
    "..DOD....DMD.........",
    "..DPOD..DMOMD........",
    "..DLDDDDOOPOD........",
    "..DDMOOOOLLOD........",
    "..DMOMOMMMLOD........",
    "..DMMLMMMMOOD........",
    ".DMELLLEMMMOOD.......",
    ".DMELLLEMMOOOOD..DD..",
    "KMPPLMLLPPLMOD..DLLD.",
    ".KMLMLMLLLMDK...DLLLK",
    "..KMLLLLLDDMOK...DOLK",
    "...KKMLMMMMODDK..DMOK",
    "...KLLLLLMMMMOK..DMMK",
    "...KMMLLLMMMMDDK.DMMK",
    "....DLLLLMMOMMODKOMK.",
    "....DDLLLLDOMMMOKOOK.",
    "....KMDLLLDMMMOODOK..",
    "....KMKMLDOMMMMODK...",
    "....KMKMLDMLLMOKK....",
    ".....KKKKKKKKKK......",
};

/* Black cat palette. Kept deliberately lighter than pure black: the
   terrain behind it is (26,30,38), so a truly black cat would vanish
   into the ruins. */
Color CatArtColor(char cell)
{
    switch (cell)
    {
        case 'K': return (Color){   4,   4,   6, 255 };   /* outline     */
        case 'D': return (Color){  20,  19,  24, 255 };   /* darkest fur */
        case 'O': return (Color){  36,  34,  43, 255 };   /* body fur    */
        case 'M': return (Color){  58,  55,  68, 255 };   /* mid fur     */
        case 'L': return (Color){  94,  89, 105, 255 };   /* lit fur     */
        case 'P': return (Color){  98,  58,  67, 255 };   /* ears, nose  */
        case 'E': return (Color){ 176, 210, 100, 255 };   /* eyes        */
        default:  return BLANK;
    }
}

/* What the blitter needs to know that the grid does not say. */
typedef struct CatPaint {
    float eyesOpen;
} CatPaint;

static Color CatCell(char cell, const void *ctx)
{
    const CatPaint *paint = (const CatPaint *)ctx;

    /* Shut eyes read as fur, not as holes. */
    if (cell == 'E' && paint && paint->eyesOpen < 0.5f) cell = 'D';

    return CatArtColor(cell);
}

void CatArtDrawFrame(const char *const *rows, int w, int h,
                     float x, float y, float cellW, float cellH,
                     float facing, float eyesOpen, float fade, float clipBelowY)
{
    Sprite sprite = { rows, w, h, CAT_ART_AUTHORED_FACING };
    CatPaint paint = { eyesOpen };

    SpriteStyle style = {
        .cellW = cellW,
        .cellH = cellH,
        .facing = facing,
        .fade = fade,
        .clipBelowY = clipBelowY,
    };

    SpriteDrawTopLeft(sprite, CatCell, &paint, x, y, style);
}

void CatArtDraw(float x, float y, float cellW, float cellH,
                float facing, float eyesOpen, float fade, float clipBelowY)
{
    CatArtDrawFrame(CatArtSit, CAT_ART_W, CAT_ART_H, x, y, cellW, cellH,
                    facing, eyesOpen, fade, clipBelowY);
}
