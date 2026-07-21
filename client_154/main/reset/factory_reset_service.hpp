#pragma once

#include "ifactory_reset_requester.hpp"

#include "esp_err.h"

using FactoryResetCleanup = esp_err_t (*)(void *context);

class FactoryResetService final : public IFactoryResetRequester
{
public:
    FactoryResetService(FactoryResetCleanup cleanup, void *cleanupContext);

    void requestFactoryReset() override;

private:
    bool requested_;
    FactoryResetCleanup cleanup_;
    void *cleanupContext_;
};
