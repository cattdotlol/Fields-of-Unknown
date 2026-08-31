#ifndef CORE_SYSINFO_WIN32_H
#define CORE_SYSINFO_WIN32_H

/* windows.h and raylib.h cannot both be included in one file: they each
   define Rectangle, CloseWindow and ShowCursor with different meanings.
   The Win32 queries therefore live in their own translation unit, which
   never sees raylib. */

#if defined(_WIN32)

int  SysWin32Threads(void);
void SysWin32CpuName(char *out, int capacity);
void SysWin32Memory(char *out, int capacity);

#endif

#endif /* CORE_SYSINFO_WIN32_H */
