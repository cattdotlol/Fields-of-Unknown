#include "core/sysinfo.h"

#include "raylib.h"

#include "core/sysinfo_win32.h"

#if defined(_WIN32)
    /* GL/gl.h needs windows.h, and windows.h collides with raylib.h.
       glGetString is the only entry point wanted here, so it is declared
       directly - opengl32 is already linked. */
    typedef unsigned int GLenum;
    typedef unsigned char GLubyte;
    __declspec(dllimport) const GLubyte * __stdcall glGetString(GLenum name);

    #define GL_VENDOR   0x1F00
    #define GL_RENDERER 0x1F01
    #define GL_VERSION  0x1F02
#elif defined(__APPLE__)
    /* Apple shipped OpenGL as deprecated years ago; it still works, and
       raylib still uses it. This just quiets the header. */
    #define GL_SILENCE_DEPRECATION
    #include <OpenGL/gl.h>
    #include <sys/sysctl.h>
#else
    #include <GL/gl.h>
#endif

#include <stdio.h>
#include <string.h>

#ifndef GL_SHADING_LANGUAGE_VERSION
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#endif

SysInfo gSysInfo;

static void CopyInto(char *dst, size_t cap, const char *src, const char *fallback)
{
    const char *text = (src != NULL && src[0] != '\0') ? src : fallback;

    strncpy(dst, text, cap - 1);
    dst[cap - 1] = '\0';
}

/* Trims leading spaces and the trailing newline left by fgets. */
static void Trim(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == ' ')) s[--len] = '\0';

    size_t lead = 0;
    while (s[lead] == ' ' || s[lead] == '\t') lead++;
    if (lead > 0) memmove(s, s + lead, strlen(s + lead) + 1);
}

#if defined(_WIN32)

static void ReadCpu(void)
{
    gSysInfo.cpuThreads = SysWin32Threads();
    SysWin32CpuName(gSysInfo.cpu, (int)sizeof(gSysInfo.cpu));

    if (gSysInfo.cpu[0] != '\0') Trim(gSysInfo.cpu);
}

static void ReadMemory(void)
{
    SysWin32Memory(gSysInfo.memory, (int)sizeof(gSysInfo.memory));
}

#elif defined(__APPLE__)

static void ReadCpu(void)
{
    size_t size = sizeof(gSysInfo.cpu);

    if (sysctlbyname("machdep.cpu.brand_string", gSysInfo.cpu, &size, NULL, 0) == 0)
    {
        /* Intel brand strings come back padded. Trimming here also keeps
           the helper used on every platform - it is defined
           unconditionally, so leaving it uncalled is a warning. */
        Trim(gSysInfo.cpu);
    }
    else
    {
        gSysInfo.cpu[0] = '\0';
    }

    int logical = 0;
    size = sizeof(logical);

    if (sysctlbyname("hw.logicalcpu", &logical, &size, NULL, 0) == 0)
    {
        gSysInfo.cpuThreads = logical;
    }
}

static void ReadMemory(void)
{
    unsigned long long bytes = 0;
    size_t size = sizeof(bytes);

    if (sysctlbyname("hw.memsize", &bytes, &size, NULL, 0) == 0)
    {
        snprintf(gSysInfo.memory, sizeof(gSysInfo.memory), "%.1f GB",
                 (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

#elif defined(__linux__)
static void ReadCpu(void)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f == NULL) return;

    char line[256];
    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (gSysInfo.cpu[0] == '\0' && strncmp(line, "model name", 10) == 0)
        {
            char *colon = strchr(line, ':');
            if (colon != NULL)
            {
                Trim(colon + 1);
                CopyInto(gSysInfo.cpu, sizeof(gSysInfo.cpu), colon + 1, "unknown");
            }
        }

        if (strncmp(line, "processor", 9) == 0) gSysInfo.cpuThreads++;
    }

    fclose(f);
}

static void ReadMemory(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (f == NULL) return;

    char line[256];
    long kb = 0;
    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (sscanf(line, "MemTotal: %ld kB", &kb) == 1) break;
    }

    fclose(f);

    if (kb > 0) snprintf(gSysInfo.memory, sizeof(gSysInfo.memory), "%.1f GB", kb / 1048576.0);
}
#endif /* platform */

void SysInfoRefreshDisplay(void)
{
    int monitor = GetCurrentMonitor();
    const char *name = GetMonitorName(monitor);

    snprintf(gSysInfo.display, sizeof(gSysInfo.display), "%s  %dx%d @ %dHz",
             (name != NULL) ? name : "Display",
             GetMonitorWidth(monitor), GetMonitorHeight(monitor),
             GetMonitorRefreshRate(monitor));
}

void SysInfoGather(void)
{
    CopyInto(gSysInfo.cpu, sizeof(gSysInfo.cpu), NULL, "unknown");
    CopyInto(gSysInfo.memory, sizeof(gSysInfo.memory), NULL, "unknown");

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    gSysInfo.cpu[0] = '\0';
    gSysInfo.cpuThreads = 0;
    ReadCpu();
    ReadMemory();
    if (gSysInfo.cpu[0] == '\0') CopyInto(gSysInfo.cpu, sizeof(gSysInfo.cpu), NULL, "unknown");
#endif

    /* GL 1.1 entry points, exported by libGL - no extension loading needed. */
    CopyInto(gSysInfo.gpu, sizeof(gSysInfo.gpu),
             (const char *)glGetString(GL_RENDERER), "unknown GPU");
    CopyInto(gSysInfo.glVersion, sizeof(gSysInfo.glVersion),
             (const char *)glGetString(GL_VERSION), "unknown");
    CopyInto(gSysInfo.glslVersion, sizeof(gSysInfo.glslVersion),
             (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION), "unknown");

    CopyInto(gSysInfo.raylibVersion, sizeof(gSysInfo.raylibVersion), RAYLIB_VERSION, "?");

    SysInfoRefreshDisplay();
}
