#pragma once

#include "issp_types.hpp"

namespace issp
{

class IBehaviorStatePublisher
{
public:
    virtual IsspResult publishState(const IsspReport &report) = 0;
    virtual ~IBehaviorStatePublisher() = default;
};

} // namespace issp
