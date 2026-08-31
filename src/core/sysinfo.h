#ifndef CORE_SYSINFO_H
#define CORE_SYSINFO_H

typedef struct SysInfo {
    char cpu[128];
    int  cpuThreads;
    char memory[32];
    char gpu[192];
    char glVersion[96];
    char glslVersion[96];
    char display[96];
    char raylibVersion[16];
} SysInfo;

extern SysInfo gSysInfo;

/* Must be called after InitWindow: the GPU strings need a live GL context. */
void SysInfoGather(void);

/* Cheap; the display line changes when the window moves between monitors. */
void SysInfoRefreshDisplay(void);

#endif /* CORE_SYSINFO_H */
