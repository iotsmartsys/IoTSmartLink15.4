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
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "device_registry.h"
#include "iot154_packet.h"
#include "iot154_radio.h"

static const char *TAG = "central_154";
static const EventBits_t RX_DONE_BIT = BIT0;
static const EventBits_t TX_DONE_BIT = BIT1;
static const EventBits_t TX_FAILED_BIT = BIT2;

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
#define REPORT_ACK_TURNAROUND_DELAY_US 20000
#define DISCOVERY_RESPONSE_TURNAROUND_DELAY_US 20000
#define COMMAND_ACK_TIMEOUT_MS 300
#define COMMAND_RETRY_DELAY_MS 50
#define COMMAND_MAX_ATTEMPTS 3
#define COMMISSIONING_JOIN_WINDOW_SECONDS 60

#define ISSP154_HOST_DEVICE_ID_PREFIX "issp154-"
#define ISSP154_HOST_DEVICE_ID_PREFIX_LEN (sizeof(ISSP154_HOST_DEVICE_ID_PREFIX) - 1)
#define ISSP154_HOST_DEVICE_ID_HEX_LEN (IOT154_EXT_ADDR_LEN * 2)
#define ISSP154_HOST_DEVICE_ID_LEN (ISSP154_HOST_DEVICE_ID_PREFIX_LEN + ISSP154_HOST_DEVICE_ID_HEX_LEN)
#define ISSP154_HOST_DEVICE_ID_BUFFER_SIZE (ISSP154_HOST_DEVICE_ID_LEN + 1)
#define ISSP154_CAPABILITY_ENDPOINT_PREFIX "_ep_"
#define ISSP154_CAPABILITY_ENDPOINT_SEPARATOR "_"
#define ISSP154_ENDPOINT_ID_MAX_TEXT_LEN 3
#define ISSP154_CAPABILITY_ENDPOINT_TEXT_LEN                                               \
    ((sizeof(ISSP154_CAPABILITY_ENDPOINT_PREFIX) - 1) + ISSP154_ENDPOINT_ID_MAX_TEXT_LEN + \
     (sizeof(ISSP154_CAPABILITY_ENDPOINT_SEPARATOR) - 1))
#define ISSP154_CAPABILITY_TYPE_MAX_LEN 32
#define ISSP154_CAPABILITY_NAME_LEN \
    (ISSP154_HOST_DEVICE_ID_LEN + ISSP154_CAPABILITY_ENDPOINT_TEXT_LEN + ISSP154_CAPABILITY_TYPE_MAX_LEN)
#define ISSP154_CAPABILITY_NAME_BUFFER_SIZE (ISSP154_CAPABILITY_NAME_LEN + 1)
#define ISSP154_HOST_EVENT_JSON_BUFFER_SIZE 384

static EventGroupHandle_t s_events;
static SemaphoreHandle_t s_host_uart_lock;
static char s_host_result_line[HOST_UART_LINE_MAX];
static char s_host_escaped_device[96];
static char s_host_escaped_capability[512];
static char s_host_escaped_value[256];
static char s_host_escaped_reason[256];
static uint8_t s_rx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint8_t s_rx_len;
static esp_ieee802154_frame_info_t s_rx_info;
static uint8_t s_tx_frame[IOT154_MAX_FRAME_LEN + 1];
static uint8_t s_mac_seq;
static uint16_t s_radio_tx_seq;
static uint16_t s_next_command_seq;
static esp_ieee802154_tx_error_t s_radio_tx_error;
static uint8_t s_central_ext_addr[IOT154_EXT_ADDR_LEN];
static const char *s_tx_type = "ACK";
static bool s_radio_tx_busy;
static volatile int64_t s_radio_tx_callback_us;
static int64_t s_radio_tx_started_us;
static volatile uint32_t s_rx_total_count;
static volatile uint32_t s_rx_overwrite_count;
static volatile bool s_tx_is_report_ack;
static volatile bool s_tx_is_command;
static uint8_t s_report_ack_mac_seq;
static uint16_t s_report_ack_issp_seq;
static uint8_t s_report_ack_dst[IOT154_EXT_ADDR_LEN];
static bool s_join_window_open;
static int64_t s_join_window_deadline_us;

typedef struct
{
    bool active;
    bool awaiting_ack;
    uint8_t attempts;
    TickType_t deadline;
    uint32_t device_id;
    uint16_t sequence;
    uint8_t endpoint_id;
    uint8_t event_type;
    uint8_t value;
    uint8_t destination[IOT154_EXT_ADDR_LEN];
    char host_device_id[96];
    char capability_name[ISSP154_CAPABILITY_NAME_BUFFER_SIZE];
    char host_value[128];
} pending_command_t;

static pending_command_t s_pending_command;

/// @brief Volatile DATA sequence dedup cache, keyed by the device's registry slot index
/// (COORD-REG-009: last_seq never belongs to the persisted blob). Zero-initialized at boot, so the
/// first DATA frame observed for each slot in this run is never treated as a duplicate.
static uint16_t s_last_seq[DEVICE_REGISTRY_MAX_ENTRIES];
static bool s_last_seq_valid[DEVICE_REGISTRY_MAX_ENTRIES];

static bool is_known_data_duplicate(size_t registry_index, uint16_t seq)
{
    if (!s_last_seq_valid[registry_index])
    {
        s_last_seq_valid[registry_index] = true;
        s_last_seq[registry_index] = seq;
        return false;
    }
    if (s_last_seq[registry_index] == seq)
    {
        return true;
    }
    s_last_seq[registry_index] = seq;
    return false;
}

static const char *type_from_event(uint8_t event_type);

static const char *radio_tx_error_name(esp_ieee802154_tx_error_t error)
{
    switch (error)
    {
    case ESP_IEEE802154_TX_ERR_NONE:
        return "none";
    case ESP_IEEE802154_TX_ERR_CCA_BUSY:
        return "cca_busy";
    case ESP_IEEE802154_TX_ERR_ABORT:
        return "abort";
    case ESP_IEEE802154_TX_ERR_NO_ACK:
        return "no_mac_ack";
    case ESP_IEEE802154_TX_ERR_INVALID_ACK:
        return "invalid_mac_ack";
    case ESP_IEEE802154_TX_ERR_COEXIST:
        return "coexist";
    case ESP_IEEE802154_TX_ERR_SECURITY:
        return "security";
    default:
        return "unknown";
    }
}

/// @brief Initialize NVS before RF calibration data is loaded by the PHY.
static esp_err_t init_nvs(void)
{
    const esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGE("DEVICE_REGISTRY", "load result=unavailable reason=%s", esp_err_to_name(err));
        return err;
    }
    return err;
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
    const uint8_t len = frame != NULL ? frame[0] : 0;
    const bool pending_slot = s_events != NULL &&
                              (xEventGroupGetBitsFromISR(s_events) & RX_DONE_BIT) != 0;
    if (len <= IOT154_MAX_FRAME_LEN)
    {
        ++s_rx_total_count;
        memcpy(s_rx_frame, frame, len + 1);
        s_rx_len = len;
        s_rx_info = *frame_info;
        xEventGroupSetBitsFromISR(s_events, RX_DONE_BIT, &task_woken);
        if (pending_slot)
        {
            ++s_rx_overwrite_count;
            ESP_DRAM_LOGW(DRAM_STR("COORD_RX"), "pending frame overwritten");
        }
    }
    else
    {
        ESP_DRAM_LOGW(DRAM_STR("COORD_RX"), "invalid frame discarded");
    }
    if (frame != NULL)
    {
        esp_ieee802154_receive_handle_done(frame);
    }
    portYIELD_FROM_ISR(task_woken);
}

static void IRAM_ATTR on_tx_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_info)
{
    BaseType_t task_woken = pdFALSE;
    s_radio_tx_callback_us = esp_timer_get_time();
    xEventGroupSetBitsFromISR(s_events, TX_DONE_BIT, &task_woken);
    if (ack != NULL)
    {
        esp_ieee802154_receive_handle_done(ack);
    }
    portYIELD_FROM_ISR(task_woken);
}

static void IRAM_ATTR on_tx_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error)
{
    BaseType_t task_woken = pdFALSE;
    s_radio_tx_callback_us = esp_timer_get_time();
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

static bool diagnostic_extract_mac(const uint8_t *frame,
                                   iot154_frame_info_t *info,
                                   iot154_packet_t *packet,
                                   size_t *payload_length,
                                   const char **reason)
{
    *reason = "unknown";
    *payload_length = 0;
    memset(info, 0, sizeof(*info));
    memset(packet, 0, sizeof(*packet));
    if (frame == NULL || frame[0] < IOT154_MAC_HEADER_LEN + sizeof(*packet) + IOT154_FCS_LEN)
    {
        *reason = "frame_too_short";
        return false;
    }

    const size_t frame_length = (size_t)frame[0] + 1U;
    const uint16_t fcf = (uint16_t)frame[1] | ((uint16_t)frame[2] << 8);
    const uint8_t dst_mode = (uint8_t)((fcf >> 10) & 0x03);
    const uint8_t src_mode = (uint8_t)((fcf >> 14) & 0x03);
    const bool pan_compression = (fcf & (1U << 6)) != 0;
    size_t pos = 4;
    info->src_mode = src_mode;
    info->dst_mode = dst_mode;

    if (dst_mode != IOT154_ADDR_MODE_NONE)
    {
        if (pos + 2 > frame_length)
        {
            *reason = "destination_pan_truncated";
            return false;
        }
        const uint16_t dst_pan = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1] << 8);
        pos += 2;
        if (dst_pan != IOT154_PAN_ID && dst_pan != 0xffffU)
        {
            *reason = "destination_pan_mismatch";
            return false;
        }
        if (dst_mode == IOT154_ADDR_MODE_SHORT)
        {
            if (pos + 2 > frame_length)
            {
                *reason = "destination_short_truncated";
                return false;
            }
            info->dst_short = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1] << 8);
            info->dst_broadcast = info->dst_short == IOT154_BROADCAST_ADDR;
            pos += 2;
        }
        else if (dst_mode == IOT154_ADDR_MODE_EXT)
        {
            if (pos + IOT154_EXT_ADDR_LEN > frame_length)
            {
                *reason = "destination_extended_truncated";
                return false;
            }
            memcpy(info->dst_ext, &frame[pos], IOT154_EXT_ADDR_LEN);
            pos += IOT154_EXT_ADDR_LEN;
        }
        else
        {
            *reason = "destination_mode_invalid";
            return false;
        }
    }

    if (src_mode != IOT154_ADDR_MODE_NONE)
    {
        if (!pan_compression)
        {
            if (pos + 2 > frame_length)
            {
                *reason = "source_pan_truncated";
                return false;
            }
            const uint16_t src_pan = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1] << 8);
            pos += 2;
            if (src_pan != IOT154_PAN_ID)
            {
                *reason = "source_pan_mismatch";
                return false;
            }
        }
        if (src_mode == IOT154_ADDR_MODE_SHORT)
        {
            if (pos + 2 > frame_length)
            {
                *reason = "source_short_truncated";
                return false;
            }
            info->src_short = (uint16_t)frame[pos] | ((uint16_t)frame[pos + 1] << 8);
            pos += 2;
        }
        else if (src_mode == IOT154_ADDR_MODE_EXT)
        {
            if (pos + IOT154_EXT_ADDR_LEN > frame_length)
            {
                *reason = "source_extended_truncated";
                return false;
            }
            memcpy(info->src_ext, &frame[pos], IOT154_EXT_ADDR_LEN);
            pos += IOT154_EXT_ADDR_LEN;
        }
        else
        {
            *reason = "source_mode_invalid";
            return false;
        }
    }

    const size_t mac_header_length = pos - 1U;
    if (frame[0] < mac_header_length + sizeof(*packet) + IOT154_FCS_LEN)
    {
        *reason = "payload_truncated";
        return false;
    }
    *payload_length = frame[0] - mac_header_length - IOT154_FCS_LEN;
    memcpy(packet, &frame[pos], sizeof(*packet));
    *reason = "ok";
    return true;
}

/// @brief Format the host-visible device identifier used by JSON messages.
static void format_device_id(const uint8_t *ext_addr, char *out, size_t out_len)
{
    snprintf(out,
             out_len,
             ISSP154_HOST_DEVICE_ID_PREFIX "%02X%02X%02X%02X%02X%02X%02X%02X",
             ext_addr[0], ext_addr[1], ext_addr[2], ext_addr[3],
             ext_addr[4], ext_addr[5], ext_addr[6], ext_addr[7]);
}

static void format_capability_name(const char *device_id, uint8_t endpoint_id, uint8_t event_type, char *out, size_t out_len)
{
    snprintf(out,
             out_len,
             "%s" ISSP154_CAPABILITY_ENDPOINT_PREFIX "%u" ISSP154_CAPABILITY_ENDPOINT_SEPARATOR "%s",
             device_id,
             endpoint_id,
             type_from_event(event_type));
}

static int hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F')
    {
        return ch - 'A' + 10;
    }
    return -1;
}

/// @brief Parse host-visible IDs generated by this central.
static bool parse_host_ext_addr(const char *text, uint8_t *ext_addr)
{
    if (text == NULL ||
        ext_addr == NULL ||
        strncmp(text, ISSP154_HOST_DEVICE_ID_PREFIX, ISSP154_HOST_DEVICE_ID_PREFIX_LEN) != 0)
    {
        return false;
    }

    const char *cursor = text + ISSP154_HOST_DEVICE_ID_PREFIX_LEN;
    if (strlen(cursor) != ISSP154_HOST_DEVICE_ID_HEX_LEN)
    {
        return false;
    }

    for (size_t i = 0; i < IOT154_EXT_ADDR_LEN; ++i)
    {
        int high = hex_nibble(cursor[i * 2]);
        int low = hex_nibble(cursor[i * 2 + 1]);
        if (high < 0 || low < 0)
        {
            return false;
        }
        ext_addr[i] = (uint8_t)((high << 4) | low);
    }

    return cursor[IOT154_EXT_ADDR_LEN * 2] == '\0';
}

static const char *type_from_event(uint8_t event_type)
{
    switch (event_type)
    {
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
    if (event_type == IOT154_EVENT_DOOR)
    {
        return value == 1 ? "open" : "closed";
    }
    if (event_type == IOT154_EVENT_POWER)
    {
        if (value == IOT154_VALUE_TOGGLE)
        {
            return "toggle";
        }
        return value != 0 ? "on" : "off";
    }

    snprintf(fallback, fallback_len, "%u", value);
    return fallback;
}

static bool extract_host_device_id_and_endpoint_from_capability(const char *capability,
                                                                char *out,
                                                                size_t out_len,
                                                                uint8_t *endpoint_id)
{
    if (capability == NULL || out == NULL || out_len == 0 || endpoint_id == NULL)
    {
        return false;
    }

    const char *endpoint = strstr(capability, ISSP154_CAPABILITY_ENDPOINT_PREFIX);
    if (endpoint == NULL)
    {
        return false;
    }

    size_t len = (size_t)(endpoint - capability);
    if (len == 0 || len >= out_len)
    {
        return false;
    }

    memcpy(out, capability, len);
    out[len] = '\0';

    const char *endpoint_text = endpoint + sizeof(ISSP154_CAPABILITY_ENDPOINT_PREFIX) - 1;
    char *end = NULL;
    unsigned long parsed = strtoul(endpoint_text, &end, 10);
    if (end == endpoint_text ||
        *end != ISSP154_CAPABILITY_ENDPOINT_SEPARATOR[0] ||
        parsed == 0 ||
        parsed > UINT8_MAX)
    {
        out[0] = '\0';
        return false;
    }

    *endpoint_id = (uint8_t)parsed;
    return true;
}

static bool capability_is_switch_plug(const char *capability, const char *type)
{
    return (type != NULL && strcmp(type, "Switch Plug") == 0) ||
           (capability != NULL && strstr(capability, "Switch Plug") != NULL) ||
           (capability != NULL && strcmp(capability, "power") == 0);
}

static bool command_to_event(const char *capability,
                             const char *type,
                             const char *value,
                             uint8_t *event_type,
                             uint8_t *event_value)
{
    if (capability == NULL || value == NULL || event_type == NULL || event_value == NULL)
    {
        return false;
    }

    if (!capability_is_switch_plug(capability, type))
    {
        return false;
    }

    *event_type = IOT154_EVENT_POWER;
    if (strcmp(value, "on") == 0 || strcmp(value, "1") == 0 || strcmp(value, "true") == 0)
    {
        *event_value = IOT154_VALUE_ON;
        return true;
    }
    if (strcmp(value, "off") == 0 || strcmp(value, "0") == 0 || strcmp(value, "false") == 0)
    {
        *event_value = IOT154_VALUE_OFF;
        return true;
    }
    if (strcmp(value, "toggle") == 0)
    {
        *event_value = IOT154_VALUE_TOGGLE;
        return true;
    }

    return false;
}

static const char *skip_json_ws(const char *cursor)
{
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
    {
        cursor++;
    }
    return cursor;
}

static bool parse_json_string(const char **cursor, char *out, size_t out_len)
{
    if (**cursor != '"' || out_len == 0)
    {
        return false;
    }

    (*cursor)++;
    size_t written = 0;
    while (**cursor != '\0')
    {
        char ch = *(*cursor)++;
        if (ch == '"')
        {
            out[written] = '\0';
            return true;
        }

        if (ch == '\\')
        {
            ch = *(*cursor)++;
            switch (ch)
            {
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

        if (written + 1 < out_len)
        {
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
    if (**cursor == '"')
    {
        return parse_json_string(cursor, ignored, sizeof(ignored));
    }

    while (**cursor != '\0' && **cursor != ',' && **cursor != '}')
    {
        (*cursor)++;
    }
    return true;
}

static bool json_get_string(const char *line, const char *field_name, char *out, size_t out_len)
{
    const char *cursor = skip_json_ws(line);
    if (*cursor != '{')
    {
        return false;
    }
    cursor++;

    while (*cursor != '\0')
    {
        char key[64] = {0};
        cursor = skip_json_ws(cursor);
        if (*cursor == '}')
        {
            break;
        }
        if (!parse_json_string(&cursor, key, sizeof(key)))
        {
            return false;
        }

        cursor = skip_json_ws(cursor);
        if (*cursor != ':')
        {
            return false;
        }
        cursor++;
        cursor = skip_json_ws(cursor);

        if (strcmp(key, field_name) == 0)
        {
            return parse_json_string(&cursor, out, out_len);
        }
        if (!skip_json_value(&cursor))
        {
            return false;
        }

        cursor = skip_json_ws(cursor);
        if (*cursor == ',')
        {
            cursor++;
        }
    }

    return false;
}

static size_t json_escape_string(const char *in, char *out, size_t out_len)
{
    size_t written = 0;
    if (out_len == 0)
    {
        return 0;
    }

    for (; in != NULL && *in != '\0'; ++in)
    {
        const char *escaped = NULL;
        char control[7] = {0};
        switch (*in)
        {
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
            if ((unsigned char)*in < 0x20)
            {
                snprintf(control, sizeof(control), "\\u%04x", (unsigned char)*in);
                escaped = control;
            }
            break;
        }

        if (escaped != NULL)
        {
            while (*escaped != '\0' && written + 1 < out_len)
            {
                out[written++] = *escaped++;
            }
        }
        else if (written + 1 < out_len)
        {
            out[written++] = *in;
        }
    }

    out[written] = '\0';
    return written;
}

static void host_send_line(const char *line)
{
    const size_t len = strlen(line);
    if (len >= HOST_UART_LINE_MAX - 1)
    {
        ESP_LOGW(TAG, "host JSON too large: %u bytes", (unsigned)len);
        return;
    }

    if (s_host_uart_lock != NULL)
    {
        xSemaphoreTake(s_host_uart_lock, portMAX_DELAY);
    }
    uart_write_bytes(HOST_UART_NUM, line, len);
    uart_write_bytes(HOST_UART_NUM, "\n", 1);
    if (s_host_uart_lock != NULL)
    {
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
    json_escape_string(device_id, s_host_escaped_device, sizeof(s_host_escaped_device));
    json_escape_string(capability_name, s_host_escaped_capability, sizeof(s_host_escaped_capability));
    json_escape_string(value, s_host_escaped_value, sizeof(s_host_escaped_value));
    snprintf(s_host_result_line,
             sizeof(s_host_result_line),
             "{\"device_id\":\"%s\",\"capability_name\":\"%s\",\"value\":\"%s\",\"direction\":\"ack\"}",
             s_host_escaped_device,
             s_host_escaped_capability,
             s_host_escaped_value);
    host_send_line(s_host_result_line);
}

static void host_send_error(const char *device_id, const char *capability_name, const char *value, const char *reason)
{
    json_escape_string(device_id, s_host_escaped_device, sizeof(s_host_escaped_device));
    json_escape_string(capability_name, s_host_escaped_capability, sizeof(s_host_escaped_capability));
    json_escape_string(value, s_host_escaped_value, sizeof(s_host_escaped_value));
    json_escape_string(reason, s_host_escaped_reason, sizeof(s_host_escaped_reason));
    snprintf(s_host_result_line,
             sizeof(s_host_result_line),
             "{\"device_id\":\"%s\",\"capability_name\":\"%s\",\"requested_value\":\"%s\",\"direction\":\"err\",\"error\":\"%s\"}",
             s_host_escaped_device,
             s_host_escaped_capability,
             s_host_escaped_value,
             s_host_escaped_reason);
    host_send_line(s_host_result_line);
}

static void host_send_event(const uint8_t *src_ext_addr, uint8_t endpoint_id, uint8_t event_type, uint8_t value)
{
    char device_text[ISSP154_HOST_DEVICE_ID_BUFFER_SIZE] = {0};
    char capability_text[ISSP154_CAPABILITY_NAME_BUFFER_SIZE] = {0};
    char value_text[12] = {0};
    char line[ISSP154_HOST_EVENT_JSON_BUFFER_SIZE] = {0};
    const char *event_value = value_from_event(event_type, value, value_text, sizeof(value_text));
    format_device_id(src_ext_addr, device_text, sizeof(device_text));
    format_capability_name(device_text, endpoint_id, event_type, capability_text, sizeof(capability_text));

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
static void send_radio_ack(uint32_t device_id, uint16_t seq, uint8_t endpoint_id, const uint8_t *dst_ext_addr)
{
    iot154_packet_t ack = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_ACK,
        .device_id = device_id,
        .seq = seq,
        .endpoint_id = endpoint_id,
        .event_type = 0,
        .value = IOT154_ACK_STATUS_OK,
    };
    iot154_packet_finalize(&ack);
    const uint8_t ack_mac_sequence = s_mac_seq;
    const size_t frame_length =
        iot154_build_ext_frame(s_tx_frame, s_central_ext_addr, dst_ext_addr, s_mac_seq++, &ack);
    char destination_text[3 * IOT154_EXT_ADDR_LEN] = {0};
    format_ext_addr(dst_ext_addr, destination_text, sizeof(destination_text));

    if (frame_length != (size_t)s_tx_frame[0] + 1U)
    {
        ESP_LOGW(TAG, "ACK frame build failed seq=%u", seq);
    }

    s_radio_tx_seq = seq;
    s_tx_type = "ACK";
    s_tx_is_report_ack = true;
    s_tx_is_command = false;
    s_report_ack_issp_seq = seq;
    s_report_ack_mac_seq = ack_mac_sequence;
    memcpy(s_report_ack_dst, dst_ext_addr, sizeof(s_report_ack_dst));
    ESP_LOGI("COORD_RADIO_TRACE",
             "event=tx_prepare role=report_ack device=0x%08" PRIx32 " issp_seq=%u endpoint=%u mac_seq=%u turnaround_us=%u",
             device_id,
             (unsigned)seq,
             (unsigned)endpoint_id,
             (unsigned)ack_mac_sequence,
             (unsigned)REPORT_ACK_TURNAROUND_DELAY_US);
    esp_rom_delay_us(REPORT_ACK_TURNAROUND_DELAY_US);
    s_radio_tx_started_us = esp_timer_get_time();
    (void)esp_ieee802154_sleep();
    esp_err_t err = esp_ieee802154_transmit(s_tx_frame, false);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "ACK TX failed seq=%u start_err=%s", seq, esp_err_to_name(err));
        s_radio_tx_busy = false;
        const esp_err_t restore_result = iot154_radio_start_rx();
        if (restore_result != ESP_OK)
        {
            ESP_LOGW(TAG, "ACK RX restore failed: %s", esp_err_to_name(restore_result));
        }
        s_tx_is_report_ack = false;
    }
    else
    {
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
        .endpoint_id = 0,
        .event_type = 0,
        .value = IOT154_ACK_STATUS_OK,
    };
    iot154_packet_finalize(&response);
    iot154_build_ext_frame(s_tx_frame, s_central_ext_addr, dst_ext_addr, s_mac_seq++, &response);

    s_radio_tx_seq = seq;
    s_tx_type = "DISCOVERY_RESP";
    s_tx_is_report_ack = false;
    s_tx_is_command = false;
    ESP_LOGI("COMMISSIONING",
             "discovery_response turnaround_delay_us=%u seq=%u",
             (unsigned)DISCOVERY_RESPONSE_TURNAROUND_DELAY_US,
             seq);
    esp_rom_delay_us(DISCOVERY_RESPONSE_TURNAROUND_DELAY_US);
    s_radio_tx_started_us = esp_timer_get_time();
    esp_ieee802154_sleep();
    esp_err_t err = esp_ieee802154_transmit(s_tx_frame, false);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "DISCOVERY_RESP TX failed seq=%u start_err=%s", seq, esp_err_to_name(err));
        s_radio_tx_busy = false;
        iot154_radio_start_rx();
    }
    else
    {
        s_radio_tx_busy = true;
    }
}

static void update_join_window(void)
{
    if (!s_join_window_open || esp_timer_get_time() < s_join_window_deadline_us)
    {
        return;
    }

    const esp_err_t result = esp_ieee802154_set_promiscuous(false);
    if (result != ESP_OK)
    {
        ESP_LOGW("COMMISSIONING", "join_window close_failed result=%s",
                 esp_err_to_name(result));
        return;
    }
    s_join_window_open = false;
    ESP_LOGI("COMMISSIONING", "join_window closed");
}

static bool transmit_pending_command(void)
{
    if (!s_pending_command.active || s_radio_tx_busy)
    {
        return false;
    }

    iot154_packet_t command = {
        .version = IOT154_VERSION,
        .msg_type = IOT154_MSG_CMD,
        .device_id = s_pending_command.device_id,
        .seq = s_pending_command.sequence,
        .endpoint_id = s_pending_command.endpoint_id,
        .event_type = s_pending_command.event_type,
        .value = s_pending_command.value,
    };
    iot154_packet_finalize(&command);
    iot154_build_ext_frame(s_tx_frame,
                           s_central_ext_addr,
                           s_pending_command.destination,
                           s_mac_seq++,
                           &command);

    s_radio_tx_seq = command.seq;
    s_tx_type = "CMD";
    s_tx_is_report_ack = false;
    s_tx_is_command = true;
    ++s_pending_command.attempts;
    s_pending_command.awaiting_ack = false;
    ESP_LOGI("COORD_RADIO_TRACE",
             "event=tx_prepare role=command device=0x%08" PRIx32 " issp_seq=%u endpoint=%u mac_seq=%u attempt=%u cca=off",
             command.device_id,
             (unsigned)command.seq,
             (unsigned)command.endpoint_id,
             (unsigned)(s_mac_seq - 1U),
             (unsigned)s_pending_command.attempts);
    s_radio_tx_started_us = esp_timer_get_time();
    esp_ieee802154_sleep();
    esp_err_t err = esp_ieee802154_transmit(s_tx_frame, false);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG,
                 "CMD TX failed dev=0x%08" PRIx32 " seq=%u attempt=%u start_err=%s",
                 command.device_id,
                 command.seq,
                 (unsigned)s_pending_command.attempts,
                 esp_err_to_name(err));
        s_tx_is_command = false;
        s_radio_tx_busy = false;
        iot154_radio_start_rx();
        s_pending_command.deadline =
            xTaskGetTickCount() + pdMS_TO_TICKS(COMMAND_RETRY_DELAY_MS);
        return false;
    }
    s_radio_tx_busy = true;
    ESP_LOGI(TAG,
             "CMD TX started dev=0x%08" PRIx32 " seq=%u attempt=%u",
             command.device_id,
             command.seq,
             (unsigned)s_pending_command.attempts);
    return true;
}

/// @brief Outcome of a host-command intake attempt, precise enough to keep RegistryUnavailable
/// distinguishable from an unknown identity per section 5.1's precedence rule (COORD-REG-011).
typedef enum
{
    HOST_COMMAND_START_OK = 0,
    HOST_COMMAND_START_PENDING,
    HOST_COMMAND_START_INVALID_ADDR,
    HOST_COMMAND_START_REGISTRY_UNAVAILABLE,
    HOST_COMMAND_START_UNKNOWN_DEVICE,
} host_command_start_result_t;

static host_command_start_result_t start_host_command(const char *host_device_id,
                               const char *host_request_device_id,
                               const char *capability_name,
                               const char *host_value,
                               uint8_t endpoint_id,
                               uint8_t event_type,
                               uint8_t value)
{
    uint8_t ext_addr[IOT154_EXT_ADDR_LEN] = {0};
    if (s_pending_command.active)
    {
        ESP_LOGW(TAG, "command rejected: another command is pending seq=%u",
                 (unsigned)s_pending_command.sequence);
        return HOST_COMMAND_START_PENDING;
    }

    if (!parse_host_ext_addr(host_device_id, ext_addr))
    {
        return HOST_COMMAND_START_INVALID_ADDR;
    }

    /* Section 5.1 precedence: RegistryUnavailable must be resolved before origin/identity, so a
       lookup failure caused by an unavailable registry is never reported as an unknown device. */
    if (device_registry_state() != DEVICE_REGISTRY_STATE_READY)
    {
        ESP_LOGW("DEVICE_REGISTRY", "command rejected reason=registry_unavailable dev=%s", host_device_id);
        return HOST_COMMAND_START_REGISTRY_UNAVAILABLE;
    }

    uint32_t known_device_id = 0;
    if (!device_registry_find(ext_addr, &known_device_id, NULL))
    {
        ESP_LOGW(TAG, "command target not known dev=%s", host_device_id);
        return HOST_COMMAND_START_UNKNOWN_DEVICE;
    }

    ++s_next_command_seq;
    if (s_next_command_seq == 0)
    {
        ++s_next_command_seq;
    }

    memset(&s_pending_command, 0, sizeof(s_pending_command));
    s_pending_command.active = true;
    s_pending_command.device_id = known_device_id;
    s_pending_command.sequence = s_next_command_seq;
    s_pending_command.endpoint_id = endpoint_id;
    s_pending_command.event_type = event_type;
    s_pending_command.value = value;
    memcpy(s_pending_command.destination, ext_addr, sizeof(ext_addr));
    strlcpy(s_pending_command.host_device_id,
            host_request_device_id,
            sizeof(s_pending_command.host_device_id));
    strlcpy(s_pending_command.capability_name,
            capability_name,
            sizeof(s_pending_command.capability_name));
    strlcpy(s_pending_command.host_value,
            host_value,
            sizeof(s_pending_command.host_value));

    (void)transmit_pending_command();
    return HOST_COMMAND_START_OK;
}

static void complete_pending_command(bool accepted, const char *error)
{
    if (!s_pending_command.active)
    {
        return;
    }

    if (accepted)
    {
        host_send_ack(s_pending_command.host_device_id,
                      s_pending_command.capability_name,
                      s_pending_command.host_value);
    }
    else
    {
        host_send_error(s_pending_command.host_device_id,
                        s_pending_command.capability_name,
                        s_pending_command.host_value,
                        error);
    }
    memset(&s_pending_command, 0, sizeof(s_pending_command));
}

static void process_pending_command(void)
{
    if (!s_pending_command.active || s_radio_tx_busy)
    {
        return;
    }

    const TickType_t now = xTaskGetTickCount();
    if ((int32_t)(now - s_pending_command.deadline) < 0)
    {
        return;
    }

    if (s_pending_command.attempts >= COMMAND_MAX_ATTEMPTS)
    {
        ESP_LOGW(TAG,
                 "CMD failed seq=%u reason=ack_timeout attempts=%u",
                 (unsigned)s_pending_command.sequence,
                 (unsigned)s_pending_command.attempts);
        complete_pending_command(false, "client command acknowledgement timeout");
        return;
    }

    ESP_LOGW(TAG,
             "CMD retry seq=%u next_attempt=%u reason=%s",
             (unsigned)s_pending_command.sequence,
             (unsigned)(s_pending_command.attempts + 1U),
             s_pending_command.awaiting_ack ? "ack_timeout" : "tx_failed");
    s_pending_command.awaiting_ack = false;
    (void)transmit_pending_command();
}

static void handle_host_line(char *line)
{
    char direction[16] = {0};
    char device_id_text[96] = {0};
    char host_device_id[ISSP154_HOST_DEVICE_ID_BUFFER_SIZE] = {0};
    char capability_name[ISSP154_CAPABILITY_NAME_BUFFER_SIZE] = {0};
    char type[64] = {0};
    char value[128] = {0};
    uint8_t endpoint_id = 0;

    ESP_LOGI(TAG, "host line rx len=%u", (unsigned)strlen(line));

    if (skip_json_ws(line)[0] != '{')
    {
        ESP_LOGW(TAG, "ignored host line: invalid JSON object");
        return;
    }

    if (json_get_string(line, "direction", direction, sizeof(direction)) && strcmp(direction, "cmd") != 0)
    {
        return;
    }

    if (!json_get_string(line, "device_id", device_id_text, sizeof(device_id_text)) ||
        !json_get_string(line, "capability_name", capability_name, sizeof(capability_name)) ||
        !json_get_string(line, "value", value, sizeof(value)))
    {
        ESP_LOGW(TAG, "ignored host cmd: missing string field");
        return;
    }
    (void)json_get_string(line, "type", type, sizeof(type));

    if (!extract_host_device_id_and_endpoint_from_capability(capability_name,
                                                             host_device_id,
                                                             sizeof(host_device_id),
                                                             &endpoint_id))
    {
        ESP_LOGW(TAG, "ignored host cmd: invalid capability target capability=%s", capability_name);
        host_send_error(device_id_text, capability_name, value, "invalid capability target");
        return;
    }

    uint8_t ext_addr[IOT154_EXT_ADDR_LEN] = {0};
    if (!parse_host_ext_addr(host_device_id, ext_addr))
    {
        ESP_LOGW(TAG, "ignored host cmd: invalid ISSP154 device id %s", host_device_id);
        host_send_error(device_id_text, capability_name, value, "invalid issp154 device id");
        return;
    }

    uint8_t event_type = 0;
    uint8_t event_value = 0;
    if (!command_to_event(capability_name, type, value, &event_type, &event_value))
    {
        ESP_LOGW(TAG, "ignored host cmd: unsupported capability/value capability=%s type=%s value=%s", capability_name, type, value);
        host_send_error(device_id_text, capability_name, value, "unsupported capability or value");
        return;
    }

    const host_command_start_result_t start_result = start_host_command(host_device_id,
                            device_id_text,
                            capability_name,
                            value,
                            endpoint_id,
                            event_type,
                            event_value);
    if (start_result != HOST_COMMAND_START_OK)
    {
        /* COORD-REG-011/5.1: report the precise reason so RegistryUnavailable is never surfaced
           to the host as "target not known" (unknown identity). */
        const char *reason;
        switch (start_result)
        {
        case HOST_COMMAND_START_PENDING:
            reason = "another command is pending";
            break;
        case HOST_COMMAND_START_REGISTRY_UNAVAILABLE:
            reason = "registry unavailable";
            break;
        case HOST_COMMAND_START_INVALID_ADDR:
            reason = "invalid issp154 device id";
            break;
        case HOST_COMMAND_START_UNKNOWN_DEVICE:
        default:
            reason = "target not known";
            break;
        }
        ESP_LOGW(TAG, "ignored host cmd: unavailable host_id=%s capability=%s value=%s reason=%s",
                host_device_id, capability_name, value, reason);
        host_send_error(device_id_text, capability_name, value, reason);
        return;
    }

    ESP_LOGI(TAG, "host cmd queued host_id=%s endpoint=%u capability=%s value=%s", host_device_id, endpoint_id, capability_name, value);
}

static void poll_host_uart(void)
{
    static char line[HOST_UART_LINE_MAX];
    static size_t line_len;
    static bool in_string;
    static bool escape_next;
    static int brace_depth;
    uint8_t bytes[128];

    int read = uart_read_bytes(HOST_UART_NUM, bytes, sizeof(bytes), 0);
    for (int i = 0; i < read; ++i)
    {
        const char ch = (char)bytes[i];
        if (ch == '\n')
        {
            if (line_len > 0 && line[line_len - 1] == '\r')
            {
                line_len--;
            }
            line[line_len] = '\0';
            if (line_len > 0)
            {
                handle_host_line(line);
            }
            line_len = 0;
            in_string = false;
            escape_next = false;
            brace_depth = 0;
            continue;
        }

        if (line_len < sizeof(line) - 1)
        {
            line[line_len++] = ch;
            line[line_len] = '\0';
        }
        else
        {
            ESP_LOGW(TAG, "discarding oversized host line");
            line_len = 0;
            in_string = false;
            escape_next = false;
            brace_depth = 0;
            continue;
        }

        if (escape_next)
        {
            escape_next = false;
            continue;
        }
        if (ch == '\\' && in_string)
        {
            escape_next = true;
            continue;
        }
        if (ch == '"')
        {
            in_string = !in_string;
            continue;
        }
        if (in_string)
        {
            continue;
        }

        if (ch == '{')
        {
            brace_depth++;
        }
        else if (ch == '}' && brace_depth > 0)
        {
            brace_depth--;
            if (brace_depth == 0 && line_len > 0)
            {
                handle_host_line(line);
                line_len = 0;
                in_string = false;
                escape_next = false;
            }
        }
    }
}

void app_main(void)
{
    char central_mac_text[3 * IOT154_EXT_ADDR_LEN] = {0};

    const esp_err_t nvs_result = init_nvs();
    if (nvs_result != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS initialization failed; coordinator device traffic remains disabled: %s",
                 esp_err_to_name(nvs_result));
        return;
    }
    device_registry_init(device_registry_nvs_storage());
    (void)device_registry_load();
    init_host_uart();
    ESP_ERROR_CHECK(esp_read_mac(s_central_ext_addr, ESP_MAC_IEEE802154));
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(iot154_radio_init(IOT154_CENTRAL_ADDR, true, true,
                                      on_rx_done, on_tx_done, on_tx_failed));
    ESP_ERROR_CHECK(esp_ieee802154_set_extended_address(s_central_ext_addr));
    ESP_ERROR_CHECK(iot154_radio_start_rx());
    // xTaskCreate(test_toggle_task, "test_toggle", 3072, NULL, 4, NULL);

    format_ext_addr(s_central_ext_addr, central_mac_text, sizeof(central_mac_text));
    ESP_LOGI(TAG,
             "central RX channel=%d pan=0x%04x short=0x%04x ext=%s",
             IOT154_CHANNEL,
             IOT154_PAN_ID,
             IOT154_CENTRAL_ADDR,
             central_mac_text);

    s_join_window_open = true;
    s_join_window_deadline_us = esp_timer_get_time() +
                                (int64_t)COMMISSIONING_JOIN_WINDOW_SECONDS * 1000000LL;
    ESP_LOGI("COMMISSIONING", "join_window opened duration_s=%u",
             (unsigned)COMMISSIONING_JOIN_WINDOW_SECONDS);

    host_send_gateway("hello");

    while (true)
    {
        update_join_window();
        EventBits_t bits = xEventGroupWaitBits(s_events,
                                               RX_DONE_BIT | TX_DONE_BIT | TX_FAILED_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(50));

        if ((bits & RX_DONE_BIT) != 0)
        {
            uint8_t frame[IOT154_MAX_FRAME_LEN + 1];
            const uint8_t rx_len = s_rx_len;
            memcpy(frame, s_rx_frame, rx_len + 1);
            const esp_ieee802154_frame_info_t rx_info = s_rx_info;
            const int64_t rx_dispatch_us = esp_timer_get_time();
            iot154_frame_info_t diagnostic_mac = {0};
            iot154_packet_t diagnostic_packet = {0};
            size_t payload_length = 0;
            const char *mac_rejection_reason = NULL;
            if (!diagnostic_extract_mac(frame,
                                        &diagnostic_mac,
                                        &diagnostic_packet,
                                        &payload_length,
                                        &mac_rejection_reason))
            {
                ESP_LOGW(TAG, "invalid MAC frame discarded reason=%s",
                         mac_rejection_reason);
                continue;
            }

            iot154_frame_info_t mac = {0};
            iot154_packet_t packet = {0};
            if (!iot154_parse_frame_info(frame, &mac, &packet))
            {
                ESP_LOGW(TAG, "ignored frame: invalid packet");
                continue;
            }

            if (packet.msg_type == IOT154_MSG_DISCOVERY_REQ &&
                mac.dst_mode == IOT154_ADDR_MODE_SHORT &&
                mac.dst_broadcast &&
                mac.src_mode == IOT154_ADDR_MODE_EXT)
            {
                char sensor_mac_text[3 * IOT154_EXT_ADDR_LEN] = {0};
                format_ext_addr(mac.src_ext, sensor_mac_text, sizeof(sensor_mac_text));
                if (device_registry_state() != DEVICE_REGISTRY_STATE_READY)
                {
                    ESP_LOGW("COMMISSIONING",
                             "discovery ignored reason=registry_unavailable dev=0x%08" PRIx32,
                             packet.device_id);
                }
                else if (!s_join_window_open)
                {
                    ESP_LOGI("COMMISSIONING",
                             "discovery ignored reason=join_window_closed dev=0x%08" PRIx32,
                             packet.device_id);
                }
                else
                {
                    const device_registry_pair_result_t pair_result =
                        device_registry_pair(mac.src_ext, packet.device_id);
                    if (pair_result == DEVICE_REGISTRY_PAIR_KNOWN ||
                        pair_result == DEVICE_REGISTRY_PAIR_UPDATED ||
                        pair_result == DEVICE_REGISTRY_PAIR_CREATED)
                    {
                        ESP_LOGI("COMMISSIONING",
                                 "discovery accepted dev=0x%08" PRIx32 " seq=%u sensor=%s",
                                 packet.device_id, packet.seq, sensor_mac_text);
                        send_discovery_response(packet.device_id, packet.seq, mac.src_ext);
                    }
                    else
                    {
                        ESP_LOGW("COMMISSIONING",
                                 "discovery rejected dev=0x%08" PRIx32 " seq=%u sensor=%s",
                                 packet.device_id, packet.seq, sensor_mac_text);
                    }
                }
            }
            else if (packet.msg_type == IOT154_MSG_DATA &&
                     mac.dst_mode == IOT154_ADDR_MODE_EXT &&
                     iot154_ext_addr_equal(mac.dst_ext, s_central_ext_addr) &&
                     mac.src_mode == IOT154_ADDR_MODE_EXT)
            {
                size_t registry_index = 0;
                if (device_registry_state() != DEVICE_REGISTRY_STATE_READY)
                {
                    ESP_LOGW("DEVICE_REGISTRY",
                             "frame ignored reason=registry_unavailable dev=0x%08" PRIx32,
                             packet.device_id);
                }
                else if (!device_registry_find(mac.src_ext, NULL, &registry_index))
                {
                    if (!s_join_window_open)
                    {
                        char sensor_mac_text[3 * IOT154_EXT_ADDR_LEN] = {0};
                        format_ext_addr(mac.src_ext, sensor_mac_text, sizeof(sensor_mac_text));
                        ESP_LOGI("DEVICE_REGISTRY",
                                 "frame ignored reason=unknown_device dev=0x%08" PRIx32 " sensor=%s",
                                 packet.device_id, sensor_mac_text);
                        /* discarded per COORD-REG-007/008: no ACK, no host event, no registry write */
                    }
                    else
                    {
                        /* Join window open, unknown origin: acceptance policy is out of this spec's
                           scope (section 9); preserve prior processing without creating persistence. */
                        ESP_LOGI(TAG,
                                 "DATA new dev=0x%08" PRIx32 " seq=%u endpoint=%u event=%u value=%u",
                                 packet.device_id, packet.seq, packet.endpoint_id, packet.event_type, packet.value);
                        host_send_event(mac.src_ext, packet.endpoint_id, packet.event_type, packet.value);
                        send_radio_ack(packet.device_id, packet.seq, packet.endpoint_id, mac.src_ext);
                    }
                }
                else
                {
                    const bool duplicate = is_known_data_duplicate(registry_index, packet.seq);
                    if (duplicate)
                    {
                        ESP_LOGI(TAG, "DATA duplicate dev=0x%08" PRIx32 " seq=%u", packet.device_id, packet.seq);
                    }
                    else
                    {
                        ESP_LOGI(TAG,
                                 "DATA new dev=0x%08" PRIx32 " seq=%u endpoint=%u event=%u value=%u",
                                 packet.device_id, packet.seq, packet.endpoint_id, packet.event_type, packet.value);
                        host_send_event(mac.src_ext, packet.endpoint_id, packet.event_type, packet.value);
                    }
                    send_radio_ack(packet.device_id, packet.seq, packet.endpoint_id, mac.src_ext);
                }
            }
            else if (packet.msg_type == IOT154_MSG_ACK &&
                     mac.dst_mode == IOT154_ADDR_MODE_EXT &&
                     iot154_ext_addr_equal(mac.dst_ext, s_central_ext_addr) &&
                     mac.src_mode == IOT154_ADDR_MODE_EXT)
            {
                uint32_t known_device_id = 0;
                const bool registry_ready = device_registry_state() == DEVICE_REGISTRY_STATE_READY;
                const bool origin_known = registry_ready &&
                                          device_registry_find(mac.src_ext, &known_device_id, NULL);
                const bool matches_pending = s_pending_command.active &&
                                             packet.device_id == s_pending_command.device_id &&
                                             packet.seq == s_pending_command.sequence &&
                                             packet.endpoint_id == s_pending_command.endpoint_id &&
                                             iot154_ext_addr_equal(mac.src_ext,
                                                                   s_pending_command.destination) &&
                                             origin_known &&
                                             known_device_id == packet.device_id;
                ESP_LOGI(TAG,
                         "CMD ACK dev=0x%08" PRIx32 " seq=%u endpoint=%u status=%u match_pending=%s",
                         packet.device_id,
                         packet.seq,
                         packet.endpoint_id,
                         packet.value,
                         matches_pending ? "yes" : "no");
                if (!registry_ready)
                {
                    ESP_LOGW("DEVICE_REGISTRY", "frame ignored reason=registry_unavailable");
                }
                if (matches_pending)
                {
                    if (packet.value == IOT154_ACK_STATUS_OK)
                    {
                        complete_pending_command(true, NULL);
                    }
                    else if (packet.value == IOT154_ACK_STATUS_UNSUPPORTED)
                    {
                        complete_pending_command(false,
                                                 "client rejected unsupported command");
                    }
                    else
                    {
                        complete_pending_command(false,
                                                 "client rejected invalid command");
                    }
                }
            }
            else
            {
                if (packet.msg_type == IOT154_MSG_DATA)
                {
                    ESP_LOGW(TAG, "report rejected reason=invalid_mac_origin seq=%u",
                             (unsigned)packet.seq);
                }
                ESP_LOGW(TAG, "ignored frame: msg=%u not for this central", packet.msg_type);
            }
            ESP_LOGI("COORD_RADIO_TRACE",
                     "event=rx_processed role=%u device=0x%08" PRIx32 " issp_seq=%u endpoint=%u mac_seq=%u phy_len=%u channel=%u rssi=%d lqi=%u sfd_us=%llu sfd_to_dispatch_us=%lld processing_us=%lld rx_total=%lu overwrites=%lu",
                     (unsigned)packet.msg_type,
                     packet.device_id,
                     (unsigned)packet.seq,
                     (unsigned)packet.endpoint_id,
                     (unsigned)(rx_len >= 3U ? frame[3] : 0U),
                     (unsigned)rx_len,
                     (unsigned)rx_info.channel,
                     (int)rx_info.rssi,
                     (unsigned)rx_info.lqi,
                     (unsigned long long)rx_info.timestamp,
                     (long long)(rx_dispatch_us - (int64_t)rx_info.timestamp),
                     (long long)(esp_timer_get_time() - rx_dispatch_us),
                     (unsigned long)s_rx_total_count,
                     (unsigned long)s_rx_overwrite_count);
        }

        if ((bits & TX_DONE_BIT) != 0)
        {
            const bool command_tx = s_tx_is_command;
            ESP_LOGI(TAG, "%s TX done seq=%u", s_tx_type, s_radio_tx_seq);
            ESP_LOGI("COORD_RADIO_TRACE",
                     "event=tx_complete role=%s issp_seq=%u result=ok elapsed_us=%lld callback_us=%lld state=%u",
                     s_tx_type,
                     (unsigned)s_radio_tx_seq,
                     (long long)(s_radio_tx_callback_us - s_radio_tx_started_us),
                     (long long)s_radio_tx_callback_us,
                     (unsigned)esp_ieee802154_get_state());
            s_radio_tx_busy = false;
            const esp_err_t restore_result = iot154_radio_start_rx();
            if (command_tx)
            {
                s_tx_is_command = false;
                if (s_pending_command.active &&
                    s_pending_command.sequence == s_radio_tx_seq)
                {
                    s_pending_command.awaiting_ack = true;
                    s_pending_command.deadline =
                        xTaskGetTickCount() +
                        pdMS_TO_TICKS(COMMAND_ACK_TIMEOUT_MS);
                    ESP_LOGI(TAG,
                             "CMD awaiting ACK seq=%u attempt=%u timeout_ms=%u",
                             (unsigned)s_pending_command.sequence,
                             (unsigned)s_pending_command.attempts,
                             (unsigned)COMMAND_ACK_TIMEOUT_MS);
                }
            }
            if (s_tx_is_report_ack)
            {
                s_tx_is_report_ack = false;
            }
            ESP_ERROR_CHECK(restore_result);
        }

        if ((bits & TX_FAILED_BIT) != 0)
        {
            const bool command_tx = s_tx_is_command;
            ESP_LOGW(TAG, "%s TX failed seq=%u error=%s error_code=%d",
                     s_tx_type,
                     s_radio_tx_seq,
                     radio_tx_error_name(s_radio_tx_error),
                     (int)s_radio_tx_error);
            ESP_LOGW("COORD_RADIO_TRACE",
                     "event=tx_complete role=%s issp_seq=%u result=failed error=%s error_code=%d elapsed_us=%lld callback_us=%lld state=%u",
                     s_tx_type,
                     (unsigned)s_radio_tx_seq,
                     radio_tx_error_name(s_radio_tx_error),
                     (int)s_radio_tx_error,
                     (long long)(s_radio_tx_callback_us - s_radio_tx_started_us),
                     (long long)s_radio_tx_callback_us,
                     (unsigned)esp_ieee802154_get_state());
            s_radio_tx_busy = false;
            const esp_err_t restore_result = iot154_radio_start_rx();
            if (command_tx)
            {
                s_tx_is_command = false;
                if (s_pending_command.active &&
                    s_pending_command.sequence == s_radio_tx_seq)
                {
                    s_pending_command.awaiting_ack = false;
                    s_pending_command.deadline =
                        xTaskGetTickCount() +
                        pdMS_TO_TICKS(COMMAND_RETRY_DELAY_MS);
                }
            }
            if (s_tx_is_report_ack)
            {
                s_tx_is_report_ack = false;
            }
            ESP_ERROR_CHECK(restore_result);
        }

        poll_host_uart();
        process_pending_command();
    }
}
