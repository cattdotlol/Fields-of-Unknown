#ifndef CORE_INPUT_H
#define CORE_INPUT_H

#include <stdbool.h>

/* Everything asks for an action, never a key. Rebinding, gamepads and
   replay all need this single choke point. */

typedef enum InputAction {
    ACT_LEFT = 0,
    ACT_RIGHT,
    ACT_UP,
    ACT_DOWN,
    ACT_RUN,
    ACT_CROUCH,
    ACT_JUMP,
    ACT_SENSE,        /* the smell/sound view */
    ACT_EAT,
    ACT_CONFIRM,
    ACT_CANCEL,
    ACT_DEBUG,
    ACT_COUNT
} InputAction;

#define INPUT_MAX_BINDINGS 3

void InputInit(void);

/* Sample once per frame, before any update. */
void InputPoll(void);

bool InputDown(InputAction action);
bool InputPressed(InputAction action);
bool InputReleased(InputAction action);

/* -1, 0 or +1, keyboard or stick. */
float InputAxisX(void);
float InputAxisY(void);

void InputBind(InputAction action, int slot, int key);
int  InputBinding(InputAction action, int slot);
void InputResetDefaults(void);

/* Names for the rebinding screen. */
const char *InputActionName(InputAction action);
const char *InputKeyName(int key);

/* Confirm, cancel and the debug toggle stay put: rebinding the key that
   cancels a rebind is a good way to lock someone out of their own menu. */
bool InputActionRebindable(InputAction action);

/* Whichever action already uses this key, or ACT_COUNT if it is free. */
InputAction InputActionUsing(int key, InputAction ignore);

#endif /* CORE_INPUT_H */
