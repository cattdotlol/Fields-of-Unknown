#ifndef GFX_SCENE_FLOOD_H
#define GFX_SCENE_FLOOD_H

#include <stdbool.h>

/* The unknown planet: a half-flooded, overgrown industrial sprawl under
   an alien sky, with the wreck the cat came down in still sitting in the
   water. Geometry is normalised so it survives resizes, and it doubles
   as the menu backdrop - the menus and the game show the same place. */

void  FloodSceneInit(unsigned int seed);
void  FloodSceneUpdate(float dt);

/* reveal:  0 = fully dark, 1 = fully lit.
   rain:    0..1, scales how much rain falls in the backdrop.
   cameraX: world x the view is centred on. The town scrolls past at a
            fraction of that and is generated from the building index, so
            it goes on as far as the world does. */
void  FloodSceneDraw(float reveal, float rain, float cameraX, bool snow);

/* Screen-space y of the waterline, for placing things on it. */
float FloodSceneWaterY(void);

/* Normalised x of the crashed pod, so the intro can sit the cat by it. */
float FloodScenePodX(void);

#endif /* GFX_SCENE_FLOOD_H */
