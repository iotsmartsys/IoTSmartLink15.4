#pragma once

enum class FactoryResetRequestResult
{
    // The request won the power transition and the reset sequence is running.
    Accepted,
    // Deep sleep already holds the transition in this boot. The request was
    // diagnosed and rejected; it does not consume the hold in course, so the
    // monitor may present it again without requiring a release and a new press.
    Rejected,
};

class IFactoryResetRequester
{
public:
    virtual ~IFactoryResetRequester() = default;
    virtual FactoryResetRequestResult requestFactoryReset() = 0;
};
