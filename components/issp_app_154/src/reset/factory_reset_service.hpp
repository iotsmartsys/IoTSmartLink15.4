#pragma once

#include "ifactory_reset_requester.hpp"

#include "esp_err.h"

using FactoryResetCleanup = esp_err_t (*)(void *context);

// Arbitration of the single power transition shared with deep sleep. The
// facade owns the three-state holder; this service only asks for it, so the
// network-binding cleanup and the commit of a deep sleep can never overlap.
// A service built without an arbiter behaves as before: every request wins.
struct FactoryResetArbiter
{
    bool (*acquire)(void *context);
    void (*release)(void *context);
    void *context;
};

class FactoryResetService final : public IFactoryResetRequester
{
public:
    FactoryResetService(FactoryResetCleanup cleanup, void *cleanupContext);
    FactoryResetService(FactoryResetCleanup cleanup,
                        void *cleanupContext,
                        const FactoryResetArbiter &arbiter);

    FactoryResetRequestResult requestFactoryReset() override;

private:
    bool requested_;
    FactoryResetCleanup cleanup_;
    void *cleanupContext_;
    FactoryResetArbiter arbiter_;
};
