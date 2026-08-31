#ifndef GFX_FILMFX_H
#define GFX_FILMFX_H

/* Screen-space "shot on a bad camera" pass. Kept separate from any one
   screen so gameplay can reuse it for cutscenes later. */

void FilmLetterbox(float amount);   /* 0 = none, 1 = full bars */
void FilmScanlines(float intensity);
void FilmGrain(float intensity);
void FilmVignette(float intensity);

#endif /* GFX_FILMFX_H */
