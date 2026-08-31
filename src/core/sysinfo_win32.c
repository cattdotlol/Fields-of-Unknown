#include "core/sysinfo_win32.h"

/* Empty translation units are not valid C, and this file is compiled on
   every platform because the build globs src/. */
typedef int SysWin32TranslationUnitNotEmpty;

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

int SysWin32Threads(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    return (int)info.dwNumberOfProcessors;
}

void SysWin32CpuName(char *out, int capacity)
{
    if (out == NULL || capacity <= 0) return;

    out[0] = '\0';

    /* The friendly name only exists in the registry. */
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        return;
    }

    DWORD size = (DWORD)capacity;
    DWORD type = 0;

    if (RegQueryValueExA(key, "ProcessorNameString", NULL, &type,
                         (LPBYTE)out, &size) != ERROR_SUCCESS || type != REG_SZ)
    {
        out[0] = '\0';
    }

    out[capacity - 1] = '\0';
    RegCloseKey(key);
}

void SysWin32Memory(char *out, int capacity)
{
    if (out == NULL || capacity <= 0) return;

    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);

    if (GlobalMemoryStatusEx(&status))
    {
        snprintf(out, (size_t)capacity, "%.1f GB",
                 (double)status.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
    }
    else
    {
        out[0] = '\0';
    }
}

#endif /* _WIN32 */
