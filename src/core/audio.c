#include "core/audio.h"
#include "core/settings.h"
#include "ui/theme.h"
#include "entity/stalker.h"
#include "world/weather.h"

#include "raylib.h"

#include <math.h>
#include <stdlib.h>

#define RAIN_ASSET "assets/audio/rain_ambience.ogg"

/* The bed fades with the rain rather than cutting in and out. */
#define RAIN_FADE 0.6f

#define THUNDER_VARIANTS 3
#define THUNDER_RATE     22050
/* Integer arithmetic: a float here is not a constant expression, so the
   static sample buffer would not compile. 16/5 is 3.2 seconds. */
#define THUNDER_SAMPLES  (THUNDER_RATE * 16 / 5)

static Music sRain;
static bool  sRainReady;
static float sRainLevel;

static Sound sThunder[THUNDER_VARIANTS];
static bool  sThunderReady;

static Sound sRoar;
static bool  sRoarReady;

static unsigned int sNoise = 0x9E3779B9u;

static float WhiteNoise(void)
{
    sNoise = sNoise * 1664525u + 1013904223u;
    return ((float)((sNoise >> 8) & 0xFFFFu) / 32767.5f) - 1.0f;
}

/* Thunder is brown noise - white noise integrated - under a slow decay,
   with a sharper crack at the front for close strikes. Rolling it here
   means a fresh rumble every run and nothing extra to ship. */
static Sound GenerateThunder(float crack, float rumble, float decay)
{
    static float work[THUNDER_SAMPLES];
    static short samples[THUNDER_SAMPLES];

    float brown = 0.0f;
    float low = 0.0f;
    float peak = 0.0001f;

    for (int i = 0; i < THUNDER_SAMPLES; i++)
    {
        float t = (float)i / (float)THUNDER_RATE;

        /* Integrate toward brown, then lowpass again for weight. */
        brown = brown * 0.996f + WhiteNoise() * 0.06f;
        low = low + (brown - low) * rumble;

        float body = expf(-t * decay);
        float open = 1.0f - expf(-t * 26.0f);

        /* The initial crack, gone within a fifth of a second. */
        float snap = WhiteNoise() * crack * expf(-t * 22.0f);

        /* A slow wobble so it rolls instead of sitting still. */
        float roll = 0.82f + 0.18f * sinf(t * 3.1f + sinf(t * 0.7f) * 2.0f);

        work[i] = (low * 7.5f + snap) * body * open * roll;

        float mag = fabsf(work[i]);
        if (mag > peak) peak = mag;
    }

    /* Normalise rather than guessing a gain. Clamping a too-hot signal
       does not sound like thunder, it sounds like distortion. */
    float gain = 0.85f / peak;

    for (int i = 0; i < THUNDER_SAMPLES; i++)
    {
        samples[i] = (short)(work[i] * gain * 32000.0f);
    }

    Wave wave = {
        .frameCount = (unsigned int)THUNDER_SAMPLES,
        .sampleRate = THUNDER_RATE,
        .sampleSize = 16,
        .channels = 1,
        .data = samples,
    };

    return LoadSoundFromWave(wave);
}

void AudioLoad(void)
{
    const char *path = AssetPath(RAIN_ASSET);

    if (FileExists(path))
    {
        sRain = LoadMusicStream(path);
        sRainReady = (sRain.stream.buffer != NULL);

        if (sRainReady)
        {
            sRain.looping = true;
            SetMusicVolume(sRain, 0.0f);
            PlayMusicStream(sRain);
        }
    }
    else
    {
        TraceLog(LOG_WARNING, "AUDIO: %s not found, running silent", path);
    }

    /* Three rumbles so repeats are not obvious: near and sharp through
       to distant and soft. */
    /* Near strikes crack and end quickly; distant ones are dull and roll
       on much longer. */
    sThunder[0] = GenerateThunder(0.55f, 0.055f, 1.60f);
    sThunder[1] = GenerateThunder(0.26f, 0.030f, 1.10f);
    sThunder[2] = GenerateThunder(0.08f, 0.016f, 0.72f);
    sThunderReady = true;

    /* Lower and longer than thunder, with a slow throb rather than a
       crack. Same technique: no second file to ship. */
    sRoar = GenerateThunder(0.05f, 0.010f, 0.95f);
    sRoarReady = true;
}

void AudioUnload(void)
{
    if (sRainReady) UnloadMusicStream(sRain);

    if (sThunderReady)
    {
        for (int i = 0; i < THUNDER_VARIANTS; i++) UnloadSound(sThunder[i]);
    }

    if (sRoarReady) UnloadSound(sRoar);
    sRoarReady = false;

    sRainReady = false;
    sThunderReady = false;
}

void AudioUpdate(void)
{
    float dt = GetFrameTime();

    /* --- rain bed ----------------------------------------------------- */
    float want = WeatherRain();
    if (want > 1.0f) want = 1.0f;

    sRainLevel += (want - sRainLevel) * RAIN_FADE * dt;
    if (sRainLevel < 0.0f) sRainLevel = 0.0f;

    if (sRainReady)
    {
        float volume = sRainLevel * gSettings.sfxVolume;

        /* Stop streaming entirely when it is dry, rather than decoding a
           silent file forever. */
        if (volume < 0.005f)
        {
            if (IsMusicStreamPlaying(sRain)) PauseMusicStream(sRain);
        }
        else
        {
            if (!IsMusicStreamPlaying(sRain)) ResumeMusicStream(sRain);

            SetMusicVolume(sRain, volume);
            UpdateMusicStream(sRain);
        }
    }

    /* --- thunder ------------------------------------------------------ */
    float loudness = 0.0f;

    if (sThunderReady && WeatherConsumeThunder(&loudness))
    {
        /* Distant strikes pick the softer, duller rumbles. */
        int variant = (loudness > 0.75f) ? 0 : ((loudness > 0.45f) ? 1 : 2);

        Sound s = sThunder[variant];

        SetSoundVolume(s, loudness * gSettings.sfxVolume);
        SetSoundPitch(s, 0.85f + loudness * 0.3f);
        PlaySound(s);
    }

    /* --- something has decided to come and find you ------------------- */
    float roar = 0.0f;

    if (sRoarReady && StalkerConsumeRoar(&roar))
    {
        SetSoundVolume(sRoar, (0.35f + roar * 0.65f) * gSettings.sfxVolume);
        SetSoundPitch(sRoar, 0.55f + roar * 0.15f);   /* well below thunder */
        PlaySound(sRoar);
    }
}
