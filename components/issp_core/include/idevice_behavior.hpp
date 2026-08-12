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
    /// Terminal, idempotent quiescence for the current boot: the behavior stops
    /// producing work on its own and publishes nothing else. It never destroys
    /// the object and never allows a restart in the same boot.
    ///
    /// This default implementation is only valid for a behavior that produces no
    /// autonomous work; a behavior with its own timer or task must override it.
    /// When begin() never completed successfully, the operation is a no-op that
    /// returns IsspResult::Ok.
    virtual IsspResult quiesce()
    {
        return IsspResult::Ok;
    }
    virtual ~IDeviceBehavior() = default;
};

} // namespace issp
