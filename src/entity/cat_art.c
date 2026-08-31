#include "entity/cat_art.h"

#include <math.h>

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

void CatArtDrawFrame(const char *const *rows, int w, int h,
                     float x, float y, float cellW, float cellH,
                     float facing, float eyesOpen, float fade, float clipBelowY)
{
    /* Mirror only when the wanted facing differs from the art's. */
    bool mirror = (facing * CAT_ART_AUTHORED_FACING) < 0.0f;

    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            int read = mirror ? (w - 1 - col) : col;
            char cell = rows[row][read];

            if (cell == '.') continue;

            /* Shut eyes read as fur, not as holes. */
            if (cell == 'E' && eyesOpen < 0.5f) cell = 'D';

            float px = x + (float)col * cellW;
            float py = y + (float)row * cellH;

            /* Used for the waterline: nothing below the surface is drawn. */
            if (clipBelowY > 0.0f && py > clipBelowY) continue;

            Color c = CatArtColor(cell);

            /* Float rect, NOT DrawRectangle with int casts: world units
               here are fractional (1.5), so truncating put columns 1 or 2
               units apart at random and sheared the sprite. */
            DrawRectangleRec((Rectangle){ px, py, cellW, cellH }, Fade(c, fade));
        }
    }
}

void CatArtDraw(float x, float y, float cellW, float cellH,
                float facing, float eyesOpen, float fade, float clipBelowY)
{
    CatArtDrawFrame(CatArtSit, CAT_ART_W, CAT_ART_H, x, y, cellW, cellH,
                    facing, eyesOpen, fade, clipBelowY);
}
