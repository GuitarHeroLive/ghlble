#include "gamepad.h"

Gamepad::Gamepad(const std::string& name)
{
    // Open uinput
    uinputHandle = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    // Configure the virtual device
    struct uinput_user_dev gamepad_configuration;
    memset(&gamepad_configuration, 0, sizeof(gamepad_configuration)); // Initialize memory
    snprintf(gamepad_configuration.name, UINPUT_MAX_NAME_SIZE, "%s", name.c_str()); // Gamepad name
    gamepad_configuration.id.bustype = BUS_USB; // Connected via USB
    gamepad_configuration.id.vendor  = 0x045e; // Microsoft
    gamepad_configuration.id.product = 0x028e; // Xbox 360 Controller
    gamepad_configuration.id.version = 1; // First version
    gamepad_configuration.absmin[AXIS_LEFT_ANALOG_HORIZONTAL] = ANALOG_VALUE_MIN; // Left Analog X
    gamepad_configuration.absmax[AXIS_LEFT_ANALOG_HORIZONTAL] = ANALOG_VALUE_MAX; // Left Analog X
    gamepad_configuration.absfuzz[AXIS_LEFT_ANALOG_HORIZONTAL] = ANALOG_VALUE_FUZZ; // Left Analog X
    gamepad_configuration.absflat[AXIS_LEFT_ANALOG_HORIZONTAL] = ANALOG_VALUE_FLAT; // Left Analog X
    gamepad_configuration.absmin[AXIS_LEFT_ANALOG_VERTICAL] = ANALOG_VALUE_MIN; // Left Analog Y
    gamepad_configuration.absmax[AXIS_LEFT_ANALOG_VERTICAL] = ANALOG_VALUE_MAX; // Left Analog Y
    gamepad_configuration.absfuzz[AXIS_LEFT_ANALOG_VERTICAL] = ANALOG_VALUE_FUZZ; // Left Analog Y
    gamepad_configuration.absflat[AXIS_LEFT_ANALOG_VERTICAL] = ANALOG_VALUE_FLAT; // Left Analog Y
    gamepad_configuration.absmin[AXIS_LEFT_TRIGGER] = TRIGGER_VALUE_MIN; // Left Trigger
    gamepad_configuration.absmax[AXIS_LEFT_TRIGGER] = TRIGGER_VALUE_MAX; // Left Trigger
    gamepad_configuration.absfuzz[AXIS_LEFT_TRIGGER] = TRIGGER_VALUE_FUZZ; // Left Trigger
    gamepad_configuration.absflat[AXIS_LEFT_TRIGGER] = TRIGGER_VALUE_FLAT; // Left Trigger
    gamepad_configuration.absmin[AXIS_RIGHT_ANALOG_HORIZONTAL] = ANALOG_VALUE_MIN; // Right Analog X
    gamepad_configuration.absmax[AXIS_RIGHT_ANALOG_HORIZONTAL] = ANALOG_VALUE_MAX; // Right Analog X
    gamepad_configuration.absfuzz[AXIS_RIGHT_ANALOG_HORIZONTAL] = ANALOG_VALUE_FUZZ; // Right Analog X
    gamepad_configuration.absflat[AXIS_RIGHT_ANALOG_HORIZONTAL] = ANALOG_VALUE_FLAT; // Right Analog X
    gamepad_configuration.absmin[AXIS_RIGHT_ANALOG_VERTICAL] = ANALOG_VALUE_MIN; // Right Analog Y
    gamepad_configuration.absmax[AXIS_RIGHT_ANALOG_VERTICAL] = ANALOG_VALUE_MAX; // Right Analog Y
    gamepad_configuration.absfuzz[AXIS_RIGHT_ANALOG_VERTICAL] = ANALOG_VALUE_FUZZ; // Right Analog Y
    gamepad_configuration.absflat[AXIS_RIGHT_ANALOG_VERTICAL] = ANALOG_VALUE_FLAT; // Right Analog Y
    gamepad_configuration.absmin[AXIS_RIGHT_TRIGGER] = TRIGGER_VALUE_MIN; // Right Trigger
    gamepad_configuration.absmax[AXIS_RIGHT_TRIGGER] = TRIGGER_VALUE_MAX; // Right Trigger
    gamepad_configuration.absfuzz[AXIS_RIGHT_TRIGGER] = TRIGGER_VALUE_FUZZ; // Right Trigger
    gamepad_configuration.absflat[AXIS_RIGHT_TRIGGER] = TRIGGER_VALUE_FLAT; // Right Trigger
    gamepad_configuration.absmin[AXIS_DPAD_HORIZONTAL] = DPAD_VALUE_MIN; // Dpad X
    gamepad_configuration.absmax[AXIS_DPAD_HORIZONTAL] = DPAD_VALUE_MAX; // Dpad X
    gamepad_configuration.absfuzz[AXIS_DPAD_HORIZONTAL] = DPAD_VALUE_FUZZ; // Dpad X
    gamepad_configuration.absflat[AXIS_DPAD_HORIZONTAL] = DPAD_VALUE_FLAT; // Dpad X
    gamepad_configuration.absmin[AXIS_DPAD_VERTICAL] = DPAD_VALUE_MIN; // Dpad Y
    gamepad_configuration.absmax[AXIS_DPAD_VERTICAL] = DPAD_VALUE_MAX; // Dpad Y
    gamepad_configuration.absfuzz[AXIS_DPAD_VERTICAL] = DPAD_VALUE_FUZZ; // Dpad Y
    gamepad_configuration.absflat[AXIS_DPAD_VERTICAL] = DPAD_VALUE_FLAT; // Dpad Y
    ioctl(uinputHandle, UI_SET_EVBIT, EV_KEY); // This gamepad has buttons
    ioctl(uinputHandle, UI_SET_EVBIT, EV_ABS); // This gamepad has axes
    ioctl(uinputHandle, UI_SET_EVBIT, EV_SYN); // This gamepad sends SYN events
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_SOUTH); // A
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_EAST); // B
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_NORTH); // Y
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_WEST); // X
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_SELECT); // SELECT
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_START); // START
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_THUMBL); // L3
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_THUMBR); // R3
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_MODE); // HOME/BACK
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_TL); // L1
    ioctl(uinputHandle, UI_SET_KEYBIT, BTN_TR); // R1
    ioctl(uinputHandle, UI_SET_ABSBIT, AXIS_LEFT_ANALOG_HORIZONTAL); // Left Analog X
    ioctl(uinputHandle, UI_SET_ABSBIT, AXIS_LEFT_ANALOG_VERTICAL); // Left Analog Y
    ioctl(uinputHandle, UI_SET_ABSBIT, AXIS_LEFT_TRIGGER); // Left Trigger
    ioctl(uinputHandle, UI_SET_ABSBIT, AXIS_RIGHT_ANALOG_HORIZONTAL); // Right Analog X
    ioctl(uinputHandle, UI_SET_ABSBIT, AXIS_RIGHT_ANALOG_VERTICAL); // Right Analog Y
    ioctl(uinputHandle, UI_SET_ABSBIT, AXIS_RIGHT_TRIGGER); // Right Trigger
    ioctl(uinputHandle, UI_SET_ABSBIT, AXIS_DPAD_HORIZONTAL); // Dpad X
    ioctl(uinputHandle, UI_SET_ABSBIT, AXIS_DPAD_VERTICAL); // Dpad Y

    // Create the virtual device
    write(uinputHandle, &gamepad_configuration, sizeof(gamepad_configuration));
    ioctl(uinputHandle, UI_DEV_CREATE);
}

Gamepad::~Gamepad()
{
    // Destroy our merged gamepad
    ioctl(uinputHandle, UI_DEV_DESTROY);

    // Close uinput
    close(uinputHandle);
}

void Gamepad::update(struct input_event * ev)
{
    // Writes the event into the virtual gamepad
    write(uinputHandle, ev, sizeof(*ev));

    struct input_event syn;
    gettimeofday(&syn.time, nullptr);
    syn.type = EV_SYN;
    syn.code = SYN_REPORT;
    syn.value = 0;
    write(uinputHandle, &syn, sizeof(syn));
}
