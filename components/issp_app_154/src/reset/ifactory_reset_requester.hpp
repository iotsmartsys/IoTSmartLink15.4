#pragma once

class IFactoryResetRequester
{
public:
    virtual ~IFactoryResetRequester() = default;
    virtual void requestFactoryReset() = 0;
};
