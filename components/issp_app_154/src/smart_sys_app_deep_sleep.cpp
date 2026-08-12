// Deep sleep policy of iotsmartsys::SmartSysApp, specified by
// docs/specs/Client-Deep-Sleep.md. Like smart_sys_app.cpp it is
// target-agnostic: it owns configuration validation, the timer limit, the wake
// LED, the absolute deadline, the readiness and delivery predicates, the
// ordered terminal sequence and the arbitration with factory reset, and it
// reaches the runtime objects (device, report executor, transport, reset button
// monitor) only through SmartSysApp::SetupHooks. Nothing here knows the
// protocol or the transport, and no operation of this file may be reached when
// deep sleep was not explicitly enabled by the product firmware.

#include "smart_sys_app_impl.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "soc/clk_tree_defs.h"
#include "soc/soc_caps.h"

namespace iotsmartsys
{

namespace
{

constexpr char kTag[] = "SmartSysApp";
constexpr UBaseType_t kLifecycleTaskPriority = tskIDLE_PRIORITY + 2;

constexpr std::uint64_t kMicrosecondsPerSecond = 1000000ULL;
constexpr std::uint64_t kSecondsPerMinute = 60ULL;
constexpr std::uint64_t kSecondsPerHour = 3600ULL;

#if !defined(SOC_LP_TIMER_BIT_WIDTH_LO) || !defined(SOC_LP_TIMER_BIT_WIDTH_HI)
#error "The target does not declare the LP timer width required to bound the timer wakeup."
#endif

// esp_sleep_enable_timer_wakeup() rejects, with ESP_ERR_INVALID_ARG, any
// duration above ((2^(LO + HI) - 1) / f_slow) seconds, where f_slow is the
// frequency of the RTC slow clock. This project fixes that source to the
// internal RC calibrated at runtime (CONFIG_RTC_CLK_SRC_INT_RC), whose nominal
// value the target declares as SOC_CLK_RC_SLOW_FREQ_APPROX.
//
// A faster slow clock lowers the accepted limit, so configuration must reason
// with an upper bound of the frequency, never with the nominal value: this is
// what makes every interval accepted by configureDeepSleep() stay inside the
// range accepted when preparing, for any supported deviation of the
// calibration. The factor below is that deviation bound -- not a product
// number -- and it is deliberately generous for an RC oscillator.
constexpr std::uint64_t kLpTimerCounts =
    (1ULL << (SOC_LP_TIMER_BIT_WIDTH_LO + SOC_LP_TIMER_BIT_WIDTH_HI)) - 1ULL;
constexpr std::uint64_t kSlowClockCalibrationBoundFactor = 2ULL;
constexpr std::uint64_t kSlowClockUpperBoundHz =
    static_cast<std::uint64_t>(SOC_CLK_RC_SLOW_FREQ_APPROX) *
    kSlowClockCalibrationBoundFactor;
constexpr std::uint64_t kDerivedMaxTimerWakeupUs =
    (kLpTimerCounts / kSlowClockUpperBoundHz) * kMicrosecondsPerSecond;

bool validTimeUnit(app::DeepSleepTimeUnit unit)
{
    return unit == app::DeepSleepTimeUnit::Minutes ||
           unit == app::DeepSleepTimeUnit::Hours;
}

bool validOnMode(app::WakeLedOnMode mode)
{
    return mode == app::WakeLedOnMode::DurationMs ||
           mode == app::WakeLedOnMode::UntilSleep;
}

const char *wakeupCauseName(esp_sleep_wakeup_cause_t cause)
{
    switch (cause)
    {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        return "cold_boot_or_reset";
    case ESP_SLEEP_WAKEUP_TIMER:
        return "timer";
    default:
        return "other";
    }
}

} // namespace

// --- configuration -------------------------------------------------------

std::uint64_t
SmartSysApp::Impl::timerWakeupIntervalUs(const app::TimerWakeupConfig &timer)
{
    // A 32-bit interval in minutes or hours cannot overflow 64 bits here; the
    // material limit is the one accepted by the wakeup API, checked separately.
    const std::uint64_t interval = static_cast<std::uint64_t>(timer.interval);
    const std::uint64_t secondsPerUnit =
        timer.unit == app::DeepSleepTimeUnit::Hours ? kSecondsPerHour
                                                    : kSecondsPerMinute;
    return interval * secondsPerUnit * kMicrosecondsPerSecond;
}

std::uint64_t SmartSysApp::Impl::maxTimerWakeupUs() const
{
    // A non-zero seam value is only ever supplied by doubles; production wiring
    // leaves it at zero and uses the limit derived from the target.
    return hooks_.maxTimerWakeupUs != 0 ? hooks_.maxTimerWakeupUs
                                        : kDerivedMaxTimerWakeupUs;
}

bool SmartSysApp::Impl::wakeLedEnabled() const
{
    return deepSleepConfigured_ && deepSleepConfig_.enabled &&
           deepSleepConfig_.wakeLed.enabled;
}

bool SmartSysApp::Impl::collidesWithWakeLed(gpio_num_t pin) const
{
    // Additive validation restricted to wake_led: with deep sleep or the LED
    // disabled, no global validation is created between pairs already accepted.
    return wakeLedEnabled() && deepSleepConfig_.wakeLed.pin == pin;
}

AppResult SmartSysApp::Impl::configureDeepSleep(const app::DeepSleepConfig &config)
{
    if (state_ != AppState::Configuring)
    {
        recordConfigurationFailure(AppResult::Failed);
        return AppResult::Failed;
    }
    if (deepSleepConfigured_)
    {
        recordConfigurationFailure(AppResult::Failed);
        return AppResult::Failed;
    }

    if (!config.enabled)
    {
        // Preserves the current runtime: no GPIO and no wakeup source is
        // touched, now or later in this boot.
        deepSleepConfig_ = config;
        deepSleepConfigured_ = true;
        return AppResult::Ok;
    }

    if (config.maxAwakeTimeMs == 0U)
    {
        recordConfigurationFailure(AppResult::InvalidArgument);
        return AppResult::InvalidArgument;
    }

    if (config.timerWakeup.enabled)
    {
        if (config.timerWakeup.interval == 0U || !validTimeUnit(config.timerWakeup.unit))
        {
            recordConfigurationFailure(AppResult::InvalidArgument);
            return AppResult::InvalidArgument;
        }
        if (timerWakeupIntervalUs(config.timerWakeup) > maxTimerWakeupUs())
        {
            ESP_LOGE(kTag, "deep_sleep_config rejected reason=timer_interval_above_limit");
            recordConfigurationFailure(AppResult::InvalidArgument);
            return AppResult::InvalidArgument;
        }
    }

    if (config.wakeLed.enabled)
    {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(config.wakeLed.pin) ||
            !validOnMode(config.wakeLed.onMode) ||
            (config.wakeLed.onMode == app::WakeLedOnMode::DurationMs &&
             config.wakeLed.onTimeMs == 0U))
        {
            recordConfigurationFailure(AppResult::InvalidArgument);
            return AppResult::InvalidArgument;
        }

        // The LED GPIO must not collide with a capability or with the factory
        // reset button already registered. The inverse comparison, for
        // resources registered after the LED, is done by their own validation.
        bool collides = factoryResetConfigured_ &&
                        factoryResetConfig_.pin == config.wakeLed.pin;
        for (std::size_t index = 0; !collides && index < switchCount_; ++index)
        {
            collides = switchConfigs_[index].pin == config.wakeLed.pin;
        }
        for (std::size_t index = 0; !collides && index < doorSensorCount_; ++index)
        {
            collides = doorSensorConfigs_[index].pin == config.wakeLed.pin;
        }
        if (collides)
        {
            ESP_LOGE(kTag, "deep_sleep_config rejected reason=wake_led_gpio_collision gpio=%d",
                     static_cast<int>(config.wakeLed.pin));
            recordConfigurationFailure(AppResult::InvalidArgument);
            return AppResult::InvalidArgument;
        }
    }

    deepSleepConfig_ = config;
    deepSleepConfigured_ = true;
    ESP_LOGI(kTag,
             "deep_sleep_config accepted max_awake_ms=%lu timer=%s wake_led=%s",
             static_cast<unsigned long>(config.maxAwakeTimeMs),
             config.timerWakeup.enabled ? "enabled" : "disabled",
             config.wakeLed.enabled ? "enabled" : "disabled");
    return AppResult::Ok;
}

// --- wake LED ------------------------------------------------------------

void SmartSysApp::Impl::setWakeLedLevel(bool on)
{
    const app::WakeLedConfig &led = deepSleepConfig_.wakeLed;
    const int level = (on == led.activeHigh) ? 1 : 0;
    const esp_err_t result = gpio_set_level(led.pin, level);
    if (result != ESP_OK)
    {
        ESP_LOGE(kTag, "wake_led level failed on=%d result=%d",
                 on ? 1 : 0, static_cast<int>(result));
        return;
    }
    wakeLedOn_ = on;
}

void SmartSysApp::Impl::wakeLedExpired(void *context)
{
    if (context == nullptr)
    {
        return;
    }
    static_cast<Impl *>(context)->setWakeLedLevel(false);
}

AppResult SmartSysApp::Impl::configureWakeLed()
{
    const app::WakeLedConfig &led = deepSleepConfig_.wakeLed;

    // Level before direction, as DigitalOutputBehavior already does: this is
    // what limits, on the ESP32-H2, a visible pulse of the opposite polarity
    // when the pad becomes an output.
    esp_err_t result = gpio_set_level(led.pin, led.activeHigh ? 1 : 0);
    if (result != ESP_OK)
    {
        ESP_LOGE(kTag, "wake_led preset failed gpio=%d result=%d",
                 static_cast<int>(led.pin), static_cast<int>(result));
        return AppResult::Failed;
    }

    gpio_config_t gpioConfig{};
    gpioConfig.pin_bit_mask = 1ULL << static_cast<std::uint32_t>(led.pin);
    gpioConfig.mode = GPIO_MODE_OUTPUT;
    gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;
    result = gpio_config(&gpioConfig);
    if (result != ESP_OK)
    {
        ESP_LOGE(kTag, "wake_led config failed gpio=%d result=%d",
                 static_cast<int>(led.pin), static_cast<int>(result));
        return AppResult::Failed;
    }

    result = gpio_set_level(led.pin, led.activeHigh ? 1 : 0);
    if (result != ESP_OK)
    {
        ESP_LOGE(kTag, "wake_led on failed gpio=%d result=%d",
                 static_cast<int>(led.pin), static_cast<int>(result));
        return AppResult::Failed;
    }
    wakeLedOn_ = true;

    if (led.onMode == app::WakeLedOnMode::DurationMs)
    {
        const esp_timer_create_args_t timerArgs = {
            .callback = &Impl::wakeLedExpired,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "issp_wake_led",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&timerArgs, &wakeLedTimer_) != ESP_OK)
        {
            ESP_LOGE(kTag, "wake_led timer create failed");
            return AppResult::Failed;
        }
        const std::uint64_t onTimeUs =
            static_cast<std::uint64_t>(led.onTimeMs) * 1000ULL;
        if (esp_timer_start_once(wakeLedTimer_, onTimeUs) != ESP_OK)
        {
            ESP_LOGE(kTag, "wake_led timer start failed");
            (void)esp_timer_delete(wakeLedTimer_);
            wakeLedTimer_ = nullptr;
            return AppResult::Failed;
        }
    }

    ESP_LOGI(kTag, "wake_led on gpio=%d active_high=%d mode=%s on_time_ms=%lu",
             static_cast<int>(led.pin), led.activeHigh ? 1 : 0,
             led.onMode == app::WakeLedOnMode::DurationMs ? "duration_ms"
                                                          : "until_sleep",
             static_cast<unsigned long>(led.onTimeMs));
    return AppResult::Ok;
}

void SmartSysApp::Impl::releaseWakeLed()
{
    if (!wakeLedEnabled())
    {
        return;
    }
    if (wakeLedTimer_ != nullptr)
    {
        (void)esp_timer_stop(wakeLedTimer_);
        (void)esp_timer_delete(wakeLedTimer_);
        wakeLedTimer_ = nullptr;
    }
    if (wakeLedOn_)
    {
        setWakeLedLevel(false);
    }
}

// --- platform entry and lifecycle task -----------------------------------

AppResult SmartSysApp::Impl::beginPlatformPowerPolicy()
{
    if (!deepSleepConfigured_ || !deepSleepConfig_.enabled)
    {
        return AppResult::Ok;
    }

    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(kTag, "boot_cause=%s raw=%d", wakeupCauseName(cause),
             static_cast<int>(cause));

    if (!deepSleepConfig_.wakeLed.enabled)
    {
        return AppResult::Ok;
    }
    return configureWakeLed();
}

void SmartSysApp::Impl::startPowerLifecycle()
{
    if (!deepSleepConfigured_ || !deepSleepConfig_.enabled)
    {
        return;
    }
    if (lifecycleTaskHandle_ != nullptr)
    {
        return;
    }

    lifecycleTaskHandle_ = xTaskCreateStatic(
        &Impl::powerLifecycleTask,
        "issp_power_lifecycle",
        kLifecycleTaskStackDepth,
        this,
        kLifecycleTaskPriority,
        lifecycleTaskStack_,
        &lifecycleTaskControl_);
    if (lifecycleTaskHandle_ == nullptr)
    {
        // Without the task there is no owner for the sequence, so this boot
        // stays awake. The device remains reachable and diagnosable.
        ESP_LOGE(kTag, "power_lifecycle task create failed deep_sleep=blocked");
        return;
    }
    ESP_LOGI(kTag, "power_lifecycle started max_awake_ms=%lu",
             static_cast<unsigned long>(deepSleepConfig_.maxAwakeTimeMs));
}

void SmartSysApp::Impl::powerLifecycleTask(void *context)
{
    if (context != nullptr)
    {
        static_cast<Impl *>(context)->runPowerLifecycle();
    }
    vTaskDelete(nullptr);
}

std::int64_t SmartSysApp::Impl::remainingAwakeMs() const
{
    const std::int64_t elapsedMs =
        (esp_timer_get_time() - awakeWindowStartUs_) / 1000;
    return static_cast<std::int64_t>(deepSleepConfig_.maxAwakeTimeMs) - elapsedMs;
}

bool SmartSysApp::Impl::readyForEarlyQuiescence() const
{
    // Early sleep demands positive evidence of a report in this boot, so the
    // sequence only starts after setup() reached Running: reaching Running is
    // itself the evidence that begin() succeeded for every registered behavior,
    // because IsspDevice::start() aborts on the first one that fails.
    if (state_ != AppState::Running)
    {
        return false;
    }

    std::size_t expected = 0;
    for (std::size_t index = 0; index < switchCount_; ++index)
    {
        if (switchConfigs_[index].reportOnStart)
        {
            // DigitalOutputBehavior::begin() publishes synchronously and
            // propagates the failure, so Running already proves admission.
            ++expected;
        }
    }
    for (std::size_t index = 0; index < doorSensorCount_; ++index)
    {
        if (!doorSensorConfigs_[index].reportOnStart)
        {
            continue;
        }
        ++expected;
        // hasConfirmedState() is only true after a required initial
        // publication succeeded; initial_stabilization_pending is not that
        // confirmation and keeps the device awake.
        if (!doorSensorBehaviors_[index]->hasConfirmedState())
        {
            return false;
        }
    }

    // No expected initial report means there is no positive evidence at all:
    // absence of a report never amounts to admission, so only the deadline may
    // authorize sleep.
    return expected > 0;
}

void SmartSysApp::Impl::runPowerLifecycle()
{
    for (;;)
    {
        if (remainingAwakeMs() <= 0)
        {
            (void)runTerminalSequence(true);
            return;
        }
        if (readyForEarlyQuiescence())
        {
            // Either the sequence commits the sleep and never returns, or it
            // aborted before any terminal operation and the runtime must stay
            // reachable, with factory reset available again.
            (void)runTerminalSequence(false);
            return;
        }
        // Ceiling the specification sets for re-evaluating both predicates.
        vTaskDelay(pdMS_TO_TICKS(kLifecyclePollIntervalMs));
    }
}

// --- arbitration ---------------------------------------------------------

bool SmartSysApp::Impl::acquireDeepSleepTransition()
{
    std::uint8_t expected = static_cast<std::uint8_t>(TransitionOwner::Free);
    if (transitionOwner_.compare_exchange_strong(
            expected, static_cast<std::uint8_t>(TransitionOwner::DeepSleep)))
    {
        return true;
    }
    // Recognizing the transition already held by deep sleep itself is what lets
    // the early path turn into the forced path without reacquiring it.
    return expected == static_cast<std::uint8_t>(TransitionOwner::DeepSleep);
}

void SmartSysApp::Impl::releaseDeepSleepTransition()
{
    transitionOwner_.store(static_cast<std::uint8_t>(TransitionOwner::Free));
}

bool SmartSysApp::Impl::acquireFactoryResetTransition(void *context)
{
    auto *self = static_cast<Impl *>(context);
    std::uint8_t expected = static_cast<std::uint8_t>(TransitionOwner::Free);
    if (self->transitionOwner_.compare_exchange_strong(
            expected, static_cast<std::uint8_t>(TransitionOwner::FactoryReset)))
    {
        return true;
    }
    return expected == static_cast<std::uint8_t>(TransitionOwner::FactoryReset);
}

void SmartSysApp::Impl::releaseFactoryResetTransition(void *context)
{
    static_cast<Impl *>(context)->releaseDeepSleepTransition();
}

// --- terminal sequence ---------------------------------------------------

bool SmartSysApp::Impl::runTerminalSequence(bool forced)
{
    if (!acquireDeepSleepTransition())
    {
        ESP_LOGW(kTag, "deep_sleep aborted reason=factory_reset_holds_transition");
        return false;
    }

    if (forced && currentStage_ == SetupStage::InitializeNetwork)
    {
        // Possibility, not observation: the private persistence window of
        // initializeNetwork() is not inspected from here.
        ESP_LOGW(kTag, "persistence_preemption_possible=true");
    }

    // 1. Prepare the requested wakeup source. A failure aborts before any
    //    terminal operation, releases the arbitration and keeps the runtime
    //    reachable, so the device is never made inaccessible unintentionally.
    if (deepSleepConfig_.timerWakeup.enabled)
    {
        const std::uint64_t sleepUs = timerWakeupIntervalUs(deepSleepConfig_.timerWakeup);
        if (sleepUs == 0U || sleepUs > maxTimerWakeupUs())
        {
            // Defence in depth: the same validation configureDeepSleep() ran.
            ESP_LOGE(kTag, "deep_sleep aborted reason=timer_interval_out_of_range");
            releaseDeepSleepTransition();
            return false;
        }
        const AppResult prepareResult =
            hooks_.prepareTimerWakeup != nullptr
                ? hooks_.prepareTimerWakeup(hooks_.context, sleepUs)
                : (esp_sleep_enable_timer_wakeup(sleepUs) == ESP_OK
                       ? AppResult::Ok
                       : AppResult::InvalidArgument);
        if (prepareResult != AppResult::Ok)
        {
            ESP_LOGE(kTag, "deep_sleep aborted reason=timer_wakeup_rejected result=%u",
                     static_cast<unsigned>(prepareResult));
            releaseDeepSleepTransition();
            return false;
        }
    }
    else
    {
        // Intentional absence of a source does not abort: the device will only
        // wake by reset or a new power-up.
        ESP_LOGW(kTag, "deep_sleep no_wakeup_source wake_by=reset_or_power_cycle");
    }

    // 2. End the reset button monitor, when configured.
    if (hooks_.stopResetButtonMonitor != nullptr)
    {
        hooks_.stopResetButtonMonitor(hooks_.context);
    }

    // 3. Close command dispatch and report admission atomically.
    if (hooks_.beginDeviceQuiescence != nullptr)
    {
        const AppResult quiescenceResult = hooks_.beginDeviceQuiescence(hooks_.context);
        if (quiescenceResult != AppResult::Ok)
        {
            ESP_LOGE(kTag, "quiescence device result=%u",
                     static_cast<unsigned>(quiescenceResult));
        }
    }

    // 4. Quiesce every behavior registered in the facade, which covers strictly
    //    more than the device registry when setup() failed before
    //    RegisterCapabilities. A failure is always recorded and reopens nothing.
    for (std::size_t index = 0; index < behaviorCount_; ++index)
    {
        if (behaviors_[index] == nullptr)
        {
            continue;
        }
        const issp::IsspResult behaviorResult = behaviors_[index]->quiesce();
        if (behaviorResult != issp::IsspResult::Ok)
        {
            ESP_LOGE(kTag, "quiescence behavior index=%u result=%u",
                     static_cast<unsigned>(index),
                     static_cast<unsigned>(behaviorResult));
        }
    }

    // 5. With admission closed and the producers stopped, a pending count of
    //    zero proves delivery. The wait uses only the time left until the
    //    absolute deadline and has no timeout of its own.
    std::size_t pending = hooks_.pendingReportCount != nullptr
                              ? hooks_.pendingReportCount(hooks_.context)
                              : 0U;
    if (!forced)
    {
        while (pending != 0U)
        {
            if (remainingAwakeMs() <= 0)
            {
                // The same task and the same holder move to the forced path.
                forced = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(kLifecyclePollIntervalMs));
            pending = hooks_.pendingReportCount != nullptr
                          ? hooks_.pendingReportCount(hooks_.context)
                          : 0U;
        }
    }

    // 6. Stop the report executor. Its bounded wait covers the transport
    //    attempt already in flight; on expiry the transport must not be torn
    //    down, because the executor may still use the ACK event group.
    const std::int64_t stopStartUs = esp_timer_get_time();
    const AppResult stopResult = hooks_.stopReportExecutor != nullptr
                                     ? hooks_.stopReportExecutor(hooks_.context)
                                     : AppResult::Ok;
    const std::int64_t stopElapsedMs = (esp_timer_get_time() - stopStartUs) / 1000;
    if (stopResult == AppResult::Ok &&
        stopElapsedMs <= static_cast<std::int64_t>(kReportExecutorStopBudgetMs))
    {
        if (hooks_.endTransport != nullptr)
        {
            hooks_.endTransport(hooks_.context);
        }
    }
    else
    {
        ESP_LOGW(kTag,
                 "report_executor_stop timeout budget_ms=%lu elapsed_ms=%lld "
                 "result=%u transport_end=suppressed",
                 static_cast<unsigned long>(kReportExecutorStopBudgetMs),
                 static_cast<long long>(stopElapsedMs),
                 static_cast<unsigned>(stopResult));
    }

    // 7. Record the outcome without payload, turn the LED off and sleep. Reports
    //    still pending are not persisted in this version.
    ESP_LOGW(kTag,
             "deep_sleep entering cause=%s pending_reports=%u app_state=%u "
             "timer_wakeup=%s",
             forced ? "deadline" : "early",
             static_cast<unsigned>(pending),
             static_cast<unsigned>(state_),
             deepSleepConfig_.timerWakeup.enabled ? "enabled" : "disabled");

    releaseWakeLed();

    if (hooks_.enterDeepSleep != nullptr)
    {
        hooks_.enterDeepSleep(hooks_.context);
        return true;
    }
    esp_deep_sleep_start();
    return true;
}

} // namespace iotsmartsys
