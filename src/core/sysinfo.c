#include "core/sysinfo.h"

#include "raylib.h"

/* windows.h must come first: GL/gl.h on Windows uses APIENTRY and
   WINGDIAPI from it and will not compile on its own. */
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include <GL/gl.h>

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
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    gSysInfo.cpuThreads = (int)info.dwNumberOfProcessors;

    /* The friendly CPU name only exists in the registry. */
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        DWORD size = (DWORD)sizeof(gSysInfo.cpu);
        DWORD type = 0;

        if (RegQueryValueExA(key, "ProcessorNameString", NULL, &type,
                             (LPBYTE)gSysInfo.cpu, &size) == ERROR_SUCCESS &&
            type == REG_SZ)
        {
            gSysInfo.cpu[sizeof(gSysInfo.cpu) - 1] = '\0';
            Trim(gSysInfo.cpu);
        }

        RegCloseKey(key);
    }
}

static void ReadMemory(void)
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);

    if (GlobalMemoryStatusEx(&status))
    {
        snprintf(gSysInfo.memory, sizeof(gSysInfo.memory), "%.1f GB",
                 (double)status.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
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

#if defined(_WIN32) || defined(__linux__)
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
