#include "issp154_network_manager.hpp"

#include <algorithm>

#include "esp_log.h"
#include "nvs.h"

namespace issp
{
namespace
{

constexpr char kTag[] = "COMMISSIONING";
constexpr char kNvsNamespace[] = "iot154";
constexpr char kNetworkKey[] = "network";
constexpr char kLegacyCentralKey[] = "central";
constexpr std::uint8_t kSchemaVersion = 1;
constexpr std::size_t kSerializedSize = 12;

bool addressIsUniform(
    const std::array<std::uint8_t, kIssp154ExtendedAddressSize> &address,
    std::uint8_t value)
{
    return std::all_of(address.begin(), address.end(),
                       [value](std::uint8_t current) { return current == value; });
}

} // namespace

Issp154NetworkManager::Issp154NetworkManager(Issp154Transport &transport,
                                             std::uint32_t deviceId)
    : transport_(transport),
      deviceId_(deviceId),
      nextDiscoverySequence_(0)
{
}

bool Issp154NetworkManager::isValid(
    const Issp154NetworkDescriptor &descriptor) const
{
    return descriptor.schemaVersion == kSchemaVersion &&
        descriptor.channel >= kIssp154FirstChannel &&
        descriptor.channel <= kIssp154LastChannel &&
        descriptor.panId != kIssp154WildcardPanId &&
        !addressIsUniform(descriptor.coordinatorAddress, 0x00) &&
        !addressIsUniform(descriptor.coordinatorAddress, 0xff);
}

IsspResult Issp154NetworkManager::loadPersistedNetwork(
    Issp154NetworkDescriptor &descriptor) const
{
    descriptor = {};
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return IsspResult::NotReady;
    }
    if (error != ESP_OK) {
        return IsspResult::Failed;
    }

    std::array<std::uint8_t, kSerializedSize> serialized{};
    std::size_t length = serialized.size();
    error = nvs_get_blob(handle, kNetworkKey, serialized.data(), &length);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return IsspResult::NotReady;
    }
    if (error != ESP_OK || length != serialized.size()) {
        return IsspResult::Failed;
    }

    descriptor.schemaVersion = serialized[0];
    descriptor.channel = serialized[1];
    descriptor.panId = static_cast<std::uint16_t>(serialized[2]) |
        (static_cast<std::uint16_t>(serialized[3]) << 8U);
    std::copy_n(serialized.begin() + 4,
                descriptor.coordinatorAddress.size(),
                descriptor.coordinatorAddress.begin());
    return isValid(descriptor) ? IsspResult::Ok : IsspResult::Failed;
}

IsspResult Issp154NetworkManager::persistNetwork(
    const Issp154NetworkDescriptor &descriptor) const
{
    if (!isValid(descriptor)) {
        return IsspResult::InvalidArgument;
    }

    std::array<std::uint8_t, kSerializedSize> serialized{};
    serialized[0] = descriptor.schemaVersion;
    serialized[1] = descriptor.channel;
    serialized[2] = static_cast<std::uint8_t>(descriptor.panId);
    serialized[3] = static_cast<std::uint8_t>(descriptor.panId >> 8U);
    std::copy(descriptor.coordinatorAddress.begin(),
              descriptor.coordinatorAddress.end(),
              serialized.begin() + 4);

    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        return IsspResult::Failed;
    }
    error = nvs_set_blob(handle, kNetworkKey,
                         serialized.data(), serialized.size());
    if (error == ESP_OK) {
        (void)nvs_erase_key(handle, kLegacyCentralKey);
        error = nvs_commit(handle);
    }
    nvs_close(handle);
    return error == ESP_OK ? IsspResult::Ok : IsspResult::Failed;
}

IsspResult Issp154NetworkManager::activateNetwork(
    const Issp154NetworkDescriptor &descriptor)
{
    IsspResult result = transport_.configureNetwork(
        descriptor.channel, descriptor.panId, false);
    if (result != IsspResult::Ok) {
        return result;
    }
    result = transport_.begin();
    if (result != IsspResult::Ok) {
        return result;
    }
    result = transport_.setDestination(descriptor.coordinatorAddress.data(),
                                       descriptor.coordinatorAddress.size());
    if (result != IsspResult::Ok) {
        (void)transport_.end();
    }
    return result;
}

IsspResult Issp154NetworkManager::initializeNetwork()
{
    Issp154NetworkDescriptor descriptor{};
    const IsspResult loadResult = loadPersistedNetwork(descriptor);
    if (loadResult == IsspResult::Ok) {
        ESP_LOGI(kTag, "persisted_network loaded channel=%u pan_id=0x%04x",
                 descriptor.channel, descriptor.panId);
        return activateNetwork(descriptor);
    }
    if (loadResult != IsspResult::NotReady) {
        ESP_LOGE(kTag, "persisted_network invalid");
        return IsspResult::Failed;
    }

    ESP_LOGI(kTag, "scan started channels=%u-%u",
             kIssp154FirstChannel, kIssp154LastChannel);
    for (std::uint8_t channel = kIssp154FirstChannel;
         channel <= kIssp154LastChannel;
         ++channel) {
        ESP_LOGI(kTag, "channel=%u attempts=3", channel);
        IsspResult result = transport_.configureNetwork(
            channel, kIssp154WildcardPanId, true);
        if (result != IsspResult::Ok) {
            return result;
        }
        result = transport_.begin();
        if (result != IsspResult::Ok) {
            return result;
        }

        Issp154DiscoveredNetwork discovered{};
        result = transport_.discoverNetwork(
            deviceId_, nextDiscoverySequence_++, discovered);
        const IsspResult endResult = transport_.end();
        if (endResult != IsspResult::Ok) {
            return endResult;
        }
        if (result != IsspResult::Ok) {
            continue;
        }

        descriptor = {
            .schemaVersion = kSchemaVersion,
            .channel = channel,
            .panId = discovered.panId,
            .coordinatorAddress = discovered.coordinatorAddress,
        };
        if (!isValid(descriptor)) {
            continue;
        }

        ESP_LOGI(kTag, "network discovered channel=%u pan_id=0x%04x",
                 descriptor.channel, descriptor.panId);
        result = persistNetwork(descriptor);
        if (result != IsspResult::Ok) {
            ESP_LOGE(kTag, "network persistence failed");
            return result;
        }
        ESP_LOGI(kTag, "network persisted");
        return activateNetwork(descriptor);
    }

    ESP_LOGW(kTag, "scan completed result=not_found");
    return IsspResult::NotReady;
}

IsspResult Issp154NetworkManager::clearPersistedNetwork() const
{
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return IsspResult::Ok;
    }
    if (error != ESP_OK) {
        return IsspResult::Failed;
    }

    const esp_err_t networkError = nvs_erase_key(handle, kNetworkKey);
    const esp_err_t legacyError = nvs_erase_key(handle, kLegacyCentralKey);
    if ((networkError != ESP_OK && networkError != ESP_ERR_NVS_NOT_FOUND) ||
        (legacyError != ESP_OK && legacyError != ESP_ERR_NVS_NOT_FOUND)) {
        nvs_close(handle);
        return IsspResult::Failed;
    }
    error = nvs_commit(handle);
    nvs_close(handle);
    return error == ESP_OK ? IsspResult::Ok : IsspResult::Failed;
}

} // namespace issp
