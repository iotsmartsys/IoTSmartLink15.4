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
      taskHandle_(nullptr),
      stopRequested_(false),
      taskExited_(false)
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

esp_err_t ResetButtonMonitor::stop()
{
    // taskHandle_ is assigned only after xTaskCreateStatic() returned a valid
    // handle, so a null handle identifies exactly the monitor never started.
    if (taskHandle_ == nullptr)
    {
        return ESP_OK;
    }
    if (taskExited_.load(std::memory_order_acquire))
    {
        return ESP_OK;
    }

    stopRequested_.store(true, std::memory_order_release);
    xTaskNotifyGive(taskHandle_);

    const std::int64_t deadlineUs =
        esp_timer_get_time() +
        static_cast<std::int64_t>(config_.pollIntervalMs + kStopSchedulingMarginMs) * 1000;
    while (!taskExited_.load(std::memory_order_acquire))
    {
        if (esp_timer_get_time() >= deadlineUs)
        {
            ESP_LOGW(kTag, "stop timeout_ms=%lu",
                     static_cast<unsigned long>(config_.pollIntervalMs +
                                                kStopSchedulingMarginMs));
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    ESP_LOGI(kTag, "stop completed");
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
    bool rejectionLogged = false;
    std::int64_t pressedAtUs = 0;

    if (!armed)
    {
        ESP_LOGW(kTag, "waiting_for_release gpio=%d", static_cast<int>(config_.gpio));
    }

    while (!stopRequested_.load(std::memory_order_acquire))
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
            rejectionLogged = false;
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
            rejectionLogged = false;
        }
        else if (currentlyPressed && pressed && !resetRequested)
        {
            const std::uint32_t elapsedMs = static_cast<std::uint32_t>(
                (esp_timer_get_time() - pressedAtUs) / 1000);
            if (elapsedMs >= config_.holdTimeMs)
            {
                // A rejected request does not consume the hold in course: the
                // press stays valid and the request is presented again while the
                // button is held, in case the transition is released.
                const FactoryResetRequestResult requestResult =
                    requester_.requestFactoryReset();
                if (requestResult == FactoryResetRequestResult::Accepted)
                {
                    resetRequested = true;
                    ESP_LOGW(kTag, "countdown completed elapsed_ms=%lu",
                             static_cast<unsigned long>(elapsedMs));
                }
                else if (!rejectionLogged)
                {
                    rejectionLogged = true;
                    ESP_LOGW(kTag,
                             "countdown completed but request rejected "
                             "elapsed_ms=%lu hold_preserved=yes",
                             static_cast<unsigned long>(elapsedMs));
                }
            }
        }

        // Preserves the configured period while letting stop() interrupt the
        // wait without xTaskAbortDelay or a Kconfig option.
        (void)ulTaskNotifyTake(pdTRUE, pollDelay);
    }

    ESP_LOGI(kTag, "task ended reason=deep_sleep_transition");
    taskExited_.store(true, std::memory_order_release);
}

bool ResetButtonMonitor::isPressed() const
{
    const int level = gpio_get_level(config_.gpio);
    return config_.activeLow ? level == 0 : level != 0;
}
