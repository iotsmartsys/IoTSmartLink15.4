#pragma once

#include <array>
#include <cstdint>

#include "issp154_transport.hpp"

namespace issp
{

class Issp154DestinationManager
{
public:
    Issp154DestinationManager(Issp154Transport &transport,
                              std::uint32_t deviceId);

    IsspResult loadPersistedDestination(
        std::array<std::uint8_t, kIssp154ExtendedAddressSize> &destination) const;
    IsspResult persistDestination(
        const std::array<std::uint8_t, kIssp154ExtendedAddressSize> &destination) const;
    IsspResult initializeDestination();

private:
    Issp154Transport &transport_;
    std::uint32_t deviceId_;
    std::uint16_t nextDiscoverySequence_;
};

} // namespace issp
