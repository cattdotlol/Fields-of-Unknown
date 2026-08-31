#ifndef SCREENS_SCREENS_H
#define SCREENS_SCREENS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum ScreenId {
    SCREEN_NONE = -1,
    SCREEN_TITLE = 0,
    SCREEN_INTRO,
    SCREEN_SETTINGS,
    SCREEN_KEYBINDS,
    SCREEN_GAMEPLAY,
    SCREEN_COUNT
} ScreenId;

/* A screen is a plain vtable of lifecycle hooks. Any of them may be NULL.
   Adding a screen means writing one file and registering it in app.c. */
typedef struct Screen {
    void (*init)(void);

    /* Per-frame: input edges, camera smoothing, anything cosmetic. dt
       varies with the framerate. */
    void (*update)(float dt);

    /* Fixed 60Hz: simulation only. dt is always TICK_DT, so physics is
       deterministic and frame-rate independent. May run 0..N times per
       frame, so treat input edges as idempotent here. */
    void (*fixedUpdate)(float dt);

    void (*draw)(void);
    void (*unload)(void);

    /* opaque:      screen paints its own full background, so the shared
                    animated backdrop is skipped.
       hidesCursor: suppress the pixel cursor (cutscenes). */
    bool opaque;
    bool hidesCursor;
} Screen;

extern const Screen ScreenTitle;
extern const Screen ScreenIntro;
extern const Screen ScreenKeybinds;
extern const Screen ScreenSettings;
extern const Screen ScreenGameplay;

#endif /* SCREENS_SCREENS_H */
