#pragma once

#include "ifactory_reset_requester.hpp"

class FactoryResetService final : public IFactoryResetRequester
{
public:
    FactoryResetService();

    void requestFactoryReset() override;

private:
    bool requested_;
};
