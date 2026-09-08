#include "light_sensor_behavior.hpp"

#include "esp_log.h"
#include "ibehavior_state_publisher.hpp"

namespace issp
{
namespace
{
constexpr char kTag[] = "LightSensor";
constexpr int kAdcMax = 4095;
}

LightSensorBehavior::LightSensorBehavior(const LightSensorConfig &config)
    : config_(config)
{
}

IsspResult LightSensorBehavior::begin(IBehaviorStatePublisher &publisher)
{
    if (quiesced_.load() || started_.exchange(true))
    {
        return IsspResult::NotReady;
    }
    if (config_.endpointId == 0U || config_.unit != ADC_UNIT_1 ||
        config_.channel != ADC_CHANNEL_1 || config_.attenuation != ADC_ATTEN_DB_12)
    {
        return IsspResult::InvalidArgument;
    }

    // Resource lifetime is restricted to this acquisition, including failures.
    adc_oneshot_unit_handle_t adc = nullptr;
    const adc_oneshot_unit_init_cfg_t unitConfig = {
        .unit_id = config_.unit,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t result = adc_oneshot_new_unit(&unitConfig, &adc);
    if (result != ESP_OK)
    {
        ESP_LOGE(kTag, "adc_init failed endpoint=%u error=%s", config_.endpointId,
                 esp_err_to_name(result));
        // Operational but without evidence of admission: only the deadline
        // may authorize sleep. Do not turn an ADC failure into a valid zero.
        return IsspResult::Ok;
    }
    const adc_oneshot_chan_cfg_t channelConfig = {
        .atten = config_.attenuation,
        .bitwidth = ADC_BITWIDTH_12,
    };
    result = adc_oneshot_config_channel(adc, config_.channel, &channelConfig);
    int raw = -1;
    if (result == ESP_OK && !quiesced_.load())
    {
        result = adc_oneshot_read(adc, config_.channel, &raw);
    }
    const esp_err_t releaseResult = adc_oneshot_del_unit(adc);
    if (releaseResult != ESP_OK)
    {
        ESP_LOGE(kTag, "adc_release failed endpoint=%u error=%s", config_.endpointId,
                 esp_err_to_name(releaseResult));
    }
    if (result != ESP_OK || raw < 0 || raw > kAdcMax)
    {
        ESP_LOGE(kTag, "acquisition invalid endpoint=%u raw=%d error=%s",
                 config_.endpointId, raw, esp_err_to_name(result));
        return IsspResult::Ok;
    }

    ESP_LOGI(kTag, "ADC: %d | Light: %.1f%%", raw,
             static_cast<double>(raw) * 100.0 / kAdcMax);
    if (quiesced_.load())
    {
        return IsspResult::Ok;
    }
    const auto percent = static_cast<std::uint8_t>(
        (100U * static_cast<std::uint32_t>(raw) + 2047U) / kAdcMax);
    const IsspResult admission = publisher.publishState({
        .endpointId = config_.endpointId,
        .eventType = kEventType,
        .value = percent,
    });
    if (admission == IsspResult::Ok)
    {
        admitted_.store(true);
    }
    else
    {
        ESP_LOGW(kTag, "report admission refused endpoint=%u result=%u",
                 config_.endpointId, static_cast<unsigned>(admission));
    }
    // Refusal is observable above and never retried by this producer. Keeping
    // the lifecycle operational lets its deadline enforce the sleep policy.
    return IsspResult::Ok;
}

bool LightSensorBehavior::accepts(const IsspCommand &command) const
{
    return command.endpointId == config_.endpointId && command.eventType == kEventType;
}

IsspCommandResult LightSensorBehavior::handle(const IsspCommand &command)
{
    (void)command;
    return IsspCommandResult::Unsupported;
}

IsspResult LightSensorBehavior::quiesce()
{
    quiesced_.store(true);
    return IsspResult::Ok;
}

bool LightSensorBehavior::hasAdmittedReport() const
{
    return admitted_.load();
}

} // namespace issp
