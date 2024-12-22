#ifndef RESETTABLE_TIMER_H
#define RESETTABLE_TIMER_H

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

class ResettableTimer {
public:
    /**
     * @brief Constructor to create a resettable timer.
     * @param timeoutSeconds Timeout duration in seconds.
     * @param callback Function to call when the timer expires.
     */
    ResettableTimer(int timeoutSeconds, std::function<void()> callback);

    /**
     * @brief Destructor to clean up the timer.
     */
    ~ResettableTimer();

    /**
     * @brief Resets or disarms the timer.
     */
    void reset();

private:
    std::chrono::seconds timeout;
    std::function<void()> callback;
    std::thread timerThread;
    std::mutex mutex;
    std::condition_variable cv;
    bool running;
    bool resetFlag = false;

    /**
     * @brief Main loop for the timer thread.
     */
    void timerLoop();
};

#endif // RESETTABLE_TIMER_H
