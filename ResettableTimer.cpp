#include "ResettableTimer.h"

ResettableTimer::ResettableTimer(int timeoutSeconds, std::function<void()> callback)
: timeout(std::chrono::seconds(timeoutSeconds)), callback(std::move(callback)), running(true) {
    timerThread = std::thread(&ResettableTimer::timerLoop, this);
}

ResettableTimer::~ResettableTimer() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        running = false; // Stop the timer
    }
    cv.notify_all();
    if (timerThread.joinable()) {
        timerThread.join(); // Ensure thread completes before destruction
    }
}

void ResettableTimer::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    resetFlag = true;
    cv.notify_all();
}

void ResettableTimer::timerLoop() {
    std::unique_lock<std::mutex> lock(mutex);

    while (running) {
        // Wait for timeout or reset signal
        if (cv.wait_for(lock, timeout, [this]() { return !running || resetFlag; })) {
            // Check if we're shutting down
            if (!running) break;

            // Reset was signaled
            resetFlag = false;
            continue;
        }

        // Timeout occurred, invoke callback
        if (running && callback) {
            callback();
        }
    }
}
