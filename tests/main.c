#include "tests.h"
#include "entity/cat.h"

#include "raylib.h"

#include <stdio.h>

static int sFailures;

void Check(const char *name, bool got, bool expected)
{
    bool ok = (got == expected);
    if (!ok) sFailures++;

    printf("  %-46s %s\n", name, ok ? "ok" : "FAIL");
}

int main(void)
{
    SetTraceLogLevel(LOG_ERROR);

    printf("cat reach: %.1f up, %.1f across\n\n",
           (double)CatMaxJumpHeight(), (double)CatMaxRunJumpDistance());

    SuiteWorldgen();
    SuiteVitals();
    SuiteMushroom();

    printf("\n%s\n", sFailures ? "FAILED" : "all passed");
    return sFailures ? 1 : 0;
}
