#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <stdbool.h>

typedef struct Settings {
    float masterVolume;   /* 0..1 */
    float musicVolume;    /* 0..1 */
    float sfxVolume;      /* 0..1 */
    bool  fullscreen;
    bool  vsync;
    bool  showFps;
} Settings;

extern Settings gSettings;

void SettingsDefaults(void);
void SettingsApply(void);                 /* push state into raylib */
bool SettingsSave(const char *path);
bool SettingsLoad(const char *path);

#endif /* CORE_SETTINGS_H */
