#include "digital_input_behavior.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ibehavior_state_publisher.hpp"

namespace issp
{

namespace
{
constexpr char kTag[] = "DIGITAL_INPUT";
}

DigitalInputBehavior::DigitalInputBehavior(const DigitalInputConfig &config)
    : DigitalInputBehavior(config, nullptr, nullptr)
{
}

DigitalInputBehavior::DigitalInputBehavior(const DigitalInputConfig &config,
                                           LevelReader levelReader,
                                           void *levelReaderContext)
    : config_(config),
      publisher_(nullptr),
      levelReader_(levelReader),
      levelReaderContext_(levelReaderContext),
      timer_(nullptr),
      timerStarted_(false),
      sampleCount_(0),
      activeSampleCount_(0),
      hasWindowClassification_(false),
      lastWindowClassification_(false),
      consecutiveClassificationCount_(0),
      hasConfirmedState_(false),
      confirmedState_(false)
{
}

DigitalInputBehavior::~DigitalInputBehavior()
{
    stopAndDeleteTimer();
    publisher_ = nullptr;
}

bool DigitalInputBehavior::validConfig() const
{
    const bool validPull = config_.pull == DigitalInputPull::Floating ||
                           config_.pull == DigitalInputPull::PullUp ||
                           config_.pull == DigitalInputPull::PullDown;
    return (config_.activeLevel == 0U || config_.activeLevel == 1U) &&
           GPIO_IS_VALID_GPIO(config_.pin) &&
           validPull &&
           config_.samplePeriodMs > 0U &&
           config_.samplesPerWindow > 0U &&
           config_.majorityThreshold > 0U &&
           config_.majorityThreshold <= config_.samplesPerWindow &&
           config_.consecutiveWindows > 0U;
}

IsspResult DigitalInputBehavior::configureGpio()
{
    gpio_config_t gpioConfig{};
    gpioConfig.pin_bit_mask = 1ULL << static_cast<std::uint32_t>(config_.pin);
    gpioConfig.mode = GPIO_MODE_INPUT;
    gpioConfig.pull_up_en = config_.pull == DigitalInputPull::PullUp
                                ? GPIO_PULLUP_ENABLE
                                : GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = config_.pull == DigitalInputPull::PullDown
                                  ? GPIO_PULLDOWN_ENABLE
                                  : GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&gpioConfig) == ESP_OK ? IsspResult::Ok : IsspResult::Failed;
}

IsspResult DigitalInputBehavior::createTimer()
{
    const esp_timer_create_args_t timerArgs = {
        .callback = &DigitalInputBehavior::timerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "issp_digital_input",
        .skip_unhandled_events = true,
    };
    return esp_timer_create(&timerArgs, &timer_) == ESP_OK
               ? IsspResult::Ok
               : IsspResult::Failed;
}

void DigitalInputBehavior::stopAndDeleteTimer()
{
    if (timer_ == nullptr)
    {
        return;
    }
    if (timerStarted_)
    {
        (void)esp_timer_stop(timer_);
        timerStarted_ = false;
    }
    (void)esp_timer_delete(timer_);
    timer_ = nullptr;
}

IsspResult DigitalInputBehavior::begin(IBehaviorStatePublisher &publisher)
{
    if (!validConfig() || levelReader_ != nullptr)
    {
        return IsspResult::InvalidArgument;
    }
    if (configureGpio() != IsspResult::Ok)
    {
        return IsspResult::Failed;
    }

    return beginTimerBacked(publisher);
}

IsspResult DigitalInputBehavior::beginTimerBacked(IBehaviorStatePublisher &publisher)
{
    if (createTimer() != IsspResult::Ok)
    {
        return IsspResult::Failed;
    }

    publisher_ = &publisher;
    const TickType_t sampleDelay = pdMS_TO_TICKS(config_.samplePeriodMs);
    if (sampleDelay == 0)
    {
        stopAndDeleteTimer();
        publisher_ = nullptr;
        return IsspResult::InvalidArgument;
    }

    const std::uint32_t initialSamples =
        static_cast<std::uint32_t>(config_.samplesPerWindow) *
        static_cast<std::uint32_t>(config_.consecutiveWindows);
    for (std::uint32_t index = 0; index < initialSamples; ++index)
    {
        vTaskDelay(sampleDelay);
        const IsspResult sampleResult = sampleCurrentLevel();
        if (sampleResult != IsspResult::Ok)
        {
            stopAndDeleteTimer();
            publisher_ = nullptr;
            return sampleResult;
        }
    }

    if (!hasConfirmedState_)
    {
        stopAndDeleteTimer();
        publisher_ = nullptr;
        return IsspResult::Failed;
    }

    const std::uint64_t periodUs =
        static_cast<std::uint64_t>(config_.samplePeriodMs) * 1000ULL;
    if (esp_timer_start_periodic(timer_, periodUs) != ESP_OK)
    {
        stopAndDeleteTimer();
        publisher_ = nullptr;
        return IsspResult::Failed;
    }
    timerStarted_ = true;
    return IsspResult::Ok;
}

IsspResult DigitalInputBehavior::beginForTest(IBehaviorStatePublisher &publisher)
{
    if (!validConfig() || levelReader_ == nullptr || publisher_ != nullptr)
    {
        return IsspResult::InvalidArgument;
    }
    publisher_ = &publisher;
    return IsspResult::Ok;
}

IsspResult DigitalInputBehavior::beginTimerForTest(IBehaviorStatePublisher &publisher)
{
    if (!validConfig() || levelReader_ == nullptr || publisher_ != nullptr)
    {
        return IsspResult::InvalidArgument;
    }
    return beginTimerBacked(publisher);
}

IsspResult DigitalInputBehavior::sampleForTest(std::uint32_t level)
{
    if (publisher_ == nullptr || levelReader_ == nullptr || level > 1U)
    {
        return IsspResult::InvalidArgument;
    }
    return processSample(level);
}

void DigitalInputBehavior::timerCallback(void *context)
{
    if (context == nullptr)
    {
        return;
    }
    auto *self = static_cast<DigitalInputBehavior *>(context);
    const IsspResult result = self->sampleCurrentLevel();
    if (result != IsspResult::Ok)
    {
        ESP_LOGE(kTag, "periodic sample failed result=%u",
                 static_cast<unsigned>(result));
    }
}

IsspResult DigitalInputBehavior::sampleCurrentLevel()
{
    const int rawLevel = levelReader_ != nullptr
                             ? levelReader_(levelReaderContext_)
                             : gpio_get_level(config_.pin);
    if (rawLevel != 0 && rawLevel != 1)
    {
        return IsspResult::Failed;
    }
    return processSample(static_cast<std::uint32_t>(rawLevel));
}

IsspResult DigitalInputBehavior::processSample(std::uint32_t level)
{
    if (level == config_.activeLevel)
    {
        ++activeSampleCount_;
    }
    ++sampleCount_;

    if (sampleCount_ < config_.samplesPerWindow)
    {
        return IsspResult::Ok;
    }

    const bool classification = activeSampleCount_ >= config_.majorityThreshold;
    sampleCount_ = 0;
    activeSampleCount_ = 0;

    if (hasWindowClassification_ && classification == lastWindowClassification_)
    {
        if (consecutiveClassificationCount_ < config_.consecutiveWindows)
        {
            ++consecutiveClassificationCount_;
        }
    }
    else
    {
        hasWindowClassification_ = true;
        lastWindowClassification_ = classification;
        consecutiveClassificationCount_ = 1;
    }

    if (consecutiveClassificationCount_ < config_.consecutiveWindows ||
        (hasConfirmedState_ && confirmedState_ == classification))
    {
        return IsspResult::Ok;
    }

    return publishConfirmedState(classification, !hasConfirmedState_);
}

IsspResult DigitalInputBehavior::publishConfirmedState(bool state, bool initial)
{
    if (publisher_ == nullptr)
    {
        return IsspResult::NotReady;
    }

    if (!initial || config_.reportOnStart)
    {
        const IsspReport report = {
            .endpointId = config_.endpointId,
            .eventType = config_.eventType,
            .value = static_cast<std::uint8_t>(state ? 1U : 0U),
        };
        const IsspResult result = publisher_->publishState(report);
        if (result != IsspResult::Ok)
        {
            return result;
        }
        ESP_LOGI(kTag, "%s endpoint=%u event=%u value=%u",
                 initial ? "initial_report" : "transition_report",
                 static_cast<unsigned>(report.endpointId),
                 static_cast<unsigned>(report.eventType),
                 static_cast<unsigned>(report.value));
    }

    confirmedState_ = state;
    hasConfirmedState_ = true;
    return IsspResult::Ok;
}

bool DigitalInputBehavior::accepts(const IsspCommand &command) const
{
    return command.endpointId == config_.endpointId &&
           command.eventType == config_.eventType;
}

IsspCommandResult DigitalInputBehavior::handle(const IsspCommand &command)
{
    (void)command;
    return IsspCommandResult::Unsupported;
}

bool DigitalInputBehavior::hasConfirmedState() const
{
    return hasConfirmedState_;
}

bool DigitalInputBehavior::state() const
{
    return confirmedState_;
}

} // namespace issp
