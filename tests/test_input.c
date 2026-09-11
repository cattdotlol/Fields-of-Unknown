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
    Check("escape is reserved", InputKeyReserved(KEY_ESCAPE), true);
#if !defined(NDEBUG)
    Check("debug reroll cannot be rebound", InputKeyReserved(KEY_F5), true);
#else
    Check("release has no reserved reroll key", InputKeyReserved(KEY_F5), false);
#endif
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

/* The whole reason nothing asks for a key: the source can be swapped.
   Half the underwater behaviour is only testable because of this, and a
   replay would need the same door. */
static void TestSomethingOtherThanAKeyboardCanDriveIt(void)
{
    Check("nothing is driving it to begin with", InputScripted(), false);

    InputScriptBegin();

    Check("now something is", InputScripted(), true);

    InputScriptHold(ACT_RIGHT, true);
    InputPoll();

    Check("a held action reads as held", InputDown(ACT_RIGHT), true);
    Check("and as a press, once", InputPressed(ACT_RIGHT), true);
    Check("the axis follows it", InputAxisX() > 0.0f, true);
    Check("and nothing else is held", InputDown(ACT_LEFT), false);

    InputPoll();

    Check("holding it is not pressing it again", InputPressed(ACT_RIGHT), false);
    Check("but it is still down", InputDown(ACT_RIGHT), true);

    InputScriptRelease();
    InputPoll();

    Check("letting go lets go", InputDown(ACT_RIGHT), false);
    Check("and reads as a release", InputReleased(ACT_RIGHT), true);

    InputScriptEnd();
    InputPoll();

    Check("and the devices have it back", InputScripted(), false);
    Check("with nothing left held", InputDown(ACT_RIGHT), false);
}

static void TestSimulationPresses(void)
{
    InputScriptBegin();
    InputScriptHold(ACT_JUMP, true);
    InputScriptHold(ACT_EAT, true);
    InputPoll();
    /* A render frame without a tick, followed by a release. */
    InputScriptRelease();
    InputPoll();
    Check("jump survives a frame without simulation",
          InputConsumePressed(ACT_JUMP), true);
    Check("eat survives a frame without simulation",
          InputConsumePressed(ACT_EAT), true);
    Check("catch-up ticks do not repeat jump", InputConsumePressed(ACT_JUMP), false);
    Check("catch-up ticks do not repeat eat", InputConsumePressed(ACT_EAT), false);

    InputScriptHold(ACT_EAT, true);
    InputPoll();
    Check("a new press can be consumed", InputConsumePressed(ACT_EAT), true);
    InputPoll();
    Check("holding does not queue another press", InputConsumePressed(ACT_EAT), false);

    InputScriptHold(ACT_JUMP, true);
    InputPoll();
    InputClearPending();
    Check("transitions discard pending gameplay input",
          InputConsumePressed(ACT_JUMP), false);
    InputScriptEnd();
}

void SuiteInput(void)
{
    TestDefaults();
    TestEveryActionIsNamed();
    TestKeyNames();
    TestConflictDetection();
    TestBindingsSurviveASave();
    TestSomethingOtherThanAKeyboardCanDriveIt();
    TestSimulationPresses();
}
