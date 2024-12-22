#ifndef GUITAR_H
#define GUITAR_H

#include "gamepad.h"
#include "ResettableTimer.h"

#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <pthread.h>
#include <gattlib.h>

enum GuitarFrets {
    Fret_W1 = 0x1,
    Fret_B1 = 0x2,
    Fret_B2 = 0x4,
    Fret_B3 = 0x8,
    Fret_W2 = 0x10,
    Fret_W3 = 0x20
};

enum GuitarButtons {
    Button_Pause = 0x2,
    Button_GHTV = 0x4,
    Button_HeroPower = 0x8,
    Button_Sync = 0x10
};

enum GuitarDirectionalPadDirections {
    Direction_South = 0,
    Direction_SouthEast = 1,
    Direction_East = 2,
    Direction_NorthEast = 3,
    Direction_North = 4,
    Direction_NorthWest = 5,
    Direction_West = 6,
    Direction_SouthWest = 7,
    Direction_Centered = 0xf
};

// 20 bytes long
typedef struct GuitarData {
    uint8_t frets;          // 1 byte
    uint8_t buttons;        // 1 byte
    uint8_t directionalPad; // 1 byte, 0x1 ~ 0x7, 0xf is the rest position
    uint8_t unused1;        // 1 byte, seems to always be 0x80
    uint8_t strum;          // 1 byte, 0x00 ~ 0xFF
    uint8_t lift;           // 1 byte, 0xFF when lifted, 0x80 when resting
    uint8_t whammy;         // 1 byte, 0x80 ~ 0xFF
    uint8_t unused2[12];    // 12 bytes, unused padding
    uint8_t tilt;           // 1 byte, 0x00 ~ 0xFF
} __attribute__((packed)) GuitarData;

class Guitar {
private:
    // The virtual gamepad
    std::unique_ptr<Gamepad> gamepad;

    // The Bluetooth adapter
    gattlib_adapter_t* adapter;

    // The connection to the guitar
    gattlib_connection_t* connection;

    // The MAC address of the guitar
    std::string address;

    // The guitar's input thread
    std::unique_ptr<std::thread> thread;

    // Whether we're currently reading guitar data
    bool is_reading;

    // The last input state
    GuitarData lastInputState;

    // Last input timestamp
    std::chrono::time_point<std::chrono::system_clock> lastInputTimestamp;

    // The disposed flag
    bool disposed;

    // Maintains a connection to the guitar
    void maintainConnection();

    // Receives guitar data
    static void receiveData(gattlib_adapter_t* adapter, const char *dst, gattlib_connection_t* connection, int error, void* user_data);

    // Updates guitar data and the last input timestamp
    void update(const GuitarData& data);

public:
    // Constructor
    Guitar(gattlib_adapter_t* adapterValue, const std::string& addressValue);

    // Destructor
    ~Guitar();

    // Getter
    std::string getAddress() const;
    bool isConnected();

};

#endif // GUITAR_H
