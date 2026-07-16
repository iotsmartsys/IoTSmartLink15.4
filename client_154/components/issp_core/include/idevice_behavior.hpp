#pragma once

#include "issp_types.hpp"

namespace issp
{

class IBehaviorStatePublisher;

class IDeviceBehavior
{
public:
    virtual IsspResult begin(IBehaviorStatePublisher &publisher) = 0;
    virtual bool accepts(const IsspCommand &command) const = 0;
    virtual IsspCommandResult handle(const IsspCommand &command) = 0;
    virtual void poll()
    {
    }
    virtual ~IDeviceBehavior() = default;
};

} // namespace issp
