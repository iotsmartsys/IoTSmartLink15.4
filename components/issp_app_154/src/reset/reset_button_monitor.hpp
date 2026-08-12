#pragma once

#include <atomic>
#include <cstdint>

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ifactory_reset_requester.hpp"

struct ResetButtonConfig
{
    gpio_num_t gpio;
    std::uint32_t holdTimeMs;
    std::uint32_t pollIntervalMs;
    bool activeLow;
};

class ResetButtonMonitor
{
public:
    ResetButtonMonitor(const ResetButtonConfig &config,
                       IFactoryResetRequester &requester);

    esp_err_t start();
    /// Terminal, idempotent stop for the current boot, used once deep sleep has
    /// won the power transition and its wakeup source is prepared. It ends the
    /// task in a bounded way and changes no GPIO, polarity, hold time or factory
    /// reset semantics outside that transition. A no-op returning ESP_OK when
    /// start() never completed successfully.
    esp_err_t stop();

private:
    static constexpr std::uint32_t kTaskStackDepth = 2048;
    // Only covers scheduling: the task waits on the notification, so it wakes as
    // soon as stop() signals it.
    static constexpr std::uint32_t kStopSchedulingMarginMs = 100;

    static void runTask(void *context);
    void run();
    bool isPressed() const;

    ResetButtonConfig config_;
    IFactoryResetRequester &requester_;
    StaticTask_t taskControl_;
    StackType_t taskStack_[kTaskStackDepth];
    TaskHandle_t taskHandle_;
    std::atomic<bool> stopRequested_;
    std::atomic<bool> taskExited_;
};
