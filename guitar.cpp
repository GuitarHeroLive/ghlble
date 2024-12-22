#include "guitar.h"

Guitar::Guitar(gattlib_adapter_t* adapterValue, const std::string& addressValue) : adapter(adapterValue), address(addressValue)
{
    // Create the input thread
    thread = std::thread(&Guitar::startConnection, this);
}

Guitar::~Guitar()
{
    // We're still connected to this guitar
    if (connection != NULL)
    {
        // Disconnect the guitar
        gattlib_disconnect(connection, false);

        // Reset the connection pointer
        connection = NULL;
    }

    // The thread hasn't finished yet
    if (thread.joinable())
    {
        // Wait for the thread to finish
        thread.join();
    }
}

std::string Guitar::getAddress() const
{
    // Return the MAC address
    return address;
}

void Guitar::startConnection()
{
    // Log the connection start
    printf("Started the connection to the guitar with the MAC address %s.\n", address.c_str());

    // Start receiving data from the guitar
    if (gattlib_connect(adapter, address.c_str(), GATTLIB_CONNECTION_OPTIONS_NONE, &Guitar::receiveData, this) != GATTLIB_SUCCESS)
    {
        // The connection to the guitar couldn't be established or was lost
        printf("The connection to the guitar with the MAC address %s was lost.\n", address.c_str());
    }
}

void Guitar::receiveData(gattlib_adapter_t* adapter, const char *dst, gattlib_connection_t* connection, int error, void* user_data)
{
    // Cast the guitar object
    Guitar* guitar = (Guitar*)user_data;

    // Log the receive loop start
    printf("Entered receiveData for the guitar with the MAC address %s.\n", guitar->address.c_str());

    // The guitar's characteristics
    gattlib_characteristic_t* characteristics;

    // The number of characteristics
    int characteristics_count;

    // Discover the guitar's characteristics
    if (gattlib_discover_char(connection, &characteristics, &characteristics_count) == GATTLIB_SUCCESS)
    {
        // Iterate the characteristics
        for (int i = 0; i < characteristics_count; i++)
        {
            // The characteristic's UUID
            char uuid_str[MAX_LEN_UUID_STR + 1];

            // We've found the characteristic that reports guitar data
            if (gattlib_uuid_to_string(&characteristics[i].uuid, uuid_str, sizeof(uuid_str)) == GATTLIB_SUCCESS && strcmp(uuid_str, "533e1524-3abe-f33f-cd00-594e8b0a8ea3") == 0)
            {
                // Log the characteristic UUID
                printf("Found the primary characteristic of the guitar with the MAC address %s.\n", guitar->address.c_str());

                // The received guitar input data
                GuitarData* receivedData = NULL;

                // The number of bytes received
                size_t numberOfBytesReceived = 0;

                // Keep track of the connection object so the guitar can be disconnected from the destructor
                guitar->connection = connection;

                // Define the timeout callback
                ResettableTimer disconnectTimer(10, [&]() {
                    // The guitar hasn't been disconnected yet
                    if (guitar->connection != NULL)
                    {
                        // Disconnect the guitar
                        gattlib_disconnect(guitar->connection, false);

                        // Reset the connection pointer
                        guitar->connection = NULL;
                    }
                });

                // Log the newly connected guitar
                printf("Connected Guitar (%s).\n", guitar->address.c_str());

                // The guitar is still connected
                if (guitar->connection != NULL)
                {
                    // Keep receiving guitar input data
                    while (gattlib_read_char_by_uuid(guitar->connection, &characteristics[i].uuid, (void **)&receivedData, &numberOfBytesReceived) == GATTLIB_SUCCESS && numberOfBytesReceived == sizeof(GuitarData))
                    {
                        // Update the guitar's input state
                        guitar->update(*receivedData);

                        // Free the guitar input data
                        gattlib_characteristic_free_value(receivedData);

                        // Buy the guitar another 10 seconds of time
                        disconnectTimer.reset();
                    }
                }

                // No need to iterate the other characteristics
                break;
            }
        }

        // Free the characteristics
        free(characteristics);
    }

    // We couldn't discover the guitar's characteristics
    else
    {
        // Log the error
        fprintf(stderr, "Failed to discover the characteristics of the guitar with the MAC address %s.\n", guitar->address.c_str());
    }

    // The guitar hasn't been disconnected yet
    if (guitar->connection != NULL)
    {
        // Disconnect the guitar
        gattlib_disconnect(guitar->connection, false);

        // Reset the connection pointer
        guitar->connection = NULL;
    }

    // NOTE: The address corrupts when stopping the application with CTRL+C. Investigate!

    // Log the now disconnected guitar
    printf("Disconnected Guitar (%s).\n", guitar->address.c_str());
}

void Guitar::update(const GuitarData& data)
{
    // The virtual gamepad hasn't been created yet'
    if (!gamepad)
    {
        // Create a new virtual gamepad for this guitar
        gamepad = std::make_unique<Gamepad>("Guitar (" + address + ")");

        // Use the first frame as a baseline
        lastInputState = data;
    }

    // The virtual gamepad exists
    if (gamepad)
    {
        // The input event
        struct input_event ev;

        // W1 -> X
        if ((lastInputState.frets & Fret_W1) != (data.frets & Fret_W1))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_X;
            ev.value = data.frets & Fret_W1 ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("W1 %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // W2 -> BTN_TL (aka. L1)
        if ((lastInputState.frets & Fret_W2) != (data.frets & Fret_W2))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_TL;
            ev.value = data.frets & Fret_W2 ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("W2 %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // W3 -> BTN_TR (aka. R1)
        if ((lastInputState.frets & Fret_W3) != (data.frets & Fret_W3))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_TR;
            ev.value = data.frets & Fret_W3 ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("W3 %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // B1 -> A
        if ((lastInputState.frets & Fret_B1) != (data.frets & Fret_B1))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_A;
            ev.value = data.frets & Fret_B1 ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("B1 %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // B2 -> B
        if ((lastInputState.frets & Fret_B2) != (data.frets & Fret_B2))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_B;
            ev.value = data.frets & Fret_B2 ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("B2 %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // B3 -> Y
        if ((lastInputState.frets & Fret_B3) != (data.frets & Fret_B3))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_Y;
            ev.value = data.frets & Fret_B3 ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("B3 %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // Pause -> BTN_START
        if ((lastInputState.buttons & Button_Pause) != (data.buttons & Button_Pause))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_START;
            ev.value = data.buttons & Button_Pause ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("Pause %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // HeroPower -> BTN_SELECT
        if ((lastInputState.buttons & Button_HeroPower) != (data.buttons & Button_HeroPower))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_SELECT;
            ev.value = data.buttons & Button_HeroPower ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("HeroPower %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // GHTV -> BTN_THUMBL
        if ((lastInputState.buttons & Button_GHTV) != (data.buttons & Button_GHTV))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            ev.code = BTN_THUMBL;
            ev.value = data.buttons & Button_GHTV ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("GHTV %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // Sync -> BTN_MODE
        if ((lastInputState.buttons & Button_Sync) != (data.buttons & Button_Sync))
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_KEY;
            //ev.code = BTN_MODE;
            ev.code = BTN_A;
            ev.value = data.buttons & Button_Sync ? BTN_PRESSED : BTN_RELEASED;
            gamepad->update(&ev);
            // printf("Sync %s\n", ev.value == BTN_PRESSED ? "pressed" : "released");
        }

        // The directional pad
        if (lastInputState.directionalPad != data.directionalPad)
        {
            int dpadX = (data.directionalPad == Direction_SouthEast || data.directionalPad == Direction_East || data.directionalPad == Direction_NorthEast) ? DPAD_VALUE_MAX : (data.directionalPad == Direction_SouthWest || data.directionalPad == Direction_West || data.directionalPad == Direction_NorthWest) ? DPAD_VALUE_MIN : 0;
            int dpadY = (data.directionalPad == Direction_SouthEast || data.directionalPad == Direction_South || data.directionalPad == Direction_SouthWest) ? DPAD_VALUE_MIN : (data.directionalPad == Direction_NorthEast || data.directionalPad == Direction_North || data.directionalPad == Direction_NorthWest) ? DPAD_VALUE_MAX : 0;

            gettimeofday(&ev.time, nullptr);
            ev.type = EV_ABS;
            ev.code = AXIS_DPAD_HORIZONTAL;
            ev.value = dpadX;
            gamepad->update(&ev);

            gettimeofday(&ev.time, nullptr);
            ev.type = EV_ABS;
            ev.code = AXIS_DPAD_VERTICAL;
            ev.value = dpadY;
            gamepad->update(&ev);

            // printf("Dpad %d/%d\n", dpadX, dpadY);
        }

        // Whammy -> Right Analog Y
        if (lastInputState.whammy != data.whammy)
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_ABS;
            ev.code = AXIS_RIGHT_ANALOG_VERTICAL;
            ev.value = (short)((data.whammy * 0x101) - ANALOG_VALUE_MAX);
            // printf("Whammy %d\n", ev.value);
            gamepad->update(&ev);
        }

        // Tilt -> Right Analog X
        if (lastInputState.tilt != data.tilt)
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_ABS;
            ev.code = AXIS_RIGHT_ANALOG_HORIZONTAL;
            ev.value = (short)((data.tilt * 0x101) - ANALOG_VALUE_MAX);
            // printf("Tilt %d\n", ev.value);
            gamepad->update(&ev);
        }

        // Strum -> Left Analog Y
        if (lastInputState.strum != data.strum)
        {
            gettimeofday(&ev.time, nullptr);
            ev.type = EV_ABS;
            /*
            ev.code = AXIS_LEFT_ANALOG_VERTICAL;
            ev.value = data.strum == 0xff ? ANALOG_VALUE_MAX : data.strum == 0 ? ANALOG_VALUE_MIN : 0;
            */
            ev.code = AXIS_DPAD_VERTICAL;
            ev.value = data.strum == 0xff ? DPAD_VALUE_MAX : data.strum == 0 ? DPAD_VALUE_MIN : 0;
            // printf("Strum %d\n", ev.value);
            gamepad->update(&ev);
        }
    }

    // Set the last input state
    lastInputState = data;

    // Update the input timestamp
    lastInputTimestamp = std::chrono::system_clock::now();
}
