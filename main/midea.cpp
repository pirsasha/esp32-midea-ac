// midea.cpp — реализация UART-протокола Midea для бытовых сплит-систем.
//
// Структура кадра (V3 протокол, что использует OSK103/302):
//   0xAA                    sync
//   length-1                длина с этого байта до checksum включительно
//   0xAC                    тип устройства (split AC)
//   0x00 0x00 0x00 0x00     sync ID (4 нулевых байта)
//   0x00                    frame protocol version
//   0x00                    device protocol version
//   msg_type                0x02 = set, 0x03 = query
//   ... payload ...
//   crc8                    по байтам payload
//   checksum                (256 - сумма байтов с length до crc8) & 0xFF
//
// Описания команд и таблица CRC8 — на основе открытых проектов
// mac-zhou/midea-msmart и dudanov/MideaUART. Если поведение не соответствует
// ожиданиям, в первую очередь проверяйте байты в build_set_frame().

#include "midea.h"

#include <string.h>
#include <stdio.h>
#include <atomic>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

namespace midea {

static const char *TAG = "midea";

// --------- параметры UART ---------
static constexpr uart_port_t  UART_NUM       = UART_NUM_1;
static constexpr int          UART_BAUD      = 9600;
static constexpr int          RX_BUF_SIZE    = 512;
static constexpr int          TX_BUF_SIZE    = 0;     // 0 = блокирующая запись через драйвер
static constexpr TickType_t   POLL_PERIOD    = pdMS_TO_TICKS(5000);
static constexpr TickType_t   RX_READ_TICK   = pdMS_TO_TICKS(50);

// --------- константы протокола ---------
static constexpr uint8_t SYNC_BYTE      = 0xAA;
static constexpr uint8_t APPLIANCE_AC   = 0xAC;
static constexpr uint8_t MSG_TYPE_SET   = 0x02;
static constexpr uint8_t MSG_TYPE_QUERY = 0x03;
static constexpr size_t  HEADER_LEN     = 10;   // от 0xAA до msg_type включительно

// --------- CRC8 таблица (полином 0x131, как у Midea) ---------
// Идентична таблице из midea-msmart / MideaUART.
static const uint8_t kCrc8Table[256] = {
    0x00,0x5E,0xBC,0xE2,0x61,0x3F,0xDD,0x83,0xC2,0x9C,0x7E,0x20,0xA3,0xFD,0x1F,0x41,
    0x9D,0xC3,0x21,0x7F,0xFC,0xA2,0x40,0x1E,0x5F,0x01,0xE3,0xBD,0x3E,0x60,0x82,0xDC,
    0x23,0x7D,0x9F,0xC1,0x42,0x1C,0xFE,0xA0,0xE1,0xBF,0x5D,0x03,0x80,0xDE,0x3C,0x62,
    0xBE,0xE0,0x02,0x5C,0xDF,0x81,0x63,0x3D,0x7C,0x22,0xC0,0x9E,0x1D,0x43,0xA1,0xFF,
    0x46,0x18,0xFA,0xA4,0x27,0x79,0x9B,0xC5,0x84,0xDA,0x38,0x66,0xE5,0xBB,0x59,0x07,
    0xDB,0x85,0x67,0x39,0xBA,0xE4,0x06,0x58,0x19,0x47,0xA5,0xFB,0x78,0x26,0xC4,0x9A,
    0x65,0x3B,0xD9,0x87,0x04,0x5A,0xB8,0xE6,0xA7,0xF9,0x1B,0x45,0xC6,0x98,0x7A,0x24,
    0xF8,0xA6,0x44,0x1A,0x99,0xC7,0x25,0x7B,0x3A,0x64,0x86,0xD8,0x5B,0x05,0xE7,0xB9,
    0x8C,0xD2,0x30,0x6E,0xED,0xB3,0x51,0x0F,0x4E,0x10,0xF2,0xAC,0x2F,0x71,0x93,0xCD,
    0x11,0x4F,0xAD,0xF3,0x70,0x2E,0xCC,0x92,0xD3,0x8D,0x6F,0x31,0xB2,0xEC,0x0E,0x50,
    0xAF,0xF1,0x13,0x4D,0xCE,0x90,0x72,0x2C,0x6D,0x33,0xD1,0x8F,0x0C,0x52,0xB0,0xEE,
    0x32,0x6C,0x8E,0xD0,0x53,0x0D,0xEF,0xB1,0xF0,0xAE,0x4C,0x12,0x91,0xCF,0x2D,0x73,
    0xCA,0x94,0x76,0x28,0xAB,0xF5,0x17,0x49,0x08,0x56,0xB4,0xEA,0x69,0x37,0xD5,0x8B,
    0x57,0x09,0xEB,0xB5,0x36,0x68,0x8A,0xD4,0x95,0xCB,0x29,0x77,0xF4,0xAA,0x48,0x16,
    0xE9,0xB7,0x55,0x0B,0x88,0xD6,0x34,0x6A,0x2B,0x75,0x97,0xC9,0x4A,0x14,0xF6,0xA8,
    0x74,0x2A,0xC8,0x96,0x15,0x4B,0xA9,0xF7,0xB6,0xE8,0x0A,0x54,0xD7,0x89,0x6B,0x35,
};

static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc = kCrc8Table[crc ^ data[i]];
    }
    return crc;
}

// «Чексумма» Midea: -sum(bytes) mod 256
static uint8_t checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; ++i) sum += data[i];
    return (uint8_t)((256u - (sum & 0xFFu)) & 0xFFu);
}

// --------- состояние ---------
static state_cb_t       g_cb           = nullptr;
static display_sound_cb_t g_display_sound_cb = nullptr;
static SemaphoreHandle_t g_state_mtx   = nullptr;
static State            g_state        = {};
static std::atomic<uint8_t> g_msg_id{0};

// Простая очередь TX-кадров: задача-опросчик читает её и пишет в UART.
static QueueHandle_t    g_tx_queue     = nullptr;

static void log_hex(const char *prefix, const uint8_t *data, size_t len) {
    char line[3 * 64 + 1];
    size_t pos = 0;
    size_t limit = len < 64 ? len : 64;

    for (size_t i = 0; i < limit && pos + 3 < sizeof(line); ++i) {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[i]);
    }
    line[pos] = '\0';
    ESP_LOGI(TAG, "%s %u bytes: %s", prefix, (unsigned)len, line);
}

struct TxItem {
    uint8_t buf[64];
    size_t  len;
};

// --------- разбор ответа 0xC0 (status) ---------
//
// Структура payload ответа 0xC0 (после общего заголовка кадра):
//   [0]  0xC0
//   [1]  power|prompt|fast_check (bit0 = power on)
//   [2]  mode<<5 | (target_int-16) & 0x0F | half<<4
//   [3]  fan_speed
//   [4]  on_timer
//   [5]  off_timer
//   [6]  extra time bits
//   [7]  swing  (нижние биты — V, верхние — H)
//   [8]  cosy_sleep|turbo|eco
//   [9..10] ?
//   [11] indoor_temp_int (значение*0.5+16 для некоторых прошивок,
//                          либо (T-50)/2 — варианты зависят от прошивки)
//   [12] outdoor_temp_int
//   ...
// Поля, которые здесь не разбираются, можно дополнить по логам реальных кадров.

static void parse_status(const uint8_t *p, size_t plen) {
    if (plen < 13) {
        ESP_LOGW(TAG, "status too short: %u", (unsigned)plen);
        return;
    }

    State st = {};
    st.valid    = true;
    st.power_on = (p[1] & 0x01) != 0;

    uint8_t mode_bits = (p[2] >> 5) & 0x07;
    switch (mode_bits) {
        case 1: st.mode = Mode::Auto; break;
        case 2: st.mode = Mode::Cool; break;
        case 3: st.mode = Mode::Dry;  break;
        case 4: st.mode = Mode::Heat; break;
        case 5: st.mode = Mode::Fan;  break;
        default: st.mode = Mode::Auto; break;
    }

    uint8_t temp_int = (p[2] & 0x0F) + 16;
    bool    half     = (p[2] & 0x10) != 0;
    st.set_temp = (float)temp_int + (half ? 0.5f : 0.0f);

    // Скорость вентилятора
    uint8_t fan_byte = p[3];
    if      (fan_byte >= 100) st.fan = FanSpeed::Auto;
    else if (fan_byte >= 70)  st.fan = FanSpeed::High;
    else if (fan_byte >= 50)  st.fan = FanSpeed::Medium;
    else if (fan_byte >= 30)  st.fan = FanSpeed::Low;
    else                      st.fan = FanSpeed::Silent;

    st.swing_v = (p[7] & 0x0C) != 0;
    st.swing_h = (p[7] & 0x03) != 0;

    if (plen > 10) {
        st.eco   = (p[9] & (0x80 | 0x10)) != 0;
        st.turbo = ((p[8] & 0x20) != 0) || ((p[10] & 0x02) != 0);
        st.sleep = (p[10] & 0x01) != 0;
    }

    // Температуры — формула из msmart: T = (raw - 50) / 2  °C
    if (plen > 11 && p[11] != 0xFF) {
        st.indoor_temp  = ((float)p[11] - 50.0f) / 2.0f;
    }
    if (plen > 12 && p[12] != 0xFF) {
        st.outdoor_temp = ((float)p[12] - 50.0f) / 2.0f;
    }

    // Сохраняем в shadow и уведомляем
    if (xSemaphoreTake(g_state_mtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_state = st;
        xSemaphoreGive(g_state_mtx);
    }
    if (g_cb) g_cb(st);

    ESP_LOGI(TAG, "status: power=%d mode=%d set=%.1f in=%.1f out=%.1f fan=%u",
             st.power_on, (int)st.mode, st.set_temp, st.indoor_temp, st.outdoor_temp,
             (unsigned)st.fan);
}

// --------- сборка кадров ---------

// Заполнить заголовок 10 байт.
static void fill_header(uint8_t *buf, size_t frame_len, uint8_t msg_type) {
    buf[0] = SYNC_BYTE;
    buf[1] = (uint8_t)frame_len;
    buf[2] = APPLIANCE_AC;
    buf[3] = buf[1] ^ buf[2];
    buf[4] = 0x00;
    buf[5] = 0x00;
    buf[6] = 0x00;
    buf[7] = 0x00;
    buf[8] = 0x00;
    buf[9] = msg_type;
}

// Кадр query status (0x41, msg_type=0x03). Длина 32 байта.
static size_t build_query_frame(uint8_t *out) {
    static const uint8_t kQueryBody[] = {
        0x41, 0x81, 0x00, 0xFF, 0x03, 0xFF, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    };
    const size_t body_len = sizeof(kQueryBody);
    const size_t frame_len = HEADER_LEN + body_len + 1 /*msg_id*/ + 1 /*crc*/;
    const size_t total = frame_len + 1 /*checksum*/;

    fill_header(out, frame_len, MSG_TYPE_QUERY);
    memcpy(out + HEADER_LEN, kQueryBody, body_len);
    out[HEADER_LEN + body_len] = g_msg_id.fetch_add(1);

    const size_t payload_off = HEADER_LEN;
    const size_t payload_len = body_len + 1;
    out[payload_off + payload_len] = crc8(out + payload_off, payload_len);
    out[total - 1] = checksum(out + 1, frame_len - 1);
    return total;
}

// Кадр set state (0x40, msg_type=0x02). Длина 32 байта.
// Использует текущую «тень» состояния, переопределяя только указанные поля.
static size_t build_display_toggle_frame(uint8_t *out) {
    uint8_t body[] = {
        0x41, 0x61, 0x00, 0xFF, 0x02, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, g_msg_id.fetch_add(1),
    };
    const size_t body_len = sizeof(body);
    const size_t frame_len = HEADER_LEN + body_len + 1 /*crc*/;
    const size_t total = frame_len + 1 /*checksum*/;

    fill_header(out, frame_len, MSG_TYPE_SET);
    memcpy(out + HEADER_LEN, body, body_len);
    out[HEADER_LEN + body_len] = crc8(out + HEADER_LEN, body_len);
    out[total - 1] = checksum(out + 1, frame_len - 1);
    return total;
}

struct SetOverride {
    enum class Field : uint8_t { Power, Mode, Temp, Fan, Eco, Turbo, Sleep, Preset, SwingV, SwingH, Swing };
    Field       field;
    bool        b_value;
    bool        b2_value;
    Mode        mode_value;
    float       temp_value;
    FanSpeed    fan_value;
};

static size_t build_set_frame(uint8_t *out, const SetOverride &ov) {
    // Снимок текущего состояния — модифицируем то поле, что просили.
    State st;
    if (xSemaphoreTake(g_state_mtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        st = g_state;
        xSemaphoreGive(g_state_mtx);
    } else {
        st = {};
    }
    if (!st.valid) {
        // Дефолты, если ещё не получили статус ни разу.
        st.power_on = false;
        st.mode     = Mode::Cool;
        st.set_temp = 24.0f;
        st.fan      = FanSpeed::Auto;
    }

    switch (ov.field) {
        case SetOverride::Field::Power: st.power_on = ov.b_value;    break;
        case SetOverride::Field::Mode:  st.mode     = ov.mode_value; break;
        case SetOverride::Field::Temp:  st.set_temp = ov.temp_value; break;
        case SetOverride::Field::Fan:   st.fan      = ov.fan_value;  break;
        case SetOverride::Field::Eco:    st.eco      = ov.b_value;    break;
        case SetOverride::Field::Turbo:  st.turbo    = ov.b_value;    break;
        case SetOverride::Field::Sleep:  st.sleep    = ov.b_value;    break;
        case SetOverride::Field::Preset:
            st.sleep = ov.b_value;
            st.turbo = ov.b2_value;
            st.eco = false;
            break;
        case SetOverride::Field::SwingV: st.swing_v  = ov.b_value;    break;
        case SetOverride::Field::SwingH: st.swing_h  = ov.b_value;    break;
        case SetOverride::Field::Swing:
            st.swing_h = ov.b_value;
            st.swing_v = ov.b2_value;
            break;
    }

    // Кодируем температуру: целая часть 16..30 → биты 0..3, половинка → бит 4
    if (st.set_temp < 16.0f) st.set_temp = 16.0f;
    if (st.set_temp > 30.0f) st.set_temp = 30.0f;
    uint8_t temp_int  = (uint8_t)st.set_temp;
    bool    temp_half = (st.set_temp - (float)temp_int) >= 0.25f;

    uint8_t body[25] = {};
    body[0]  = 0x40;
    body[1]  = 0x40                                  // beep prompt off
             | (st.power_on ? 0x01 : 0x00);
    body[2]  = ((uint8_t)st.mode << 5)
             | ((temp_int - 16) & 0x0F)
             | (temp_half ? 0x10 : 0x00);
    body[3]  = (uint8_t)st.fan;
    body[4]  = 0x7F;                                 // on-timer off
    body[5]  = 0x7F;                                 // off-timer off
    body[6]  = 0x00;
    body[7]  = 0x30
             | (st.swing_v ? 0x0C : 0x00)
             | (st.swing_h ? 0x03 : 0x00);
    body[8]  = (st.turbo ? 0x20 : 0x00);
    body[9]  = (st.eco ? 0x80 : 0x00);
    body[10] = (st.turbo ? 0x02 : 0x00)
             | (st.sleep ? 0x01 : 0x00);
    body[18] = (temp_int >= 12) ? ((temp_int - 12) & 0x1F) : 0;
    // body[9..23] остаются нулями — стандартные значения
    body[24] = g_msg_id.fetch_add(1);                // message id

    const size_t body_len = sizeof(body);
    const size_t frame_len = HEADER_LEN + body_len + 1 /*crc*/;
    const size_t total = frame_len + 1 /*checksum*/;
    fill_header(out, frame_len, MSG_TYPE_SET);
    memcpy(out + HEADER_LEN, body, body_len);

    out[HEADER_LEN + body_len] = crc8(out + HEADER_LEN, body_len);
    out[total - 1] = checksum(out + 1, frame_len - 1);
    return total;
}

// --------- TX-помощник ---------
static esp_err_t enqueue_frame(const uint8_t *buf, size_t len) {
    if (!g_tx_queue) return ESP_ERR_INVALID_STATE;
    if (len > sizeof(TxItem::buf)) return ESP_ERR_INVALID_SIZE;
    TxItem it;
    memcpy(it.buf, buf, len);
    it.len = len;
    return xQueueSend(g_tx_queue, &it, pdMS_TO_TICKS(200)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

// --------- RX state machine ---------
class FrameReader {
public:
    void feed(uint8_t b) {
        switch (state_) {
            case S_SYNC:
                if (b == SYNC_BYTE) {
                    buf_[0] = b;
                    idx_    = 1;
                    state_  = S_LEN;
                }
                break;
            case S_LEN:
                expected_total_ = b + 1u;  // +1 потому что length-1 в кадре
                if (expected_total_ < HEADER_LEN + 2 || expected_total_ > sizeof(buf_)) {
                    state_ = S_SYNC;
                    return;
                }
                buf_[idx_++] = b;
                state_       = S_BODY;
                break;
            case S_BODY:
                buf_[idx_++] = b;
                if (idx_ >= expected_total_) {
                    process();
                    state_ = S_SYNC;
                }
                break;
        }
    }

private:
    bool is_display_sound_frame() const {
        return expected_total_ == 31 &&
               buf_[0] == SYNC_BYTE &&
               buf_[1] == 0x1E &&
               buf_[2] == APPLIANCE_AC &&
               buf_[9] == 0x64;
    }

    void process() {
        if (is_display_sound_frame()) {
            log_hex("rx display/sound frame", buf_, expected_total_);
            if (g_display_sound_cb) {
                g_display_sound_cb();
            }
            return;
        }
        // Проверяем checksum
        uint8_t cs = checksum(buf_ + 1, expected_total_ - 2);
        if (cs != buf_[expected_total_ - 1]) {
            ESP_LOGW(TAG, "bad checksum: calc=%02X got=%02X", cs, buf_[expected_total_ - 1]);
            log_hex("rx bad frame", buf_, expected_total_);
            return;
        }
        if (buf_[2] != APPLIANCE_AC) {
            return;  // не наше устройство
        }
        // Извлекаем payload и проверяем CRC8
        size_t payload_len = expected_total_ - HEADER_LEN - 2; // -crc -checksum
        const uint8_t *payload = buf_ + HEADER_LEN;
        uint8_t crc_calc = crc8(payload, payload_len);
        if (crc_calc != buf_[expected_total_ - 2]) {
            // Не все прошивки добавляют CRC8 — некоторые ставят 0. Логируем,
            // но не отбрасываем, чтобы не потерять данные.
            ESP_LOGI(TAG, "crc8 mismatch (calc=%02X, got=%02X), continuing",
                     crc_calc, buf_[expected_total_ - 2]);
        }
        if (payload_len < 1) return;

        uint8_t cmd = payload[0];
        log_hex("rx frame", buf_, expected_total_);
        if (cmd == 0xC0 || cmd == 0xA0) {
            parse_status(payload, payload_len);
        } else {
            ESP_LOGI(TAG, "rx cmd=0x%02X len=%u", cmd, (unsigned)payload_len);
        }
    }

    enum { S_SYNC, S_LEN, S_BODY } state_ = S_SYNC;
    uint8_t  buf_[64];
    size_t   idx_           = 0;
    size_t   expected_total_= 0;
};

// --------- задача RX/TX ---------
static void uart_task(void *) {
    FrameReader reader;
    uint8_t     rx_buf[64];
    TickType_t  next_poll = xTaskGetTickCount() + pdMS_TO_TICKS(1000);

    // первичный запрос статуса
    request_status();

    while (true) {
        // приём
        int n = uart_read_bytes(UART_NUM, rx_buf, sizeof(rx_buf), RX_READ_TICK);
        if (n > 0) {
            log_hex("rx raw", rx_buf, n);
            for (int i = 0; i < n; ++i) reader.feed(rx_buf[i]);
        }

        // передача из очереди
        TxItem item;
        while (xQueueReceive(g_tx_queue, &item, 0) == pdTRUE) {
            int written = uart_write_bytes(UART_NUM, (const char *)item.buf, item.len);
            ESP_LOGI(TAG, "tx %d bytes (wrote %d)", (int)item.len, written);
            log_hex("tx frame", item.buf, item.len);
        }

        // периодический опрос
        if ((int)(xTaskGetTickCount() - next_poll) >= 0) {
            request_status();
            next_poll += POLL_PERIOD;
        }
    }
}

// --------- публичный API ---------

esp_err_t init(int tx_gpio, int rx_gpio, state_cb_t cb) {
    g_cb        = cb;
    g_state_mtx = xSemaphoreCreateMutex();
    g_tx_queue  = xQueueCreate(8, sizeof(TxItem));
    if (!g_state_mtx || !g_tx_queue) return ESP_ERR_NO_MEM;

    uart_config_t cfg = {};
    cfg.baud_rate = UART_BAUD;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity    = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err;
    err = uart_driver_install(UART_NUM, RX_BUF_SIZE, TX_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) { ESP_LOGE(TAG, "uart_driver_install: %s", esp_err_to_name(err)); return err; }
    err = uart_param_config(UART_NUM, &cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "uart_param_config: %s", esp_err_to_name(err)); return err; }
    err = uart_set_pin(UART_NUM, tx_gpio, rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) { ESP_LOGE(TAG, "uart_set_pin: %s", esp_err_to_name(err)); return err; }

    BaseType_t ok = xTaskCreate(uart_task, "midea_uart", 4096, NULL, 7, NULL);
    if (ok != pdPASS) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "Midea UART started on TX=%d RX=%d @ %d 8N1", tx_gpio, rx_gpio, UART_BAUD);
    return ESP_OK;
}

void set_display_sound_callback(display_sound_cb_t cb) {
    g_display_sound_cb = cb;
}

State get_state() {
    State copy = {};
    if (g_state_mtx && xSemaphoreTake(g_state_mtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        copy = g_state;
        xSemaphoreGive(g_state_mtx);
    }
    return copy;
}

esp_err_t set_power(bool on) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::Power, on, false, Mode::Auto, 0, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t set_mode(Mode mode) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::Mode, false, false, mode, 0, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t set_target_temp(float celsius) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::Temp, false, false, Mode::Auto, celsius, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t set_fan(FanSpeed fan) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::Fan, false, false, Mode::Auto, 0, fan};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t toggle_display() {
    uint8_t f[64];
    size_t len = build_display_toggle_frame(f);
    log_hex("tx DISPLAY TOGGLE", f, len);
    return enqueue_frame(f, len);
}

esp_err_t set_turbo(bool on) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::Turbo, on, false, Mode::Auto, 0, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t set_sleep(bool on) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::Sleep, on, false, Mode::Auto, 0, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t set_preset(bool sleep, bool turbo) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::Preset, sleep, turbo, Mode::Auto, 0, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t set_swing_v(bool on) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::SwingV, on, false, Mode::Auto, 0, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t set_swing_h(bool on) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::SwingH, on, false, Mode::Auto, 0, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t set_swing(bool horizontal, bool vertical) {
    uint8_t f[64];
    SetOverride ov{SetOverride::Field::Swing, horizontal, vertical, Mode::Auto, 0, FanSpeed::Auto};
    size_t len = build_set_frame(f, ov);
    return enqueue_frame(f, len);
}

esp_err_t request_status() {
    uint8_t f[64];
    size_t len = build_query_frame(f);
    return enqueue_frame(f, len);
}

}  // namespace midea
