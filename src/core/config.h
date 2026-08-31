#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

/* Proper case for the OS window title; the caps variant is the logo on
   the title screen, where the pixel font reads better uppercase. */
#define GAME_TITLE         "Fields of Unknown"
#define GAME_TITLE_DISPLAY "Fields of Unknown"
#define GAME_TAGLINE       "A CAT, A LONG WAY FROM HOME"
#define WINDOW_WIDTH   1280
#define WINDOW_HEIGHT  720
#define SETTINGS_FILE  "settings.cfg"

/* The menu backdrop and the intro build the same world from this. */
#define WORLD_SEED     20260831u

/* Layout is authored against this height and scaled to the real window. */
#define DESIGN_HEIGHT  720.0f

/* Size of one art pixel, measured in screen pixels at DESIGN_HEIGHT.
   Everything visual derives from this through PixelScale(), so the whole
   game sits on one grid. Raise it for a chunkier look, lower it for a
   finer one. */
#define ART_PIXEL      0.5f

/* Screen pixels per world unit, before display scaling. Lower shows more
   of the world and makes everything finer. */
#define WORLD_ZOOM     1.0f

#endif /* CORE_CONFIG_H */
