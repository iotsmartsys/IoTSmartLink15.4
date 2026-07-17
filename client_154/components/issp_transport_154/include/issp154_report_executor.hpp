#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "issp_device.hpp"
#include "issp154_transport.hpp"

namespace issp
{

class Issp154ReportExecutor
{
public:
    Issp154ReportExecutor(IsspDevice &device, Issp154Transport &transport);

    IsspResult start();
    IsspResult processOne();

private:
    static constexpr std::size_t kTaskStackSizeBytes = 4096;
    static constexpr std::size_t kTaskStackDepth =
        kTaskStackSizeBytes / sizeof(StackType_t);

    static void runTask(void *context);
    static void notifyPendingReport(void *context);
    void run();

    IsspDevice &device_;
    Issp154Transport &transport_;
    StaticTask_t taskControl_;
    StackType_t taskStack_[kTaskStackDepth];
    TaskHandle_t taskHandle_;
};

} // namespace issp
