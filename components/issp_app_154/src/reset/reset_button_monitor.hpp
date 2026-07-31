#pragma once

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

private:
    static constexpr std::uint32_t kTaskStackDepth = 2048;

    static void runTask(void *context);
    void run();
    bool isPressed() const;

    ResetButtonConfig config_;
    IFactoryResetRequester &requester_;
    StaticTask_t taskControl_;
    StackType_t taskStack_[kTaskStackDepth];
    TaskHandle_t taskHandle_;
};
