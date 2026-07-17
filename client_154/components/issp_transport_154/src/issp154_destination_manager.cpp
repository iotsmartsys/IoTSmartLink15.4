#include "issp154_destination_manager.hpp"

#include "nvs.h"

namespace issp
{
namespace
{

constexpr char kNvsNamespace[] = "iot154";
constexpr char kCentralKey[] = "central";

} // namespace

Issp154DestinationManager::Issp154DestinationManager(
    Issp154Transport &transport,
    std::uint32_t deviceId)
    : transport_(transport),
      deviceId_(deviceId),
      nextDiscoverySequence_(0)
{
}

IsspResult Issp154DestinationManager::loadPersistedDestination(
    std::array<std::uint8_t, kIssp154ExtendedAddressSize> &destination) const
{
    destination.fill(0);
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        return IsspResult::NotReady;
    }
    if (error != ESP_OK) {
        return IsspResult::Failed;
    }

    std::size_t length = 0;
    error = nvs_get_blob(handle, kCentralKey, nullptr, &length);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return IsspResult::NotReady;
    }
    if (error != ESP_OK || length != destination.size()) {
        nvs_close(handle);
        return IsspResult::Failed;
    }

    error = nvs_get_blob(handle, kCentralKey, destination.data(), &length);
    nvs_close(handle);
    if (error != ESP_OK || length != destination.size()) {
        destination.fill(0);
        return IsspResult::Failed;
    }
    return IsspResult::Ok;
}

IsspResult Issp154DestinationManager::persistDestination(
    const std::array<std::uint8_t, kIssp154ExtendedAddressSize> &destination) const
{
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        return IsspResult::Failed;
    }

    error = nvs_set_blob(handle,
                         kCentralKey,
                         destination.data(),
                         destination.size());
    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }
    nvs_close(handle);
    return error == ESP_OK ? IsspResult::Ok : IsspResult::Failed;
}

IsspResult Issp154DestinationManager::initializeDestination()
{
    std::array<std::uint8_t, kIssp154ExtendedAddressSize> destination{};
    const IsspResult loadResult = loadPersistedDestination(destination);
    if (loadResult == IsspResult::Ok) {
        return transport_.setDestination(destination.data(), destination.size());
    }
    if (loadResult != IsspResult::NotReady) {
        return IsspResult::Failed;
    }

    const std::uint16_t sequence = nextDiscoverySequence_++;
    const IsspResult discoveryResult =
        transport_.discoverDestination(deviceId_, sequence, destination);
    if (discoveryResult != IsspResult::Ok) {
        return discoveryResult;
    }

    if (persistDestination(destination) != IsspResult::Ok) {
        return IsspResult::Failed;
    }
    return transport_.setDestination(destination.data(), destination.size());
}

} // namespace issp
