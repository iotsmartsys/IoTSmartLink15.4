#include "reset/reset_button_monitor.hpp"

#include "esp_log.h"
#include "esp_timer.h"

namespace
{

constexpr char kTag[] = "RESET_BUTTON";
constexpr UBaseType_t kTaskPriority = tskIDLE_PRIORITY + 1;

} // namespace

ResetButtonMonitor::ResetButtonMonitor(const ResetButtonConfig &config,
                                       IFactoryResetRequester &requester)
    : config_(config),
      requester_(requester),
      taskControl_{},
      taskStack_{},
      taskHandle_(nullptr)
{
}

esp_err_t ResetButtonMonitor::start()
{
    if (taskHandle_ != nullptr)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!GPIO_IS_VALID_GPIO(config_.gpio) || config_.holdTimeMs == 0U ||
        config_.pollIntervalMs == 0U)
    {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t gpioConfig{};
    gpioConfig.pin_bit_mask = 1ULL << static_cast<std::uint32_t>(config_.gpio);
    gpioConfig.mode = GPIO_MODE_INPUT;
    gpioConfig.pull_up_en = config_.activeLow ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = config_.activeLow ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;

    const esp_err_t gpioResult = gpio_config(&gpioConfig);
    if (gpioResult != ESP_OK)
    {
        return gpioResult;
    }

    taskHandle_ = xTaskCreateStatic(
        &ResetButtonMonitor::runTask,
        "reset_button",
        kTaskStackDepth,
        this,
        kTaskPriority,
        taskStack_,
        &taskControl_);
    if (taskHandle_ == nullptr)
    {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag,
             "initialized gpio=%d hold_ms=%lu",
             static_cast<int>(config_.gpio),
             static_cast<unsigned long>(config_.holdTimeMs));
    return ESP_OK;
}

void ResetButtonMonitor::runTask(void *context)
{
    if (context != nullptr)
    {
        static_cast<ResetButtonMonitor *>(context)->run();
    }
    vTaskDelete(nullptr);
}

void ResetButtonMonitor::run()
{
    const TickType_t pollDelay = pdMS_TO_TICKS(config_.pollIntervalMs);
    bool armed = !isPressed();
    bool pressed = false;
    bool resetRequested = false;
    std::int64_t pressedAtUs = 0;

    if (!armed)
    {
        ESP_LOGW(kTag, "waiting_for_release gpio=%d", static_cast<int>(config_.gpio));
    }

    for (;;)
    {
        const bool currentlyPressed = isPressed();

        if (!armed)
        {
            if (!currentlyPressed)
            {
                armed = true;
                ESP_LOGI(kTag, "armed gpio=%d", static_cast<int>(config_.gpio));
            }
        }
        else if (currentlyPressed && !pressed)
        {
            pressed = true;
            resetRequested = false;
            pressedAtUs = esp_timer_get_time();
            ESP_LOGI(kTag, "pressed");
        }
        else if (!currentlyPressed && pressed)
        {
            const std::uint32_t elapsedMs = static_cast<std::uint32_t>(
                (esp_timer_get_time() - pressedAtUs) / 1000);
            ESP_LOGI(kTag, "released elapsed_ms=%lu",
                     static_cast<unsigned long>(elapsedMs));
            pressed = false;
            resetRequested = false;
        }
        else if (currentlyPressed && pressed && !resetRequested)
        {
            const std::uint32_t elapsedMs = static_cast<std::uint32_t>(
                (esp_timer_get_time() - pressedAtUs) / 1000);
            if (elapsedMs >= config_.holdTimeMs)
            {
                resetRequested = true;
                ESP_LOGW(kTag, "countdown completed elapsed_ms=%lu",
                         static_cast<unsigned long>(elapsedMs));
                requester_.requestFactoryReset();
            }
        }

        vTaskDelay(pollDelay);
    }
}

bool ResetButtonMonitor::isPressed() const
{
    const int level = gpio_get_level(config_.gpio);
    return config_.activeLow ? level == 0 : level != 0;
}
