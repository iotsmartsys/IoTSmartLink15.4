#include "issp154_transport.h"

#include <string.h>

#include "esp_attr.h"
#include "esp_ieee802154.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "iot154_radio.h"

static const EventBits_t TX_WAIT_DONE_BIT = BIT0;
static const EventBits_t TX_WAIT_FAILED_BIT = BIT1;

enum {
    ISSP154_RX_FRAME_CAPACITY = 128,
    ISSP154_RX_QUEUE_LENGTH = 8,
    ISSP154_RX_TASK_STACK_SIZE = 4096,
    ISSP154_RX_TASK_PRIORITY = tskIDLE_PRIORITY + 4,
};

typedef struct {
    size_t frame_length;
    uint8_t frame[ISSP154_RX_FRAME_CAPACITY];
    esp_ieee802154_frame_info_t frame_info;
} issp154_rx_event_t;

typedef enum {
    TX_SYNC_IDLE,
    TX_SYNC_WAITING,
    TX_SYNC_TIMED_OUT_PENDING_CALLBACK,
} tx_sync_state_t;

static issp154_transport_rx_done_cb_t s_rx_done_cb;
static issp154_transport_tx_done_cb_t s_tx_done_cb;
static issp154_transport_tx_failed_cb_t s_tx_failed_cb;
static void *s_context;
static EventGroupHandle_t s_tx_wait_events;
static portMUX_TYPE s_tx_wait_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized;
static tx_sync_state_t s_tx_sync_state;
static EventBits_t s_tx_completion;
static const uint8_t *s_waiting_tx_frame;
static esp_ieee802154_tx_error_t s_waiting_tx_error;
static bool s_defer_rx_to_task;
static QueueHandle_t s_rx_queue;
static StaticQueue_t s_rx_queue_control;
static uint8_t s_rx_queue_storage[ISSP154_RX_QUEUE_LENGTH * sizeof(issp154_rx_event_t)];
static TaskHandle_t s_rx_task;
static StaticTask_t s_rx_task_control;
static StackType_t s_rx_task_stack[ISSP154_RX_TASK_STACK_SIZE / sizeof(StackType_t)];
static volatile uint32_t s_rx_drop_count;

static void rx_task(void *context)
{
    (void)context;
    issp154_rx_event_t event;

    for (;;) {
        if (xQueueReceive(s_rx_queue, &event, portMAX_DELAY) == pdTRUE &&
            s_rx_done_cb != NULL) {
            s_rx_done_cb(event.frame, &event.frame_info, s_context);
        }
    }
}

static void IRAM_ATTR transport_rx_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    if (!s_defer_rx_to_task) {
        if (s_rx_done_cb != NULL) {
            s_rx_done_cb(frame, frame_info, s_context);
        }
        return;
    }

    BaseType_t task_woken = pdFALSE;
    bool queued = false;
    bool queue_attempted = false;
    if (frame != NULL) {
        const uint8_t physical_length = frame[0];
        const size_t frame_length = (size_t)physical_length + 1U;
        if (physical_length > 0 && physical_length <= 127 &&
            frame_length <= ISSP154_RX_FRAME_CAPACITY && s_rx_queue != NULL) {
            issp154_rx_event_t event = {0};
            event.frame_length = frame_length;
            memcpy(event.frame, frame, frame_length);
            if (frame_info != NULL) {
                event.frame_info = *frame_info;
            }
            queue_attempted = true;
            queued = xQueueSendFromISR(s_rx_queue, &event, &task_woken) == pdTRUE;
        }

        esp_ieee802154_receive_handle_done(frame);
    }

    if (queue_attempted && !queued) {
        ++s_rx_drop_count;
    }
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR(task_woken);
    }
}

static void IRAM_ATTR transport_tx_done(const uint8_t *frame,
                                        const uint8_t *ack,
                                        esp_ieee802154_frame_info_t *ack_info)
{
    bool signal_waiter = false;
    portENTER_CRITICAL_ISR(&s_tx_wait_lock);
    if (s_tx_sync_state == TX_SYNC_WAITING && frame == s_waiting_tx_frame) {
        s_tx_completion = TX_WAIT_DONE_BIT;
        signal_waiter = true;
    } else if (s_tx_sync_state == TX_SYNC_TIMED_OUT_PENDING_CALLBACK &&
               frame == s_waiting_tx_frame) {
        s_waiting_tx_frame = NULL;
        s_tx_sync_state = TX_SYNC_IDLE;
    }
    portEXIT_CRITICAL_ISR(&s_tx_wait_lock);

    if (signal_waiter) {
        BaseType_t task_woken = pdFALSE;
        xEventGroupSetBitsFromISR(s_tx_wait_events, TX_WAIT_DONE_BIT, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }

    if (s_tx_done_cb != NULL) {
        s_tx_done_cb(frame, ack, ack_info, s_context);
    }
}

static void IRAM_ATTR transport_tx_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error)
{
    bool signal_waiter = false;
    portENTER_CRITICAL_ISR(&s_tx_wait_lock);
    if (s_tx_sync_state == TX_SYNC_WAITING && frame == s_waiting_tx_frame) {
        s_waiting_tx_error = error;
        s_tx_completion = TX_WAIT_FAILED_BIT;
        signal_waiter = true;
    } else if (s_tx_sync_state == TX_SYNC_TIMED_OUT_PENDING_CALLBACK &&
               frame == s_waiting_tx_frame) {
        s_waiting_tx_error = error;
        s_waiting_tx_frame = NULL;
        s_tx_sync_state = TX_SYNC_IDLE;
    }
    portEXIT_CRITICAL_ISR(&s_tx_wait_lock);

    if (signal_waiter) {
        BaseType_t task_woken = pdFALSE;
        xEventGroupSetBitsFromISR(s_tx_wait_events, TX_WAIT_FAILED_BIT, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }

    if (s_tx_failed_cb != NULL) {
        s_tx_failed_cb(frame, error, s_context);
    }
}

esp_err_t issp154_transport_init(const issp154_transport_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_tx_wait_events == NULL) {
        s_tx_wait_events = xEventGroupCreate();
        if (s_tx_wait_events == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (config->defer_rx_to_task) {
        if (s_rx_queue == NULL) {
            s_rx_queue = xQueueCreateStatic(ISSP154_RX_QUEUE_LENGTH,
                                            sizeof(issp154_rx_event_t),
                                            s_rx_queue_storage,
                                            &s_rx_queue_control);
            if (s_rx_queue == NULL) {
                return ESP_ERR_NO_MEM;
            }
        }
        if (s_rx_task == NULL) {
            s_rx_task = xTaskCreateStatic(rx_task,
                                          "issp154_rx",
                                          ISSP154_RX_TASK_STACK_SIZE / sizeof(StackType_t),
                                          NULL,
                                          ISSP154_RX_TASK_PRIORITY,
                                          s_rx_task_stack,
                                          &s_rx_task_control);
            if (s_rx_task == NULL) {
                return ESP_ERR_NO_MEM;
            }
        }
        (void)xQueueReset(s_rx_queue);
    }

    s_rx_done_cb = config->rx_done_cb;
    s_tx_done_cb = config->tx_done_cb;
    s_tx_failed_cb = config->tx_failed_cb;
    s_context = config->context;
    s_defer_rx_to_task = config->defer_rx_to_task;

    const esp_err_t error = iot154_radio_init(config->channel,
                                              config->pan_id,
                                              config->short_address,
                                              config->coordinator,
                                              transport_rx_done,
                                              transport_tx_done,
                                              transport_tx_failed);
    s_initialized = error == ESP_OK;
    return error;
}

esp_err_t issp154_transport_set_extended_address(const uint8_t *extended_address)
{
    return esp_ieee802154_set_extended_address(extended_address);
}

esp_err_t issp154_transport_start(void)
{
    return iot154_radio_start_rx();
}

esp_err_t issp154_transport_sleep(void)
{
    return esp_ieee802154_sleep();
}

esp_err_t issp154_transport_send(const uint8_t *frame, bool cca)
{
    return esp_ieee802154_transmit(frame, cca);
}

esp_err_t issp154_transport_transmit_and_wait(const uint8_t *frame,
                                              bool cca,
                                              uint32_t timeout_ms)
{
    if (frame == NULL || timeout_ms == 0 || frame[0] == 0 || frame[0] > 127) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || s_tx_wait_events == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&s_tx_wait_lock);
    if (s_tx_sync_state != TX_SYNC_IDLE) {
        portEXIT_CRITICAL(&s_tx_wait_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_tx_sync_state = TX_SYNC_WAITING;
    s_tx_completion = 0;
    s_waiting_tx_frame = frame;
    portEXIT_CRITICAL(&s_tx_wait_lock);

    xEventGroupClearBits(s_tx_wait_events, TX_WAIT_DONE_BIT | TX_WAIT_FAILED_BIT);

    esp_err_t result = issp154_transport_sleep();
    if (result == ESP_OK) {
        result = issp154_transport_send(frame, cca);
    }

    if (result == ESP_OK) {
        uint64_t wait_ticks = ((uint64_t)timeout_ms * configTICK_RATE_HZ + 999U) / 1000U;
        if (wait_ticks == 0) {
            wait_ticks = 1;
        } else if (wait_ticks > portMAX_DELAY) {
            wait_ticks = portMAX_DELAY;
        }

        (void)xEventGroupWaitBits(s_tx_wait_events,
                                  TX_WAIT_DONE_BIT | TX_WAIT_FAILED_BIT,
                                  pdTRUE,
                                  pdFALSE,
                                  (TickType_t)wait_ticks);

        portENTER_CRITICAL(&s_tx_wait_lock);
        const EventBits_t completion = s_tx_completion;
        if (completion == 0) {
            s_tx_sync_state = TX_SYNC_TIMED_OUT_PENDING_CALLBACK;
        }
        portEXIT_CRITICAL(&s_tx_wait_lock);

        if ((completion & TX_WAIT_FAILED_BIT) != 0) {
            (void)s_waiting_tx_error;
            result = ESP_FAIL;
        } else if ((completion & TX_WAIT_DONE_BIT) == 0) {
            result = ESP_ERR_TIMEOUT;
        }
    }

    const esp_err_t restore_error = issp154_transport_start();

    portENTER_CRITICAL(&s_tx_wait_lock);
    if (s_tx_sync_state == TX_SYNC_WAITING) {
        s_waiting_tx_frame = NULL;
        s_tx_sync_state = TX_SYNC_IDLE;
    }
    portEXIT_CRITICAL(&s_tx_wait_lock);

    return restore_error != ESP_OK ? restore_error : result;
}

bool issp154_transport_is_synchronous_transmit_busy(void)
{
    portENTER_CRITICAL(&s_tx_wait_lock);
    const bool busy = s_tx_sync_state != TX_SYNC_IDLE;
    portEXIT_CRITICAL(&s_tx_wait_lock);
    return busy;
}

void issp154_transport_release_receive_buffer(const uint8_t *frame)
{
    esp_ieee802154_receive_handle_done((uint8_t *)frame);
}
