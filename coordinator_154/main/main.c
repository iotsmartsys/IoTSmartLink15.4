#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "iot154_packet.h"
#include "iot154_radio.h"

static const char *TAG = "central_154";
static const EventBits_t RX_DONE_BIT = BIT0;
static const EventBits_t TX_DONE_BIT = BIT1;
static const EventBits_t TX_FAILED_BIT = BIT2;
static const EventBits_t TEST_TOGGLE_BIT = BIT3;

#ifndef HOST_UART_TX_GPIO
#define HOST_UART_TX_GPIO 16
#endif

#ifndef HOST_UART_RX_GPIO
#define HOST_UART_RX_GPIO 17
#endif

#define HOST_UART_NUM UART_NUM_1
#define HOST_UART_BAUD_RATE 115200
#define HOST_UART_BUF_SIZE 4096
#define HOST_UART_LINE_MAX 4096

static EventGroupHandle_t s_events;
static SemaphoreHandle_t s_host_uart_lock;
static uint8_t s_rx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint8_t s_rx_len;
static esp_ieee802154_frame_info_t s_rx_info;
static uint8_t s_tx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint8_t s_mac_seq;
static uint16_t s_radio_tx_seq;
static esp_ieee802154_tx_error_t s_radio_tx_error;
static uint8_t s_central_ext_addr[IOT154_EXT_ADDR_LEN];
static const char *s_tx_type = "ACK";
static bool s_radio_tx_busy;
static uint8_t s_last_device_ext_addr[IOT154_EXT_ADDR_LEN];
static bool s_has_last_device;

typedef struct {
    uint32_t device_id;
    uint16_t last_seq;
    uint8_t ext_addr[IOT154_EXT_ADDR_LEN];
    bool valid;
} device_seq_state_t;

static device_seq_state_t s_devices[8];

static const char *type_from_event(uint8_t event_type);

/// @brief Initialize NVS before RF calibration data is loaded by the PHY.
static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
        return;
    }
    ESP_ERROR_CHECK(err);
}

/// @brief Configure the host side UART as JSON-lines transport.
static void init_host_uart(void)
{
    const uart_config_t config = {
        .baud_rate = HOST_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(HOST_UART_NUM, HOST_UART_BUF_SIZE, HOST_UART_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(HOST_UART_NUM, &config));
    ESP_ERROR_CHECK(uart_set_pin(HOST_UART_NUM, HOST_UART_TX_GPIO, HOST_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    s_host_uart_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(s_host_uart_lock != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG,
             "host UART%d baud=%d tx=GPIO%d rx=GPIO%d",
             HOST_UART_NUM,
             HOST_UART_BAUD_RATE,
             HOST_UART_TX_GPIO,
             HOST_UART_RX_GPIO);
}

/// @brief Copy a received frame out of the driver buffer and release it.
static void IRAM_ATTR on_rx_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info)
{
    BaseType_t task_woken = pdFALSE;
    uint8_t len = frame[0];
    if (len <= IOT154_MAX_FRAME_LEN) {
        memcpy(s_rx_frame, frame, len + 1);
        s_rx_len = len;
        s_rx_info = *frame_info;
        xEventGroupSetBitsFromISR(s_events, RX_DONE_BIT, &task_woken);
    }
    esp_ieee802154_receive_handle_done(frame);
    portYIELD_FROM_ISR(task_woken);
}

static void IRAM_ATTR on_tx_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_info)
{
    BaseType_t task_woken = pdFALSE;
    xEventGroupSetBitsFromISR(s_events, TX_DONE_BIT, &task_woken);
    if (ack != NULL) {
        esp_ieee802154_receive_handle_done(ack);
    }
    portYIELD_FROM_ISR(task_woken);
}

static void IRAM_ATTR on_tx_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error)
{
    BaseType_t task_woken = pdFALSE;
    s_radio_tx_error = error;
    xEventGroupSetBitsFromISR(s_events, TX_FAILED_BIT, &task_woken);
    portYIELD_FROM_ISR(task_woken);
}

/// @brief Format an extended address for logs.
static void format_ext_addr(const uint8_t *addr, char *out, size_t out_len)
{
    snprintf(out,
             out_len,
             "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
}

/// @brief Format the host-visible device identifier used by JSON messages.
static void format_device_id(const uint8_t *ext_addr, char *out, size_t out_len)
{
    snprintf(out,
             out_len,
             "issp154-%02X%02X%02X%02X%02X%02X%02X%02X",
             ext_addr[0], ext_addr[1], ext_addr[2], ext_addr[3],
             ext_addr[4], ext_addr[5], ext_addr[6], ext_addr[7]);
}

static void format_capability_name(const char *device_id, uint8_t event_type, char *out, size_t out_len)
{
    snprintf(out, out_len, "%s_ep_1_%s", device_id, type_from_event(event_type));
}

static int hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

/// @brief Parse host-visible IDs generated by this central.
static bool parse_host_ext_addr(const char *text, uint8_t *ext_addr)
{
    if (text == NULL || ext_addr == NULL || strncmp(text, "issp154-", 7) != 0) {
        return false;
    }

    const char *cursor = text + 7;
    if (strlen(cursor) != IOT154_EXT_ADDR_LEN * 2) {
        return false;
    }

    for (size_t i = 0; i < IOT154_EXT_ADDR_LEN; ++i) {
        int high = hex_nibble(cursor[i * 2]);
        int low = hex_nibble(cursor[i * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        ext_addr[i] = (uint8_t)((high << 4) | low);
    }

    return cursor[IOT154_EXT_ADDR_LEN * 2] == '\0';
}

/// @brief Parse legacy hex/numeric protocol device IDs from the host.
static bool parse_protocol_device_id(const char *text, uint32_t *device_id)
{
    if (text == NULL || device_id == NULL || strncmp(text, "issp154-", 7) == 0) {
        return false;
    }

    const char *cursor = text;
    int base = 10;
    if (strncmp(cursor, "0x", 2) == 0 || strncmp(cursor, "0X", 2) == 0) {
        base = 16;
    }

    char *end = NULL;
    unsigned long long parsed = strtoull(cursor, &end, base);
    if (end == cursor || *end != '\0') {
        return false;
    }

    *device_id = (uint32_t)parsed;
    return true;
}

static const char *type_from_event(uint8_t event_type)
{
    switch (event_type) {
    case IOT154_EVENT_DOOR:
        return "Door Sensor";
    case IOT154_EVENT_POWER:
        return "Switch Plug";
    case IOT154_EVENT_BATTERY_LEVEL_PERCENT:
        return "Battery Level (%)";
    default:
        return "Device";
    }
}

static const char *value_from_event(uint8_t event_type, uint8_t value, char *fallback, size_t fallback_len)
{
    if (event_type == IOT154_EVENT_DOOR) {
        return value == 1 ? "open" : "closed";
    }
    if (event_type == IOT154_EVENT_POWER) {
        if (value == IOT154_VALUE_TOGGLE) {
            return "toggle";
        }
        return value != 0 ? "on" : "off";
    }

    snprintf(fallback, fallback_len, "%u", value);
    return fallback;
}

static bool command_to_event(const char *capability, const char *value, uint8_t *event_type, uint8_t *event_value)
{
    if (capability == NULL || value == NULL || event_type == NULL || event_value == NULL) {
        return false;
    }

    if (strcmp(capability, "power") == 0 || strstr(capability, "Switch Plug") != NULL) {
        *event_type = IOT154_EVENT_POWER;
        if (strcmp(value, "on") == 0 || strcmp(value, "1") == 0 || strcmp(value, "true") == 0) {
            *event_value = IOT154_VALUE_ON;
            return true;
        }
        if (strcmp(value, "off") == 0 || strcmp(value, "0") == 0 || strcmp(value, "false") == 0) {
            *event_value = IOT154_VALUE_OFF;
            return true;
        }
        if (strcmp(value, "toggle") == 0) {
            *event_value = IOT154_VALUE_TOGGLE;
            return true;
        }
    }

    return false;
}

static const char *skip_json_ws(const char *cursor)
{
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        cursor++;
    }
    return cursor;
}

static bool parse_json_string(const char **cursor, char *out, size_t out_len)
{
    if (**cursor != '"' || out_len == 0) {
        return false;
    }

    (*cursor)++;
    size_t written = 0;
    while (**cursor != '\0') {
        char ch = *(*cursor)++;
        if (ch == '"') {
            out[written] = '\0';
            return true;
        }

        if (ch == '\\') {
            ch = *(*cursor)++;
            switch (ch) {
            case '"':
            case '\\':
            case '/':
                break;
            case 'b':
                ch = '\b';
                break;
            case 'f':
                ch = '\f';
                break;
            case 'n':
                ch = '\n';
                break;
            case 'r':
                ch = '\r';
                break;
            case 't':
                ch = '\t';
                break;
            default:
                ch = '?';
                break;
            }
        }

        if (written + 1 < out_len) {
            out[written++] = ch;
        }
    }

    out[0] = '\0';
    return false;
}

static bool skip_json_value(const char **cursor)
{
    char ignored[2] = {0};
    *cursor = skip_json_ws(*cursor);
    if (**cursor == '"') {
        return parse_json_string(cursor, ignored, sizeof(ignored));
    }

    while (**cursor != '\0' && **cursor != ',' && **cursor != '}') {
        (*cursor)++;
    }
    return true;
}

static bool json_get_string(const char *line, const char *field_name, char *out, size_t out_len)
{
    const char *cursor = skip_json_ws(line);
    if (*cursor != '{') {
        return false;
    }
    cursor++;

    while (*cursor != '\0') {
        char key[64] = {0};
        cursor = skip_json_ws(cursor);
        if (*cursor == '}') {
            break;
        }
        if (!parse_json_string(&cursor, key, sizeof(key))) {
            return false;
        }

        cursor = skip_json_ws(cursor);
        if (*cursor != ':') {
            return false;
        }
        cursor++;
        cursor = skip_json_ws(cursor);

        if (strcmp(key, field_name) == 0) {
            return parse_json_string(&cursor, out, out_len);
        }
        if (!skip_json_value(&cursor)) {
            return false;
        }

        cursor = skip_json_ws(cursor);
        if (*cursor == ',') {
            cursor++;
        }
    }

    return false;
}

static size_t json_escape_string(const char *in, char *out, size_t out_len)
{
    size_t written = 0;
    if (out_len == 0) {
        return 0;
    }

    for (; in != NULL && *in != '\0'; ++in) {
        const char *escaped = NULL;
        char control[7] = {0};
        switch (*in) {
        case '"':
            escaped = "\\\"";
            break;
        case '\\':
            escaped = "\\\\";
            break;
        case '\b':
            escaped = "\\b";
            break;
        case '\f':
            escaped = "\\f";
            break;
        case '\n':
            escaped = "\\n";
            break;
        case '\r':
            escaped = "\\r";
            break;
        case '\t':
            escaped = "\\t";
            break;
        default:
            if ((unsigned char)*in < 0x20) {
                snprintf(control, sizeof(control), "\\u%04x", (unsigned char)*in);
                escaped = control;
            }
            break;
        }

        if (escaped != NULL) {
            while (*escaped != '\0' && written + 1 < out_len) {
                out[written++] = *escaped++;
            }
        } else if (written + 1 < out_len) {
            out[written++] = *in;
        }
    }

    out[written] = '\0';
    return written;
}

static void host_send_line(const char *line)
{
    const size_t len = strlen(line);
    if (len >= HOST_UART_LINE_MAX - 1) {
        ESP_LOGW(TAG, "host JSON too large: %u bytes", (unsigned)len);
        return;
    }

    if (s_host_uart_lock != NULL) {
        xSemaphoreTake(s_host_uart_lock, portMAX_DELAY);
    }
    uart_write_bytes(HOST_UART_NUM, line, len);
    uart_write_bytes(HOST_UART_NUM, "\n", 1);
    if (s_host_uart_lock != NULL) {
        xSemaphoreGive(s_host_uart_lock);
    }
}

static void host_send_gateway(const char *direction)
{
    char line[192] = {0};
    snprintf(line,
             sizeof(line),
             "{\"device_id\":\"iotsmartlink_gateway\",\"capability_name\":\"iotsmartlink_gateway\",\"type\":\"Gateway\",\"direction\":\"%s\"}",
             direction);
    host_send_line(line);
}

static void host_send_ack(const char *device_id, const char *capability_name, const char *value)
{
    char escaped_device[96] = {0};
    char escaped_capability[512] = {0};
    char escaped_value[256] = {0};
    char line[HOST_UART_LINE_MAX] = {0};

    json_escape_string(device_id, escaped_device, sizeof(escaped_device));
    json_escape_string(capability_name, escaped_capability, sizeof(escaped_capability));
    json_escape_string(value, escaped_value, sizeof(escaped_value));
    snprintf(line,
             sizeof(line),
             "{\"device_id\":\"%s\",\"capability_name\":\"%s\",\"value\":\"%s\",\"direction\":\"ack\"}",
             escaped_device,
             escaped_capability,
             escaped_value);
    host_send_line(line);
}

static void host_send_event(const uint8_t *src_ext_addr, uint8_t event_type, uint8_t value)
{
    char device_text[24] = {0};
    char capability_text[96] = {0};
    char value_text[12] = {0};
    char line[320] = {0};
    const char *event_value = value_from_event(event_type, value, value_text, sizeof(value_text));
    format_device_id(src_ext_addr, device_text, sizeof(device_text));
    format_capability_name(device_text, event_type, capability_text, sizeof(capability_text));

    snprintf(line,
             sizeof(line),
             "{\"device_id\":\"%s\",\"capability_name\":\"%s\",\"value\":\"%s\",\"type\":\"%s\",\"direction\":\"evt\"}",
             device_text,
             capability_text,
             event_value,
             type_from_event(event_type));
    host_send_line(line);
}

/// @brief Send a protocol ACK packet carrying the received sequence number.
static void send_radio_ack(uint32_t device_id, uint16_t seq, const uint8_t *dst_ext_addr)
{
    iot154_packet_t ack = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_ACK,
        .device_id = device_id,
        .seq = seq,
        .event_type = 0,
        .value = IOT154_ACK_STATUS_OK,
    };
    iot154_packet_finalize(&ack);
    iot154_build_ext_frame(s_tx_frame, s_central_ext_addr, dst_ext_addr, s_mac_seq++, &ack);

    s_radio_tx_seq = seq;
    s_tx_type = "ACK";
    esp_ieee802154_sleep();
    esp_err_t err = esp_ieee802154_transmit(s_tx_frame, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ACK TX failed seq=%u start_err=%s", seq, esp_err_to_name(err));
        s_radio_tx_busy = false;
        iot154_radio_start_rx();
    } else {
        s_radio_tx_busy = true;
    }
}

/// @brief Reply to a sensor discovery request; the source MAC in this frame is the central identity.
static void send_discovery_response(uint32_t device_id, uint16_t seq, const uint8_t *dst_ext_addr)
{
    iot154_packet_t response = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_DISCOVERY_RESP,
        .device_id = device_id,
        .seq = seq,
        .event_type = 0,
        .value = IOT154_ACK_STATUS_OK,
    };
    iot154_packet_finalize(&response);
    iot154_build_ext_frame(s_tx_frame, s_central_ext_addr, dst_ext_addr, s_mac_seq++, &response);

    s_radio_tx_seq = seq;
    s_tx_type = "DISCOVERY_RESP";
    esp_ieee802154_sleep();
    esp_err_t err = esp_ieee802154_transmit(s_tx_frame, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DISCOVERY_RESP TX failed seq=%u start_err=%s", seq, esp_err_to_name(err));
        s_radio_tx_busy = false;
        iot154_radio_start_rx();
    } else {
        s_radio_tx_busy = true;
    }
}

/// @brief Return true when this source address + seq was already processed, remembering the logical protocol ID.
static bool is_duplicate(uint32_t device_id, uint16_t seq, const uint8_t *src_ext_addr)
{
    device_seq_state_t *free_slot = NULL;

    for (size_t i = 0; i < sizeof(s_devices) / sizeof(s_devices[0]); ++i) {
        if (s_devices[i].valid && iot154_ext_addr_equal(s_devices[i].ext_addr, src_ext_addr)) {
            s_devices[i].device_id = device_id;
            memcpy(s_last_device_ext_addr, src_ext_addr, IOT154_EXT_ADDR_LEN);
            s_has_last_device = true;
            if (s_devices[i].last_seq == seq) {
                return true;
            }
            s_devices[i].last_seq = seq;
            return false;
        }
        if (!s_devices[i].valid && free_slot == NULL) {
            free_slot = &s_devices[i];
        }
    }

    if (free_slot == NULL) {
        free_slot = &s_devices[0];
    }

    free_slot->device_id = device_id;
    free_slot->last_seq = seq;
    memcpy(free_slot->ext_addr, src_ext_addr, IOT154_EXT_ADDR_LEN);
    free_slot->valid = true;
    memcpy(s_last_device_ext_addr, src_ext_addr, IOT154_EXT_ADDR_LEN);
    s_has_last_device = true;
    return false;
}

static const device_seq_state_t *find_device_by_ext_addr(const uint8_t *ext_addr)
{
    for (size_t i = 0; i < sizeof(s_devices) / sizeof(s_devices[0]); ++i) {
        if (s_devices[i].valid && iot154_ext_addr_equal(s_devices[i].ext_addr, ext_addr)) {
            return &s_devices[i];
        }
    }
    return NULL;
}

static const device_seq_state_t *find_device_by_protocol_id(uint32_t device_id)
{
    for (size_t i = 0; i < sizeof(s_devices) / sizeof(s_devices[0]); ++i) {
        if (s_devices[i].valid && s_devices[i].device_id == device_id) {
            return &s_devices[i];
        }
    }
    return NULL;
}

static bool find_last_device(uint32_t *device_id, const uint8_t **ext_addr)
{
    const device_seq_state_t *device = s_has_last_device ? find_device_by_ext_addr(s_last_device_ext_addr) : NULL;
    if (device == NULL) {
        return false;
    }

    *device_id = device->device_id;
    *ext_addr = device->ext_addr;
    return true;
}

static bool send_radio_command(uint32_t device_id, const uint8_t *dst_ext_addr, uint8_t event_type, uint8_t value)
{
    if (s_radio_tx_busy) {
        ESP_LOGW(TAG, "command deferred/drop: radio TX busy dev=0x%08" PRIx32, device_id);
        return false;
    }

    if (dst_ext_addr == NULL) {
        ESP_LOGW(TAG, "command target not known dev=0x%08" PRIx32, device_id);
        return false;
    }

    iot154_packet_t command = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_DATA,
        .device_id = device_id,
        .seq = s_radio_tx_seq + 1,
        .event_type = event_type,
        .value = value,
    };
    iot154_packet_finalize(&command);
    iot154_build_ext_frame(s_tx_frame, s_central_ext_addr, dst_ext_addr, s_mac_seq++, &command);

    s_radio_tx_seq = command.seq;
    s_tx_type = "CMD";
    esp_ieee802154_sleep();
    esp_err_t err = esp_ieee802154_transmit(s_tx_frame, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CMD TX failed dev=0x%08" PRIx32 " seq=%u start_err=%s", device_id, command.seq, esp_err_to_name(err));
        s_radio_tx_busy = false;
        iot154_radio_start_rx();
        return false;
    }
    s_radio_tx_busy = true;
    return true;
}

static bool send_radio_command_to_host_device(const char *host_device_id, uint8_t event_type, uint8_t value)
{
    uint8_t ext_addr[IOT154_EXT_ADDR_LEN] = {0};
    uint32_t device_id = 0;

    if (parse_host_ext_addr(host_device_id, ext_addr)) {
        const device_seq_state_t *device = find_device_by_ext_addr(ext_addr);
        if (device == NULL) {
            ESP_LOGW(TAG, "command target not known dev=%s", host_device_id);
            return false;
        }
        return send_radio_command(device->device_id, ext_addr, event_type, value);
    }

    if (parse_protocol_device_id(host_device_id, &device_id)) {
        const device_seq_state_t *device = find_device_by_protocol_id(device_id);
        if (device == NULL) {
            ESP_LOGW(TAG, "command target not known dev=0x%08" PRIx32, device_id);
            return false;
        }
        return send_radio_command(device_id, device->ext_addr, event_type, value);
    }

    return false;
}

static void test_toggle_task(void *arg)
{
    (void)arg;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        xEventGroupSetBits(s_events, TEST_TOGGLE_BIT);
    }
}

static void handle_host_line(char *line)
{
    char direction[16] = {0};
    char device_id_text[96] = {0};
    char capability_name[256] = {0};
    char value[128] = {0};

    if (skip_json_ws(line)[0] != '{') {
        ESP_LOGW(TAG, "ignored host line: invalid JSON object");
        return;
    }

    if (!json_get_string(line, "direction", direction, sizeof(direction)) || strcmp(direction, "cmd") != 0) {
        return;
    }

    if (!json_get_string(line, "device_id", device_id_text, sizeof(device_id_text)) ||
        !json_get_string(line, "capability_name", capability_name, sizeof(capability_name)) ||
        !json_get_string(line, "value", value, sizeof(value))) {
        ESP_LOGW(TAG, "ignored host cmd: missing string field");
        return;
    }

    uint8_t event_type = 0;
    uint8_t event_value = 0;
    if (!command_to_event(capability_name, value, &event_type, &event_value) ||
        !send_radio_command_to_host_device(device_id_text, event_type, event_value)) {
        ESP_LOGW(TAG,
                 "host cmd accepted but not translatable dev=%s capability=%s value=%s",
                 device_id_text,
                 capability_name,
                 value);
    }

    host_send_ack(device_id_text, capability_name, value);
}

static void poll_host_uart(void)
{
    static char line[HOST_UART_LINE_MAX];
    static size_t line_len;
    uint8_t bytes[128];

    int read = uart_read_bytes(HOST_UART_NUM, bytes, sizeof(bytes), 0);
    for (int i = 0; i < read; ++i) {
        const char ch = (char)bytes[i];
        if (ch == '\n') {
            if (line_len > 0 && line[line_len - 1] == '\r') {
                line_len--;
            }
            line[line_len] = '\0';
            if (line_len > 0) {
                handle_host_line(line);
            }
            line_len = 0;
            continue;
        }

        if (line_len < sizeof(line) - 1) {
            line[line_len++] = ch;
        } else {
            ESP_LOGW(TAG, "discarding oversized host line");
            line_len = 0;
        }
    }
}

void app_main(void)
{
    char central_mac_text[3 * IOT154_EXT_ADDR_LEN] = {0};

    init_nvs();
    init_host_uart();
    ESP_ERROR_CHECK(esp_read_mac(s_central_ext_addr, ESP_MAC_IEEE802154));
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(iot154_radio_init(IOT154_CENTRAL_ADDR, true, on_rx_done, on_tx_done, on_tx_failed));
    ESP_ERROR_CHECK(esp_ieee802154_set_extended_address(s_central_ext_addr));
    ESP_ERROR_CHECK(iot154_radio_start_rx());
    xTaskCreate(test_toggle_task, "test_toggle", 3072, NULL, 4, NULL);

    format_ext_addr(s_central_ext_addr, central_mac_text, sizeof(central_mac_text));
    ESP_LOGI(TAG,
             "central RX channel=%d pan=0x%04x short=0x%04x ext=%s",
             IOT154_CHANNEL,
             IOT154_PAN_ID,
             IOT154_CENTRAL_ADDR,
             central_mac_text);

    host_send_gateway("hello");

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(s_events,
                                               RX_DONE_BIT | TX_DONE_BIT | TX_FAILED_BIT | TEST_TOGGLE_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(50));

        if ((bits & RX_DONE_BIT) != 0) {
            uint8_t frame[IOT154_MAX_FRAME_LEN + 1];
            memcpy(frame, s_rx_frame, s_rx_len + 1);

            iot154_frame_info_t mac = {0};
            iot154_packet_t packet = {0};
            if (!iot154_parse_frame_info(frame, &mac, &packet)) {
                ESP_LOGW(TAG, "ignored frame: invalid packet");
                continue;
            }

            if (packet.msg_type == IOT154_MSG_DISCOVERY_REQ &&
                mac.dst_mode == IOT154_ADDR_MODE_SHORT &&
                mac.dst_broadcast &&
                mac.src_mode == IOT154_ADDR_MODE_EXT) {
                char sensor_mac_text[3 * IOT154_EXT_ADDR_LEN] = {0};
                format_ext_addr(mac.src_ext, sensor_mac_text, sizeof(sensor_mac_text));
                ESP_LOGI(TAG, "DISCOVERY_REQ dev=0x%08" PRIx32 " seq=%u sensor=%s", packet.device_id, packet.seq, sensor_mac_text);
                (void)is_duplicate(packet.device_id, packet.seq, mac.src_ext);
                send_discovery_response(packet.device_id, packet.seq, mac.src_ext);
            } else if (packet.msg_type == IOT154_MSG_DATA &&
                       mac.dst_mode == IOT154_ADDR_MODE_EXT &&
                       iot154_ext_addr_equal(mac.dst_ext, s_central_ext_addr) &&
                       mac.src_mode == IOT154_ADDR_MODE_EXT) {
                bool duplicate = is_duplicate(packet.device_id, packet.seq, mac.src_ext);
                if (duplicate) {
                    ESP_LOGI(TAG, "DATA duplicate dev=0x%08" PRIx32 " seq=%u", packet.device_id, packet.seq);
                } else {
                    ESP_LOGI(TAG,
                             "DATA new dev=0x%08" PRIx32 " seq=%u event=%u value=%u",
                             packet.device_id, packet.seq, packet.event_type, packet.value);
                    host_send_event(mac.src_ext, packet.event_type, packet.value);
                }
                send_radio_ack(packet.device_id, packet.seq, mac.src_ext);
            } else {
                ESP_LOGW(TAG, "ignored frame: msg=%u not for this central", packet.msg_type);
            }
        }

        if ((bits & TX_DONE_BIT) != 0) {
            ESP_LOGI(TAG, "%s TX done seq=%u", s_tx_type, s_radio_tx_seq);
            s_radio_tx_busy = false;
            ESP_ERROR_CHECK(iot154_radio_start_rx());
        }

        if ((bits & TX_FAILED_BIT) != 0) {
            ESP_LOGW(TAG, "%s TX failed seq=%u error=%d", s_tx_type, s_radio_tx_seq, s_radio_tx_error);
            s_radio_tx_busy = false;
            ESP_ERROR_CHECK(iot154_radio_start_rx());
        }

        if ((bits & TEST_TOGGLE_BIT) != 0) {
            uint32_t device_id = 0;
            const uint8_t *ext_addr = NULL;
            if (find_last_device(&device_id, &ext_addr)) {
                ESP_LOGI(TAG, "test toggle dev=0x%08" PRIx32, device_id);
                (void)send_radio_command(device_id, ext_addr, IOT154_EVENT_POWER, IOT154_VALUE_TOGGLE);
            } else {
                ESP_LOGW(TAG, "test toggle skipped: no known client");
            }
        }

        poll_host_uart();
    }
}
