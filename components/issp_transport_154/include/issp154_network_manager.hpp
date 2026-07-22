#pragma once

#include <array>
#include <cstdint>

#include "issp154_transport.hpp"

namespace issp
{

inline constexpr std::uint8_t kIssp154FirstChannel = 11;
inline constexpr std::uint8_t kIssp154LastChannel = 26;
inline constexpr std::uint16_t kIssp154WildcardPanId = 0xffff;

struct Issp154NetworkDescriptor
{
    std::uint8_t schemaVersion;
    std::uint8_t channel;
    std::uint16_t panId;
    std::array<std::uint8_t, kIssp154ExtendedAddressSize> coordinatorAddress;
};

class Issp154NetworkManager
{
public:
    Issp154NetworkManager(Issp154Transport &transport,
                          std::uint32_t deviceId);

    IsspResult initializeNetwork();
    IsspResult loadPersistedNetwork(Issp154NetworkDescriptor &descriptor) const;
    IsspResult clearPersistedNetwork() const;

private:
    bool isValid(const Issp154NetworkDescriptor &descriptor) const;
    IsspResult persistNetwork(const Issp154NetworkDescriptor &descriptor) const;
    IsspResult activateNetwork(const Issp154NetworkDescriptor &descriptor);

    Issp154Transport &transport_;
    std::uint32_t deviceId_;
    std::uint16_t nextDiscoverySequence_;
};

} // namespace issp
