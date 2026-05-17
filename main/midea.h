// midea.h — UART-драйвер протокола Midea для управления кондиционером
// Royal Clima TRIUMPH = Midea OEM, фирменный USB-Wi-Fi модуль OSK103/302 общается с
// кондером по UART 9600 8N1 через 4-контактный разъём USB-A на внутреннем блоке.
//
// Распиновка разъёма (со стороны кондера):
//   pin 1 (край платы) — +5 V
//   pin 2              — RX кондера (TX нашей ESP)
//   pin 3              — TX кондера (RX нашей ESP)
//   pin 4              — GND
//
// !!! ВАЖНО: линия 5 В, поэтому RX ESP подключаем через делитель напряжения
//     или level shifter (BSS138). На TX ESP 3.3 В обычно воспринимается как «1»,
//     но безопаснее тоже через level shifter.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

namespace midea {

// Внутренние режимы Midea (значения совпадают с протокольными битами 5..7 байта 2 set-кадра)
enum class Mode : uint8_t {
    Auto = 1,
    Cool = 2,
    Dry  = 3,
    Heat = 4,
    Fan  = 5,
};

// Скорости вентилятора (значения для байта 3 set-кадра)
enum class FanSpeed : uint8_t {
    Auto   = 102,  // 0x66
    Low    = 40,   // 0x28
    Medium = 60,   // 0x3C
    High   = 80,   // 0x50
    Silent = 20,   // 0x14
};

// Текущее состояние кондера (распарсенное из ответа 0xC0)
struct State {
    bool     valid;        // true, если получили хотя бы один корректный ответ
    bool     power_on;
    Mode     mode;
    float    set_temp;     // °C, задание
    float    indoor_temp;  // °C, фактическая температура в помещении
    float    outdoor_temp; // °C, наружный блок (0 если не пришло)
    FanSpeed fan;
    bool     eco;
    bool     turbo;
    bool     sleep;
    bool     swing_v;
    bool     swing_h;
};

// Колбэк, вызывается из задачи RX при каждом успешно распарсенном статусе.
// Может вызываться часто — в нём только обновляйте атрибуты Matter, не блокируйте.
typedef void (*state_cb_t)(const State &state);
typedef void (*display_sound_cb_t)();

// Инициализация драйвера: настраивает UART и стартует фоновую задачу-опросчик.
// tx_gpio / rx_gpio — пины ESP, на которые приходит RX/TX кондера.
esp_err_t init(int tx_gpio, int rx_gpio, state_cb_t cb);
void set_display_sound_callback(display_sound_cb_t cb);

// Поточно-безопасное чтение последнего сохранённого состояния.
State get_state();

// Команды на запись — все они формируют полный set-кадр на основе текущей «тени»
// (последнего известного состояния) и отправляют его кондеру.
esp_err_t set_power(bool on);
esp_err_t set_mode(Mode mode);
esp_err_t set_target_temp(float celsius);   // 16.0 .. 30.0, шаг 0.5
esp_err_t set_fan(FanSpeed fan);
esp_err_t toggle_display();
esp_err_t set_turbo(bool on);
esp_err_t set_sleep(bool on);
esp_err_t set_preset(bool sleep, bool turbo);
esp_err_t set_swing_v(bool on);
esp_err_t set_swing_h(bool on);
esp_err_t set_swing(bool horizontal, bool vertical);

// Принудительно запросить статус «прямо сейчас» (вне расписания опроса).
esp_err_t request_status();

}  // namespace midea
