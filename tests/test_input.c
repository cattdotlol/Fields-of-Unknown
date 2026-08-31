/* Bindings are written into the same file as everything else, so the
   round trip is worth pinning: a player who remaps their keys and quits
   should not find them back to defaults. */

#include "tests.h"

#include "core/input.h"
#include "core/settings.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define TMP "/tmp/fou_settings_test.cfg"

static void TestDefaults(void)
{
    puts("input");

    InputResetDefaults();

    Check("movement is bound out of the box", InputBinding(ACT_LEFT, 0) != 0, true);
    Check("jump is bound out of the box", InputBinding(ACT_JUMP, 0) == KEY_SPACE, true);
    Check("confirm is not rebindable", InputActionRebindable(ACT_CONFIRM), false);
    Check("cancel is not rebindable", InputActionRebindable(ACT_CANCEL), false);
    Check("jumping is rebindable", InputActionRebindable(ACT_JUMP), true);
}

/* Every action needs a label, or the controls screen shows a neighbour's
   name - or nothing at all. */
static void TestEveryActionIsNamed(void)
{
    int unnamed = 0, duplicated = 0;

    for (int a = 0; a < ACT_COUNT; a++)
    {
        const char *name = InputActionName((InputAction)a);

        if (name == NULL || name[0] == '\0' || strcmp(name, "?") == 0) unnamed++;

        for (int b = a + 1; b < ACT_COUNT; b++)
        {
            if (strcmp(name, InputActionName((InputAction)b)) == 0) duplicated++;
        }
    }

    Check("every action has a name", unnamed == 0, true);
    Check("no two actions share a name", duplicated == 0, true);
    Check("eat is named correctly",
          strcmp(InputActionName(ACT_EAT), "EAT") == 0, true);
}

static void TestKeyNames(void)
{
    Check("letters name themselves", strcmp(InputKeyName(KEY_A), "A") == 0, true);
    Check("space has a name", strcmp(InputKeyName(KEY_SPACE), "SPACE") == 0, true);
    Check("function keys are numbered", strcmp(InputKeyName(KEY_F5), "F5") == 0, true);
    Check("an empty slot reads as empty", strcmp(InputKeyName(0), "--") == 0, true);
}

/* A key must only ever mean one thing. */
static void TestConflictDetection(void)
{
    InputResetDefaults();

    Check("space is seen to be taken by jump",
          InputActionUsing(KEY_SPACE, ACT_LEFT) == ACT_JUMP, true);
    Check("but not when jump is the one asking",
          InputActionUsing(KEY_SPACE, ACT_JUMP) == ACT_CONFIRM, true);

    InputBind(ACT_JUMP, 0, KEY_J);
    Check("an unused key comes back free",
          InputActionUsing(KEY_SPACE, ACT_JUMP) == ACT_CONFIRM, true);
}

static void TestBindingsSurviveASave(void)
{
    InputResetDefaults();
    SettingsDefaults();

    InputBind(ACT_JUMP, 0, KEY_Z);
    InputBind(ACT_RUN, 1, KEY_X);
    InputBind(ACT_EAT, 0, KEY_Q);

    Check("saving works", SettingsSave(TMP), true);

    InputResetDefaults();
    Check("defaults really were restored", InputBinding(ACT_JUMP, 0) == KEY_SPACE, true);

    Check("loading works", SettingsLoad(TMP), true);

    Check("a remapped jump comes back", InputBinding(ACT_JUMP, 0) == KEY_Z, true);
    Check("a remapped second slot comes back", InputBinding(ACT_RUN, 1) == KEY_X, true);
    Check("a remapped eat comes back", InputBinding(ACT_EAT, 0) == KEY_Q, true);

    remove(TMP);
    InputResetDefaults();
}

void SuiteInput(void)
{
    TestDefaults();
    TestEveryActionIsNamed();
    TestKeyNames();
    TestConflictDetection();
    TestBindingsSurviveASave();
}
