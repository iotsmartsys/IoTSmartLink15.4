#include "issp154_transport.h"

#include <string.h>

#include "esp_attr.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
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

static const char DRAM_ATTR s_tx_error_none[] = "NONE";
static const char DRAM_ATTR s_tx_error_cca_busy[] = "CCA_BUSY";
static const char DRAM_ATTR s_tx_error_abort[] = "ABORT";
static const char DRAM_ATTR s_tx_error_no_ack[] = "NO_ACK";
static const char DRAM_ATTR s_tx_error_invalid_ack[] = "INVALID_ACK";
static const char DRAM_ATTR s_tx_error_coexist[] = "COEXIST";
static const char DRAM_ATTR s_tx_error_security[] = "SECURITY";
static const char DRAM_ATTR s_tx_error_unknown[] = "UNKNOWN";
static const char DRAM_ATTR s_log_yes[] = "yes";
static const char DRAM_ATTR s_log_no[] = "no";

static const char *IRAM_ATTR tx_error_name(esp_ieee802154_tx_error_t error)
{
    switch (error) {
    case ESP_IEEE802154_TX_ERR_NONE:
        return s_tx_error_none;
    case ESP_IEEE802154_TX_ERR_CCA_BUSY:
        return s_tx_error_cca_busy;
    case ESP_IEEE802154_TX_ERR_ABORT:
        return s_tx_error_abort;
    case ESP_IEEE802154_TX_ERR_NO_ACK:
        return s_tx_error_no_ack;
    case ESP_IEEE802154_TX_ERR_INVALID_ACK:
        return s_tx_error_invalid_ack;
    case ESP_IEEE802154_TX_ERR_COEXIST:
        return s_tx_error_coexist;
    case ESP_IEEE802154_TX_ERR_SECURITY:
        return s_tx_error_security;
    default:
        return s_tx_error_unknown;
    }
}

static uint8_t IRAM_ATTR frame_mac_sequence(const uint8_t *frame)
{
    return frame != NULL && frame[0] >= 3U ? frame[3] : 0U;
}

static tx_sync_state_t tx_sync_state_snapshot(void)
{
    portENTER_CRITICAL(&s_tx_wait_lock);
    const tx_sync_state_t state = s_tx_sync_state;
    portEXIT_CRITICAL(&s_tx_wait_lock);
    return state;
}

static bool waiting_frame_cleared_snapshot(void)
{
    portENTER_CRITICAL(&s_tx_wait_lock);
    const bool cleared = s_waiting_tx_frame == NULL;
    portEXIT_CRITICAL(&s_tx_wait_lock);
    return cleared;
}

static void rx_task(void *context)
{
    (void)context;
    issp154_rx_event_t event;

    for (;;) {
        if (xQueueReceive(s_rx_queue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("RX_TASK", "dequeue frame_len=%u", (unsigned)event.frame_length);
            if (s_rx_done_cb != NULL) {
                s_rx_done_cb(event.frame, &event.frame_info, s_context);
            }
        }
    }
}

static void IRAM_ATTR transport_rx_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    const unsigned physical_length = frame != NULL ? frame[0] : 0U;
    if (frame_info != NULL) {
        ESP_DRAM_LOGI(DRAM_STR("RADIO_RX"),
                      "frame received phy_len=%u rssi=%d lqi=%u",
                      physical_length,
                      (int)frame_info->rssi,
                      (unsigned)frame_info->lqi);
    } else {
        ESP_DRAM_LOGI(DRAM_STR("RADIO_RX"),
                      "frame received phy_len=%u rssi=unavailable lqi=unavailable",
                      physical_length);
    }

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
            if (queued) {
                ESP_DRAM_LOGI(DRAM_STR("RX_QUEUE"),
                              "enqueue=ok frame_len=%u",
                              (unsigned)frame_length);
            } else {
                ESP_DRAM_LOGI(DRAM_STR("RX_QUEUE"),
                              "enqueue=failed frame_len=%u",
                              (unsigned)frame_length);
            }
        } else {
            ESP_DRAM_LOGI(DRAM_STR("RX_QUEUE"),
                          "enqueue=skipped reason=invalid_frame_or_queue phy_len=%u",
                          (unsigned)physical_length);
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
    bool frame_match = false;
    tx_sync_state_t sync_state_found;
    portENTER_CRITICAL_ISR(&s_tx_wait_lock);
    sync_state_found = s_tx_sync_state;
    frame_match = frame == s_waiting_tx_frame;
    if (s_tx_sync_state == TX_SYNC_WAITING && frame == s_waiting_tx_frame) {
        s_tx_completion = TX_WAIT_DONE_BIT;
        signal_waiter = true;
    } else if (s_tx_sync_state == TX_SYNC_TIMED_OUT_PENDING_CALLBACK &&
               frame == s_waiting_tx_frame) {
        s_waiting_tx_frame = NULL;
        s_tx_sync_state = TX_SYNC_IDLE;
    }
    portEXIT_CRITICAL_ISR(&s_tx_wait_lock);

    ESP_DRAM_LOGI(DRAM_STR("PHY_TX"),
                  "callback=tx_done frame=%p mac_sequence=%u frame_match=%s sync_state=%u radio_state=%u",
                  (const void *)frame,
                  (unsigned)frame_mac_sequence(frame),
                  frame_match ? s_log_yes : s_log_no,
                  (unsigned)sync_state_found,
                  (unsigned)esp_ieee802154_get_state());

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
    bool frame_match = false;
    tx_sync_state_t sync_state_found;
    portENTER_CRITICAL_ISR(&s_tx_wait_lock);
    sync_state_found = s_tx_sync_state;
    frame_match = frame == s_waiting_tx_frame;
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

    ESP_DRAM_LOGI(DRAM_STR("PHY_TX"),
                  "callback=tx_failed frame=%p mac_sequence=%u frame_match=%s sync_state=%u error=%u error_name=%s radio_state=%u",
                  (const void *)frame,
                  (unsigned)frame_mac_sequence(frame),
                  frame_match ? s_log_yes : s_log_no,
                  (unsigned)sync_state_found,
                  (unsigned)error,
                  tx_error_name(error),
                  (unsigned)esp_ieee802154_get_state());

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
                                              config->promiscuous,
                                              transport_rx_done,
                                              transport_tx_done,
                                              transport_tx_failed);
    s_initialized = error == ESP_OK;
    return error;
}

esp_err_t issp154_transport_deinit(void)
{
    portENTER_CRITICAL(&s_tx_wait_lock);
    const bool transmit_busy = s_tx_sync_state != TX_SYNC_IDLE;
    portEXIT_CRITICAL(&s_tx_wait_lock);
    if (transmit_busy) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t error = iot154_radio_deinit();
    if (error != ESP_OK) {
        return error;
    }

    s_initialized = false;
    s_rx_done_cb = NULL;
    s_tx_done_cb = NULL;
    s_tx_failed_cb = NULL;
    s_context = NULL;
    s_defer_rx_to_task = false;
    s_tx_completion = 0;
    s_waiting_tx_frame = NULL;
    s_waiting_tx_error = 0;

    if (s_rx_task != NULL) {
        vTaskDelete(s_rx_task);
        s_rx_task = NULL;
    }
    if (s_rx_queue != NULL) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
    }
    if (s_tx_wait_events != NULL) {
        vEventGroupDelete(s_tx_wait_events);
        s_tx_wait_events = NULL;
    }
    return ESP_OK;
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
    const unsigned physical_length = frame != NULL ? frame[0] : 0U;
    const bool mac_sequence_valid = frame != NULL && physical_length >= 3U;
    ESP_LOGI("PHY_TX",
             "begin frame=%p phy_len=%u mac_sequence=%u mac_sequence_valid=%s cca=%s sync_state=%u radio_state=%u",
             (const void *)frame,
             physical_length,
             (unsigned)frame_mac_sequence(frame),
             mac_sequence_valid ? "yes" : "no",
             cca ? "true" : "false",
             (unsigned)tx_sync_state_snapshot(),
             (unsigned)esp_ieee802154_get_state());

    if (frame == NULL || timeout_ms == 0 || frame[0] == 0 || frame[0] > 127) {
        ESP_LOGI("PHY_TX",
                 "end result=%d result_name=%s reason=invalid_argument sync_state=%u radio_state=%u waiting_frame_cleared=%s",
                 ESP_ERR_INVALID_ARG,
                 esp_err_to_name(ESP_ERR_INVALID_ARG),
                 (unsigned)tx_sync_state_snapshot(),
                 (unsigned)esp_ieee802154_get_state(),
                 waiting_frame_cleared_snapshot() ? "yes" : "no");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || s_tx_wait_events == NULL) {
        ESP_LOGI("PHY_TX",
                 "end result=%d result_name=%s reason=transport_not_ready sync_state=%u radio_state=%u waiting_frame_cleared=%s",
                 ESP_ERR_INVALID_STATE,
                 esp_err_to_name(ESP_ERR_INVALID_STATE),
                 (unsigned)tx_sync_state_snapshot(),
                 (unsigned)esp_ieee802154_get_state(),
                 waiting_frame_cleared_snapshot() ? "yes" : "no");
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&s_tx_wait_lock);
    if (s_tx_sync_state != TX_SYNC_IDLE) {
        portEXIT_CRITICAL(&s_tx_wait_lock);
        ESP_LOGI("PHY_TX",
                 "end result=%d result_name=%s reason=sync_busy sync_state=%u radio_state=%u waiting_frame_cleared=no",
                 ESP_ERR_INVALID_STATE,
                 esp_err_to_name(ESP_ERR_INVALID_STATE),
                 (unsigned)tx_sync_state_snapshot(),
                 (unsigned)esp_ieee802154_get_state());
        return ESP_ERR_INVALID_STATE;
    }
    s_tx_sync_state = TX_SYNC_WAITING;
    s_tx_completion = 0;
    s_waiting_tx_frame = frame;
    portEXIT_CRITICAL(&s_tx_wait_lock);

    xEventGroupClearBits(s_tx_wait_events, TX_WAIT_DONE_BIT | TX_WAIT_FAILED_BIT);

    const char *reason = "success";
    esp_err_t result = issp154_transport_sleep();
    ESP_LOGI("PHY_TX",
             "sleep result=%d result_name=%s radio_state=%u",
             result,
             esp_err_to_name(result),
             (unsigned)esp_ieee802154_get_state());
    if (result != ESP_OK) {
        reason = "sleep_failed";
    }
    if (result == ESP_OK) {
        result = issp154_transport_send(frame, cca);
        ESP_LOGI("PHY_TX",
                 "transmit_call result=%d result_name=%s frame=%p mac_sequence=%u radio_state=%u",
                 result,
                 esp_err_to_name(result),
                 (const void *)frame,
                 (unsigned)frame_mac_sequence(frame),
                 (unsigned)esp_ieee802154_get_state());
        if (result != ESP_OK) {
            reason = "transmit_rejected";
        }
    }

    if (result == ESP_OK) {
        uint64_t wait_ticks = ((uint64_t)timeout_ms * configTICK_RATE_HZ + 999U) / 1000U;
        if (wait_ticks == 0) {
            wait_ticks = 1;
        } else if (wait_ticks > portMAX_DELAY) {
            wait_ticks = portMAX_DELAY;
        }

        const EventBits_t wait_bits = xEventGroupWaitBits(
            s_tx_wait_events,
            TX_WAIT_DONE_BIT | TX_WAIT_FAILED_BIT,
            pdTRUE,
            pdFALSE,
            (TickType_t)wait_ticks);

        portENTER_CRITICAL(&s_tx_wait_lock);
        const EventBits_t completion = s_tx_completion;
        const esp_ieee802154_tx_error_t stored_error = s_waiting_tx_error;
        const tx_sync_state_t sync_state_before_finalization = s_tx_sync_state;
        if (completion == 0) {
            s_tx_sync_state = TX_SYNC_TIMED_OUT_PENDING_CALLBACK;
        }
        portEXIT_CRITICAL(&s_tx_wait_lock);

        ESP_LOGI("PHY_TX",
                 "wait_complete bits=0x%lx completion=0x%lx done=%s failed=%s timeout=%s stored_error=%u stored_error_name=%s sync_state=%u",
                 (unsigned long)wait_bits,
                 (unsigned long)completion,
                 (completion & TX_WAIT_DONE_BIT) != 0 ? "yes" : "no",
                 (completion & TX_WAIT_FAILED_BIT) != 0 ? "yes" : "no",
                 completion == 0 ? "yes" : "no",
                 (unsigned)stored_error,
                 tx_error_name(stored_error),
                 (unsigned)sync_state_before_finalization);

        if ((completion & TX_WAIT_FAILED_BIT) != 0) {
            result = ESP_FAIL;
            reason = "tx_failed_callback";
        } else if ((completion & TX_WAIT_DONE_BIT) == 0) {
            result = ESP_ERR_TIMEOUT;
            reason = "physical_timeout";
        }
    }

    const esp_err_t restore_error = issp154_transport_start();
    ESP_LOGI("PHY_TX",
             "restore_rx result=%d result_name=%s radio_state=%u sync_state=%u",
             restore_error,
             esp_err_to_name(restore_error),
             (unsigned)esp_ieee802154_get_state(),
             (unsigned)tx_sync_state_snapshot());
    if (restore_error != ESP_OK) {
        reason = "restore_rx_failed";
    }

    portENTER_CRITICAL(&s_tx_wait_lock);
    if (s_tx_sync_state == TX_SYNC_WAITING) {
        s_waiting_tx_frame = NULL;
        s_tx_sync_state = TX_SYNC_IDLE;
    }
    const tx_sync_state_t final_sync_state = s_tx_sync_state;
    const bool waiting_frame_cleared = s_waiting_tx_frame == NULL;
    portEXIT_CRITICAL(&s_tx_wait_lock);

    const esp_err_t final_result = restore_error != ESP_OK ? restore_error : result;
    ESP_LOGI("PHY_TX",
             "end result=%d result_name=%s reason=%s sync_state=%u radio_state=%u waiting_frame_cleared=%s",
             final_result,
             esp_err_to_name(final_result),
             reason,
             (unsigned)final_sync_state,
             (unsigned)esp_ieee802154_get_state(),
             waiting_frame_cleared ? "yes" : "no");
    return final_result;
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
