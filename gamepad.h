#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <string>
#include <iostream>

// The button states
enum GamepadButtonStates
{
    BTN_RELEASED = 0,
    BTN_PRESSED = 1,
    BTN_REPEAT = 2
};

enum GamepadButtons
{
    BTN_XINPUT_B = 0x130,
    BTN_XINPUT_X = 0x131,
    BTN_XINPUT_Y = 0x132,
    BTN_XINPUT_L1 = 0x133,
    BTN_XINPUT_R1 = 0x134,
    BTN_XINPUT_BACK = 0x135,
    BTN_XINPUT_START = 0x136,
    BTN_XINPUT_GUIDE = 0x137,
    BTN_XINPUT_L3 = 0x13a,
    BTN_XINPUT_R3 = 0x13b,
};

// XBOX360 gamepad axis
enum GamepadAxis
{
    AXIS_LEFT_ANALOG_HORIZONTAL = 0,
    AXIS_LEFT_ANALOG_VERTICAL = 1,
    AXIS_LEFT_TRIGGER = 2,
    AXIS_RIGHT_ANALOG_HORIZONTAL = 3,
    AXIS_RIGHT_ANALOG_VERTICAL = 4,
    AXIS_RIGHT_TRIGGER = 5,
    AXIS_DPAD_HORIZONTAL = 16,
    AXIS_DPAD_VERTICAL = 17
};

// Analog stick axis range, fuzz and flat values
#define ANALOG_VALUE_MIN -32767
#define ANALOG_VALUE_MAX 32768
#define ANALOG_VALUE_FUZZ 64
#define ANALOG_VALUE_FLAT 4096

// Dpad axis range, fuzz and flat values
#define DPAD_VALUE_MIN -1
#define DPAD_VALUE_MAX 1
#define DPAD_VALUE_FUZZ 0
#define DPAD_VALUE_FLAT 0

// Trigger axis range, fuzz and flat values
#define TRIGGER_VALUE_MIN 0
#define TRIGGER_VALUE_MAX 255
#define TRIGGER_VALUE_FUZZ 0
#define TRIGGER_VALUE_FLAT 0

class Gamepad {
private:
    // The uinput handle
    int uinputHandle;

public:
    // Constructor
    Gamepad(const std::string& name);

    // Destructor
    ~Gamepad();

    // Feeds the gamepad a new input event
    void update(struct input_event * ev);
};

#endif // GAMEPAD_H
