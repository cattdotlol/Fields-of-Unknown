#include "core/input.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define GAMEPAD 0
#define STICK_DEADZONE 0.35f

static int  sKeys[ACT_COUNT][INPUT_MAX_BINDINGS];
static int  sPads[ACT_COUNT];      /* one gamepad button per action, -1 for none */

static bool sDown[ACT_COUNT];
static bool sPrev[ACT_COUNT];

static void Bind3(InputAction a, int k0, int k1, int k2, int pad)
{
    sKeys[a][0] = k0;
    sKeys[a][1] = k1;
    sKeys[a][2] = k2;
    sPads[a] = pad;
}

void InputResetDefaults(void)
{
    memset(sKeys, 0, sizeof(sKeys));
    memset(sDown, 0, sizeof(sDown));
    memset(sPrev, 0, sizeof(sPrev));

    Bind3(ACT_LEFT,    KEY_A, KEY_LEFT,  0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    Bind3(ACT_RIGHT,   KEY_D, KEY_RIGHT, 0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    Bind3(ACT_UP,      KEY_W, KEY_UP,    0, GAMEPAD_BUTTON_LEFT_FACE_UP);
    Bind3(ACT_DOWN,    KEY_S, KEY_DOWN,  0, GAMEPAD_BUTTON_LEFT_FACE_DOWN);

    Bind3(ACT_RUN,     KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, 0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    Bind3(ACT_CROUCH,  KEY_LEFT_CONTROL, KEY_S, KEY_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    Bind3(ACT_JUMP,    KEY_SPACE, 0, 0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    Bind3(ACT_SENSE,   KEY_LEFT_ALT, KEY_Q, 0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    Bind3(ACT_EAT,     KEY_E, KEY_F, 0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);

    Bind3(ACT_CONFIRM, KEY_ENTER, KEY_KP_ENTER, KEY_SPACE, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    Bind3(ACT_CANCEL,  KEY_ESCAPE, 0, 0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    Bind3(ACT_DEBUG,   KEY_F1, 0, 0, -1);
}

void InputInit(void)
{
    InputResetDefaults();
}

static const char *ACTION_NAMES[ACT_COUNT] = {
    "MOVE LEFT", "MOVE RIGHT", "LOOK UP", "CROUCH / DOWN",
    "RUN", "SNEAK", "JUMP", "SENSE",
    "CONFIRM", "CANCEL", "DEBUG",
};

const char *InputActionName(InputAction action)
{
    if (action < 0 || action >= ACT_COUNT) return "?";

    return ACTION_NAMES[action];
}

bool InputActionRebindable(InputAction action)
{
    return (action != ACT_CONFIRM && action != ACT_CANCEL && action != ACT_DEBUG);
}

InputAction InputActionUsing(int key, InputAction ignore)
{
    if (key == 0) return ACT_COUNT;

    for (int a = 0; a < ACT_COUNT; a++)
    {
        if ((InputAction)a == ignore) continue;

        for (int i = 0; i < INPUT_MAX_BINDINGS; i++)
        {
            if (sKeys[a][i] == key) return (InputAction)a;
        }
    }

    return ACT_COUNT;
}

typedef struct KeyName { int key; const char *name; } KeyName;

/* raylib has no key-name function, so this is the table. Letters and
   digits fall through to their ASCII character. */
static const KeyName KEY_NAMES[] = {
    { KEY_SPACE, "SPACE" }, { KEY_APOSTROPHE, "'" }, { KEY_COMMA, "," },
    { KEY_MINUS, "-" }, { KEY_PERIOD, "." }, { KEY_SLASH, "/" },
    { KEY_SEMICOLON, ";" }, { KEY_EQUAL, "=" },
    { KEY_LEFT_BRACKET, "[" }, { KEY_BACKSLASH, "\\" },
    { KEY_RIGHT_BRACKET, "]" }, { KEY_GRAVE, "`" },
    { KEY_ESCAPE, "ESC" }, { KEY_ENTER, "ENTER" }, { KEY_TAB, "TAB" },
    { KEY_BACKSPACE, "BKSP" }, { KEY_INSERT, "INS" }, { KEY_DELETE, "DEL" },
    { KEY_RIGHT, "RIGHT" }, { KEY_LEFT, "LEFT" },
    { KEY_DOWN, "DOWN" }, { KEY_UP, "UP" },
    { KEY_PAGE_UP, "PGUP" }, { KEY_PAGE_DOWN, "PGDN" },
    { KEY_HOME, "HOME" }, { KEY_END, "END" },
    { KEY_CAPS_LOCK, "CAPS" }, { KEY_NUM_LOCK, "NUMLK" },
    { KEY_PRINT_SCREEN, "PRTSC" }, { KEY_PAUSE, "PAUSE" },
    { KEY_LEFT_SHIFT, "LSHIFT" }, { KEY_LEFT_CONTROL, "LCTRL" },
    { KEY_LEFT_ALT, "LALT" }, { KEY_LEFT_SUPER, "LSUPER" },
    { KEY_RIGHT_SHIFT, "RSHIFT" }, { KEY_RIGHT_CONTROL, "RCTRL" },
    { KEY_RIGHT_ALT, "RALT" }, { KEY_RIGHT_SUPER, "RSUPER" },
    { KEY_KP_ENTER, "KP ENTER" }, { KEY_KP_ADD, "KP +" },
    { KEY_KP_SUBTRACT, "KP -" }, { KEY_KP_MULTIPLY, "KP *" },
    { KEY_KP_DIVIDE, "KP /" }, { KEY_KP_DECIMAL, "KP ." },
};

#define KEY_NAME_COUNT ((int)(sizeof(KEY_NAMES) / sizeof(KEY_NAMES[0])))

const char *InputKeyName(int key)
{
    static char buffer[16];

    if (key == 0) return "--";

    for (int i = 0; i < KEY_NAME_COUNT; i++)
    {
        if (KEY_NAMES[i].key == key) return KEY_NAMES[i].name;
    }

    if (key >= KEY_F1 && key <= KEY_F12)
    {
        snprintf(buffer, sizeof(buffer), "F%d", key - KEY_F1 + 1);
        return buffer;
    }

    if (key >= KEY_KP_0 && key <= KEY_KP_9)
    {
        snprintf(buffer, sizeof(buffer), "KP %d", key - KEY_KP_0);
        return buffer;
    }

    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
    {
        buffer[0] = (char)key;
        buffer[1] = '\0';
        return buffer;
    }

    snprintf(buffer, sizeof(buffer), "#%d", key);
    return buffer;
}

static bool RawDown(InputAction a)
{
    for (int i = 0; i < INPUT_MAX_BINDINGS; i++)
    {
        if (sKeys[a][i] != 0 && IsKeyDown(sKeys[a][i])) return true;
    }

    if (sPads[a] >= 0 && IsGamepadAvailable(GAMEPAD) &&
        IsGamepadButtonDown(GAMEPAD, sPads[a])) return true;

    return false;
}

void InputPoll(void)
{
    memcpy(sPrev, sDown, sizeof(sDown));

    for (int a = 0; a < ACT_COUNT; a++) sDown[a] = RawDown((InputAction)a);

    /* Stick counts as a direction hold. */
    if (IsGamepadAvailable(GAMEPAD))
    {
        float ax = GetGamepadAxisMovement(GAMEPAD, GAMEPAD_AXIS_LEFT_X);
        float ay = GetGamepadAxisMovement(GAMEPAD, GAMEPAD_AXIS_LEFT_Y);

        if (ax < -STICK_DEADZONE) sDown[ACT_LEFT] = true;
        if (ax >  STICK_DEADZONE) sDown[ACT_RIGHT] = true;
        if (ay < -STICK_DEADZONE) sDown[ACT_UP] = true;
        if (ay >  STICK_DEADZONE) sDown[ACT_DOWN] = true;
    }
}

bool InputDown(InputAction action)
{
    return sDown[action];
}

/* NOTE: edges are frame-scoped, so a frame that runs two fixed ticks
   shows the same press to both. Anything reacting to a press inside the
   fixed step must be idempotent - see the cat's jump buffer. */
bool InputPressed(InputAction action)
{
    return sDown[action] && !sPrev[action];
}

bool InputReleased(InputAction action)
{
    return !sDown[action] && sPrev[action];
}

float InputAxisX(void)
{
    float x = 0.0f;

    if (sDown[ACT_LEFT])  x -= 1.0f;
    if (sDown[ACT_RIGHT]) x += 1.0f;

    return x;
}

float InputAxisY(void)
{
    float y = 0.0f;

    if (sDown[ACT_UP])   y -= 1.0f;
    if (sDown[ACT_DOWN]) y += 1.0f;

    return y;
}

void InputBind(InputAction action, int slot, int key)
{
    if (slot < 0 || slot >= INPUT_MAX_BINDINGS) return;

    sKeys[action][slot] = key;
}

int InputBinding(InputAction action, int slot)
{
    if (slot < 0 || slot >= INPUT_MAX_BINDINGS) return 0;

    return sKeys[action][slot];
}
