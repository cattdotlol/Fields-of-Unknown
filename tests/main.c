#include "tests.h"
#include "entity/cat.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

static int sFailures;

void Check(const char *name, bool got, bool expected)
{
    bool ok = (got == expected);
    if (!ok) sFailures++;

    printf("  %-46s %s\n", name, ok ? "ok" : "FAIL");
}

static const struct { const char *name; void (*run)(void); } SUITES[] = {
    { "worldgen", SuiteWorldgen }, { "vitals", SuiteVitals },
    { "mushroom", SuiteMushroom }, { "input", SuiteInput },
    { "rat", SuiteRat }, { "stalker", SuiteStalker },
    { "physics", SuitePhysics }, { "district", SuiteDistrict },
    { "tree", SuiteTree }, { "daylight", SuiteDaylight },
    { "ocean", SuiteOcean }, { "aquatic", SuiteAquatic },
    { "species", SuiteSpecies }, { "creatures", SuiteCreatures },
    { "agents", SuiteAgents }, { "timestep", SuiteTimeStep },
};

int main(int argc, char **argv)
{
    int count = (int)(sizeof(SUITES) / sizeof(SUITES[0]));
    int selected = -1;
    if (argc == 2 && strcmp(argv[1], "--list") == 0)
    {
        for (int i = 0; i < count; i++) puts(SUITES[i].name);
        return 0;
    }
    if (argc > 2)
    {
        fprintf(stderr, "usage: %s [suite|--list]\n", argv[0]);
        return 2;
    }
    if (argc == 2)
    {
        for (int i = 0; i < count; i++)
            if (strcmp(argv[1], SUITES[i].name) == 0) selected = i;
        if (selected < 0)
        {
            fprintf(stderr, "unknown suite: %s (use --list)\n", argv[1]);
            return 2;
        }
    }
    SetTraceLogLevel(LOG_ERROR);

    printf("cat reach: %.1f up, %.1f across\n\n",
           (double)CatMaxJumpHeight(), (double)CatMaxRunJumpDistance());

    for (int i = 0; i < count; i++)
        if (selected < 0 || selected == i) SUITES[i].run();

    printf("\n%s\n", sFailures ? "FAILED" : "all passed");
    return sFailures ? 1 : 0;
}
