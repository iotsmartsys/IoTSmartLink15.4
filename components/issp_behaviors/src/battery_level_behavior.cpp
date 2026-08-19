#include "battery_level_behavior.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "ibehavior_state_publisher.hpp"
#include "sdkconfig.h"

namespace issp
{

namespace
{
constexpr char kTag[] = "BATTERY_LEVEL";

void delayMilliseconds(std::uint32_t milliseconds)
{
    constexpr std::uint32_t kMaximumChunkMs =
        std::numeric_limits<std::uint32_t>::max() / 1000U;
    while (milliseconds != 0U)
    {
        const std::uint32_t chunk = std::min(milliseconds, kMaximumChunkMs);
        esp_rom_delay_us(chunk * 1000U);
        milliseconds -= chunk;
    }
}
} // namespace

BatteryLevelBehavior::BatteryLevelBehavior(const BatteryLevelConfig &config)
    : config_(config),
      publisher_(nullptr),
      adcUnit_(nullptr),
      calibration_(nullptr),
      timer_(nullptr),
      timerStarted_(false),
      inert_(false),
      telemetryState_(TelemetryState::Inert),
      telemetryStateListener_(nullptr),
      telemetryStateListenerContext_(nullptr),
      hasPublishedPercentage_(false),
      lastPublishedPercentage_(0)
{
}

BatteryLevelBehavior::~BatteryLevelBehavior()
{
    stopAndDeleteTimer();
    releaseAdc();
    publisher_ = nullptr;
}

bool BatteryLevelBehavior::validConfig() const
{
    return config_.samples != 0U && config_.reportDeltaPercent >= 1U &&
           config_.reportDeltaPercent <= 100U && config_.fullMv > config_.emptyMv &&
           config_.rBottomOhms != 0U && config_.endpointId != 0U;
}

BatteryLevelBehavior::TelemetryState BatteryLevelBehavior::telemetryState() const
{
    return telemetryState_;
}

void BatteryLevelBehavior::setTelemetryStateListener(
    TelemetryStateListener listener, void *context)
{
    telemetryStateListener_ = listener;
    telemetryStateListenerContext_ = context;
}

void BatteryLevelBehavior::setTelemetryState(TelemetryState state)
{
    if (telemetryState_ == state)
    {
        return;
    }
    telemetryState_ = state;
    if (telemetryStateListener_ != nullptr)
    {
        telemetryStateListener_(telemetryStateListenerContext_, state);
    }
}

bool BatteryLevelBehavior::initializeAdc()
{
    const adc_oneshot_unit_init_cfg_t unitConfig = {
        .unit_id = config_.unit,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t result = adc_oneshot_new_unit(&unitConfig, &adcUnit_);
    if (result != ESP_OK)
    {
        ESP_LOGE(kTag, "adc_unit_config failed endpoint=%u error=%s",
                 static_cast<unsigned>(config_.endpointId), esp_err_to_name(result));
        adcUnit_ = nullptr;
        setTelemetryState(TelemetryState::Inert);
        return false;
    }

    const adc_oneshot_chan_cfg_t channelConfig = {
        .atten = config_.attenuation,
        .bitwidth = kBitwidth,
    };
    result = adc_oneshot_config_channel(adcUnit_, config_.channel, &channelConfig);
    if (result != ESP_OK)
    {
        ESP_LOGE(kTag, "adc_channel_config failed endpoint=%u error=%s",
                 static_cast<unsigned>(config_.endpointId), esp_err_to_name(result));
        releaseAdc();
        return false;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    const adc_cali_curve_fitting_config_t calibrationConfig = {
        .unit_id = config_.unit,
        .chan = config_.channel,
        .atten = config_.attenuation,
        .bitwidth = kBitwidth,
    };
    result = adc_cali_create_scheme_curve_fitting(&calibrationConfig, &calibration_);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    const adc_cali_line_fitting_config_t calibrationConfig = {
        .unit_id = config_.unit,
        .atten = config_.attenuation,
        .bitwidth = kBitwidth,
    };
    result = adc_cali_create_scheme_line_fitting(&calibrationConfig, &calibration_);
#else
    result = ESP_ERR_NOT_SUPPORTED;
#endif

    if (result != ESP_OK)
    {
        calibration_ = nullptr;
        ESP_LOGW(kTag,
                 "adc_calibration unavailable endpoint=%u error=%s "
                 "fallback_full_scale_mv=%u",
                 static_cast<unsigned>(config_.endpointId), esp_err_to_name(result),
                 static_cast<unsigned>(fallbackFullScaleMv()));
    }
    setTelemetryState(calibration_ != nullptr ? TelemetryState::Calibrated
                                               : TelemetryState::Approximate);
    return true;
}

void BatteryLevelBehavior::releaseAdc()
{
    if (calibration_ != nullptr)
    {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        (void)adc_cali_delete_scheme_curve_fitting(calibration_);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        (void)adc_cali_delete_scheme_line_fitting(calibration_);
#endif
        calibration_ = nullptr;
    }
    if (adcUnit_ != nullptr)
    {
        (void)adc_oneshot_del_unit(adcUnit_);
        adcUnit_ = nullptr;
    }
}

IsspResult BatteryLevelBehavior::createAndStartTimer()
{
    const esp_timer_create_args_t timerArgs = {
        .callback = &BatteryLevelBehavior::timerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "issp_battery",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&timerArgs, &timer_) != ESP_OK)
    {
        return IsspResult::Failed;
    }

    const std::uint64_t periodUs =
        static_cast<std::uint64_t>(config_.samplePeriodMs) * 1000ULL;
    if (esp_timer_start_periodic(timer_, periodUs) != ESP_OK)
    {
        stopAndDeleteTimer();
        return IsspResult::Failed;
    }
    timerStarted_ = true;
    return IsspResult::Ok;
}

void BatteryLevelBehavior::stopAndDeleteTimer()
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

IsspResult BatteryLevelBehavior::begin(IBehaviorStatePublisher &publisher)
{
    if (!validConfig() || publisher_ != nullptr)
    {
        return IsspResult::InvalidArgument;
    }

    publisher_ = &publisher;
    if (!initializeAdc())
    {
        inert_ = true;
        return IsspResult::Ok;
    }

    if (config_.samplePeriodMs == 0U)
    {
        const IsspResult initialResult = measureAndMaybePublish();
        if (initialResult != IsspResult::Ok)
        {
            releaseAdc();
            publisher_ = nullptr;
            return initialResult;
        }
        return IsspResult::Ok;
    }

    return IsspResult::Ok;
}

IsspResult BatteryLevelBehavior::startDeferredSampling()
{
    if (config_.samplePeriodMs == 0U || publisher_ == nullptr || inert_ ||
        adcUnit_ == nullptr || timer_ != nullptr)
    {
        return IsspResult::Ok;
    }
    return createAndStartTimer();
}

void BatteryLevelBehavior::timerCallback(void *context)
{
    if (context == nullptr)
    {
        return;
    }
    auto *self = static_cast<BatteryLevelBehavior *>(context);
    const IsspResult result = self->measureAndMaybePublish();
    if (result != IsspResult::Ok)
    {
        ESP_LOGW(kTag, "periodic_report failed endpoint=%u result=%u",
                 static_cast<unsigned>(self->config_.endpointId),
                 static_cast<unsigned>(result));
    }
}

bool BatteryLevelBehavior::readPinMillivolts(std::uint32_t &pinMv)
{
    int raw = 0;
    const esp_err_t readResult = adc_oneshot_read(adcUnit_, config_.channel, &raw);
    if (readResult != ESP_OK)
    {
        ESP_LOGW(kTag, "adc_read failed endpoint=%u error=%s",
                 static_cast<unsigned>(config_.endpointId),
                 esp_err_to_name(readResult));
        return false;
    }
    if (raw <= 0 || raw >= kMaximumRaw)
    {
        ESP_LOGW(kTag, "adc_sample electrically_invalid endpoint=%u raw=%d",
                 static_cast<unsigned>(config_.endpointId), raw);
        return false;
    }

    if (calibration_ != nullptr)
    {
        int calibratedMv = 0;
        const esp_err_t conversionResult =
            adc_cali_raw_to_voltage(calibration_, raw, &calibratedMv);
        if (conversionResult != ESP_OK || calibratedMv < 0)
        {
            ESP_LOGW(kTag, "adc_conversion failed endpoint=%u error=%s",
                     static_cast<unsigned>(config_.endpointId),
                     esp_err_to_name(conversionResult));
            return false;
        }
        pinMv = static_cast<std::uint32_t>(calibratedMv);
        return true;
    }

    const std::uint32_t fullScaleMv = fallbackFullScaleMv();
    if (fullScaleMv == 0U)
    {
        ESP_LOGW(kTag, "adc_fallback unavailable endpoint=%u attenuation=%u",
                 static_cast<unsigned>(config_.endpointId),
                 static_cast<unsigned>(config_.attenuation));
        return false;
    }
    const std::uint64_t scaled =
        static_cast<std::uint64_t>(raw) * fullScaleMv + kMaximumRaw / 2;
    pinMv = static_cast<std::uint32_t>(scaled / kMaximumRaw);
    return true;
}

std::uint32_t BatteryLevelBehavior::fallbackFullScaleMv() const
{
#if defined(CONFIG_IDF_TARGET_ESP32H2)
    // ESP32-H2 Series Datasheet, ADC characteristics: effective measurement
    // ranges for ATTEN0..ATTEN3. This target fact is intentionally kept out of
    // the generic configuration and out of the product firmware.
    switch (config_.attenuation)
    {
    case ADC_ATTEN_DB_0:
        return 1000U;
    case ADC_ATTEN_DB_2_5:
        return 1300U;
    case ADC_ATTEN_DB_6:
        return 1900U;
    case ADC_ATTEN_DB_12:
        return 3300U;
    }
#endif
    return 0U;
}

std::uint8_t
BatteryLevelBehavior::percentageFromBatteryMv(std::uint64_t batteryMv) const
{
    if (batteryMv <= config_.emptyMv)
    {
        return 0U;
    }
    if (batteryMv >= config_.fullMv)
    {
        return 100U;
    }

    const std::uint64_t span =
        static_cast<std::uint64_t>(config_.fullMv) - config_.emptyMv;
    const std::uint64_t offset = batteryMv - config_.emptyMv;
    return static_cast<std::uint8_t>((offset * 100U + span / 2U) / span);
}

IsspResult BatteryLevelBehavior::measureAndMaybePublish()
{
    if (inert_ || publisher_ == nullptr || adcUnit_ == nullptr)
    {
        return IsspResult::Ok;
    }

    std::uint64_t pinMillivoltSum = 0U;
    for (std::uint32_t index = 0; index < config_.samples; ++index)
    {
        std::uint32_t pinMv = 0U;
        if (!readPinMillivolts(pinMv))
        {
            return IsspResult::Ok;
        }
        pinMillivoltSum += pinMv;
        if (index + 1U < config_.samples && config_.sampleIntervalMs != 0U)
        {
            delayMilliseconds(config_.sampleIntervalMs);
        }
    }

    const std::uint64_t averagePinMv = pinMillivoltSum / config_.samples;
    const std::uint64_t dividerResistance =
        static_cast<std::uint64_t>(config_.rTopOhms) + config_.rBottomOhms;
    const std::uint64_t batteryMv =
        averagePinMv * dividerResistance / config_.rBottomOhms;
    const std::uint8_t percentage = percentageFromBatteryMv(batteryMv);

    const int difference = hasPublishedPercentage_
                               ? std::abs(static_cast<int>(percentage) -
                                          static_cast<int>(lastPublishedPercentage_))
                               : config_.reportDeltaPercent;
    if (hasPublishedPercentage_ &&
        difference < static_cast<int>(config_.reportDeltaPercent))
    {
        return IsspResult::Ok;
    }

    const IsspReport report = {
        .endpointId = config_.endpointId,
        .eventType = kEventType,
        .value = percentage,
    };
    const IsspResult publishResult = publisher_->publishState(report);
    if (publishResult != IsspResult::Ok)
    {
        return publishResult;
    }

    hasPublishedPercentage_ = true;
    lastPublishedPercentage_ = percentage;
    ESP_LOGI(kTag, "report endpoint=%u event=%u value=%u battery_mv=%llu",
             static_cast<unsigned>(report.endpointId),
             static_cast<unsigned>(report.eventType),
             static_cast<unsigned>(report.value), batteryMv);
    return IsspResult::Ok;
}

bool BatteryLevelBehavior::accepts(const IsspCommand &command) const
{
    return command.endpointId == config_.endpointId &&
           command.eventType == kEventType;
}

IsspCommandResult BatteryLevelBehavior::handle(const IsspCommand &command)
{
    (void)command;
    return IsspCommandResult::Unsupported;
}

IsspResult BatteryLevelBehavior::quiesce()
{
    stopAndDeleteTimer();
    releaseAdc();
    inert_ = true;
    ESP_LOGI(kTag, "quiesced endpoint=%u event=%u",
             static_cast<unsigned>(config_.endpointId),
             static_cast<unsigned>(kEventType));
    return IsspResult::Ok;
}

} // namespace issp
