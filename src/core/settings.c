#include "core/settings.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

Settings gSettings;

void SettingsDefaults(void)
{
    gSettings.masterVolume = 0.80f;
    gSettings.musicVolume  = 0.70f;
    gSettings.sfxVolume    = 0.80f;
    gSettings.fullscreen   = false;
    gSettings.vsync        = true;
    gSettings.showFps      = true;
}

void SettingsApply(void)
{
    SetMasterVolume(gSettings.masterVolume);

    if (gSettings.fullscreen != IsWindowFullscreen()) ToggleFullscreen();

    if (gSettings.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else                 ClearWindowState(FLAG_VSYNC_HINT);
}

bool SettingsSave(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) return false;

    fprintf(f, "master=%.3f\n",     (double)gSettings.masterVolume);
    fprintf(f, "music=%.3f\n",      (double)gSettings.musicVolume);
    fprintf(f, "sfx=%.3f\n",        (double)gSettings.sfxVolume);
    fprintf(f, "fullscreen=%d\n",   gSettings.fullscreen ? 1 : 0);
    fprintf(f, "vsync=%d\n",        gSettings.vsync ? 1 : 0);
    fprintf(f, "showfps=%d\n",      gSettings.showFps ? 1 : 0);

    fclose(f);
    return true;
}

bool SettingsLoad(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) return false;

    char key[64];
    float value;
    char line[128];

    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (sscanf(line, "%63[^=]=%f", key, &value) != 2) continue;

        if      (strcmp(key, "master")     == 0) gSettings.masterVolume = value;
        else if (strcmp(key, "music")      == 0) gSettings.musicVolume  = value;
        else if (strcmp(key, "sfx")        == 0) gSettings.sfxVolume    = value;
        else if (strcmp(key, "fullscreen") == 0) gSettings.fullscreen   = (value != 0.0f);
        else if (strcmp(key, "vsync")      == 0) gSettings.vsync        = (value != 0.0f);
        else if (strcmp(key, "showfps")    == 0) gSettings.showFps      = (value != 0.0f);
    }

    fclose(f);
    return true;
}
