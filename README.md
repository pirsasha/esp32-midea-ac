# Royal Clima / Midea AC on ESP32

Локальный модуль управления кондиционерами Royal Clima / Midea OEM и совместимыми моделями через внутренний UART кондиционера. Заменяет штатный Wi-Fi модуль и даёт полное локальное управление.

Прошить модуль из браузера можно на сайте:

**[flash.pirogovx.ru](https://flash.pirogovx.ru)** — выбор платы и модели, прошивка в один клик, без установки программ.

Проект содержит готовые варианты прошивок:

- **Wi-Fi ESP32-C6** — WQTT + Алиса, настройка через веб-портал, OTA.
- **Wi-Fi ESP32-C3** — WQTT + Алиса, настройка через веб-портал, OTA.
- **Zigbee ESP32-C6** — Zigbee2MQTT / Home Assistant, работает как **роутер**, OTA по воздуху, кнопка сброса.
- **Zigbee ESP32-H2** — Zigbee2MQTT / Home Assistant, работает как **роутер**, OTA по воздуху, кнопка сброса.
- **Matter ESP32-C6** — локальная Matter-интеграция.

Все варианты подключаются к UART кондиционера и управляют им напрямую, без родного USB Wi-Fi модуля.

## Что работает

- Включение и выключение.
- Режимы: auto, cool, heat, dry, fan only.
- Установка температуры (шаг 1 °C).
- Скорость вентилятора: auto, low, medium, high, quiet.
- Шторки: off, horizontal, vertical, both.
- Preset: none, sleep, turbo.
- Управление дисплеем и звуком (beep).
- Температура внутреннего блока.
- Температура наружного блока, если кондиционер отдаёт её в UART-статусе.

Wi-Fi версия дополнительно поддерживает:

- captive portal для первой настройки;
- WQTT token вместо ручного ввода MQTT;
- автоматическое создание устройства в WQTT;
- интеграцию с Алисой через WQTT;
- OTA и локальную веб-панель;
- выбор пинов TX/RX/порта из приложения.

Zigbee версия дополнительно:

- работает как **Zigbee Router** (устройство питается от кондиционера, всегда онлайн, ретранслирует сеть и надёжно принимает команды);
- **OTA-обновление по воздуху** через Zigbee2MQTT — без USB и разбора корпуса;
- **кнопка сброса**: удержание BOOT (GPIO9) 5 секунд возвращает устройство к заводскому состоянию (выход из сети, готовность к новому спариванию);
- телеметрия текущей и наружной температуры в Home Assistant.

## Совместимость

Прошивки рассчитаны на кондиционеры с **Midea UART protocol**. Это не только Midea, но и множество OEM-брендов на той же платформе.

Хорошие признаки совместимости:

- Родной модуль похож на **OSK102 / OSK103 / OSK104 / OSK105 / OSK302 / SK10x / SK11x**.
- В инструкции указано приложение **NetHome Plus**, **Midea Air**, **MSmartHome**, **Hommyn Home** или похожее Midea-приложение.
- Внутри кондиционера есть USB-A или 4-проводной UART-разъём для Wi-Fi модуля.

Проверенные модели:

- Royal Clima RCI-TWA22HN TRIUMPH.
- Kentatsu KSGYK35HZRN1 / KSRYK35HZRN1.
- Kentatsu KSGA26HZRN1.
- Hommyn (серии на Midea/Syncleo-платформе).
- Neoline NAM 07HN1.

Потенциально совместимые бренды и линейки:

- Royal Clima.
- Midea.
- Hommyn.
- Neoline.
- Kentatsu на Midea/OEM платформе.
- Comfee.
- Pioneer.
- Lessar, часть моделей.
- Marsalle, часть моделей.
- Electrolux, часть моделей.
- Carrier, часть моделей.
- Toshiba/Midea, часть моделей.
- Cooper&Hunter, часть моделей.
- Senville / MrCool / Klimaire, часть моделей.

Не подойдут напрямую кондиционеры на других протоколах: Gree / часть Ballu / часть TCL / часть Hisense, Haier, Daikin, Mitsubishi, Hitachi. Для них нужна отдельная реализация протокола.

> Старые модули (некоторые OSK103 / Royal Clima) отвечают только после 7-кратного нажатия кнопки дисплея и используют «legacy» 0x64-хендшейк — для них на сайте есть отдельный вариант прошивки.

## Подключение

Типовая распиновка:

```text
Кондиционер 5V   -> ESP 5V
Кондиционер GND  -> ESP GND
Кондиционер TX   -> ESP RX
Кондиционер RX   -> ESP TX
```

Если кондиционер не реагирует, но питание есть, сначала поменяйте местами только TX/RX.

Важно: ESP работает на 3.3V логике. У некоторых кондиционеров UART может быть 5V. Правильнее использовать level shifter хотя бы на линию **TX кондиционера -> RX ESP**.

### Wi-Fi ESP32-C6

```text
ESP GPIO6  = TX к кондиционеру RX
ESP GPIO7  = RX от кондиционера TX
UART       = 9600 baud
```

### Wi-Fi ESP32-C3

```text
ESP GPIO20 = TX к кондиционеру RX
ESP GPIO21 = RX от кондиционера TX
UART       = 9600 baud
```

В C3 версии консоль ESP-IDF перенесена на USB Serial/JTAG, а UART0 отключён, чтобы GPIO20/GPIO21 не спамили логами в линию кондиционера.

### Zigbee ESP32-C6

```text
ESP GPIO7  = TX к кондиционеру RX
ESP GPIO6  = RX от кондиционера TX
UART       = 9600 baud
```

### Zigbee ESP32-H2

```text
ESP GPIO5  = TX к кондиционеру RX
ESP GPIO8  = RX от кондиционера TX
UART       = 9600 baud
```

## Готовые прошивки

Готовые файлы лежат в папке `release/`. Проще всего прошивать через сайт:

**[flash.pirogovx.ru](https://flash.pirogovx.ru)**

Структура релизов:

```text
release/
  wifi-esp32c6/
  wifi-esp32c3/
  zigbee-esp32c6/
  zigbee-esp32h2/
  matter-esp32c6/
```

### Zigbee ESP32-C6

Папка: `release/zigbee-esp32c6/`

```powershell
esptool.py --chip esp32c6 -p COM9 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 2MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin 0x20000 zb_midea_ac.bin
```

### Zigbee ESP32-H2

Папка: `release/zigbee-esp32h2/`

```powershell
esptool.py --chip esp32h2 -p COM11 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 48m --flash_size 2MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin 0x20000 zb_midea_ac.bin
```

### Wi-Fi ESP32-C6

Папка: `release/wifi-esp32c6/`

```powershell
esptool.py --chip esp32c6 -p COM9 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin 0x20000 ac_wifi_module.bin
```

### Wi-Fi ESP32-C3

Папка: `release/wifi-esp32c3/`

```powershell
esptool.py --chip esp32c3 -p COM14 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin 0x20000 ac_wifi_module.bin
```

### Matter ESP32-C6

Папка: `release/matter-esp32c6/`

```powershell
esptool.py --chip esp32c6 -p COM9 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 ac_matter.bin
```

Замените `COMx` на свой порт.

## Первичная настройка Wi-Fi версии

1. Прошейте ESP32-C6 или ESP32-C3.
2. После первой загрузки плата поднимет Wi-Fi точку доступа.
3. Подключитесь к этой точке с телефона или компьютера.
4. Введите Wi-Fi сеть, пароль, WQTT token и имя кондиционера.
5. После сохранения модуль подключится к Wi-Fi, создаст устройство в WQTT и отправит MQTT state.
6. В Алисе устройство появляется через привязанный WQTT аккаунт.

Пользователь не должен вручную видеть MQTT broker, JSON, YAML или UART-настройки.

## Zigbee2MQTT

Устройство определяется как:

```text
PirogovX / ZB-MIDEA-AC
```

Одно определение покрывает обе платы — ESP32-C6 и ESP32-H2.

Поддержка отправлена в официальный репозиторий Zigbee2MQTT — после мержа устройство будет распознаваться **автоматически, без внешнего конвертера** и с фотографией в списке:

- конвертер: [zigbee-herdsman-converters #12918](https://github.com/Koenkk/zigbee-herdsman-converters/pull/12918)
- картинка: [zigbee2mqtt.io #5414](https://github.com/Koenkk/zigbee2mqtt.io/pull/5414)

До мержа используйте внешний конвертер:

```text
release/zigbee-esp32c6/esp-ac.js   (или release/zigbee-esp32h2/esp-ac.js — они идентичны)
```

Скопируйте его в папку external converters Zigbee2MQTT, например:

```text
/config/zigbee2mqtt/external_converters/esp-ac.js
```

Перезапустите Zigbee2MQTT и добавьте устройство заново (или нажмите reconfigure).

### Обновление по воздуху (Zigbee OTA)

Zigbee-прошивки поддерживают OTA через Zigbee2MQTT. Один раз добавьте в `configuration.yaml`:

```yaml
ota:
    zigbee_ota_override_index_location: https://flash.pirogovx.ru/firmware/ota-zigbee/index.json
```

Затем в Z2M: вкладка **OTA → Check for new updates → Update**. Устройство обновится и перезагрузится само, переспаривать не нужно.

## Home Assistant и Алиса

Wi-Fi версия идёт в Алису через WQTT:

```text
ESP32 -> Wi-Fi -> WQTT -> Алиса
```

Matter и Zigbee версии удобнее использовать через локальную инфраструктуру:

```text
ESP32-C6 Matter          -> Matter controller / Home Assistant
ESP32-C6 / H2 Zigbee     -> Zigbee2MQTT -> Home Assistant -> Yandex Smart Home
```

## Что пока не идеально

- Не все OEM-бренды одинаково реализуют swing/display/sound.
- Для новых моделей может потребоваться поправить Midea UART parser.
- Температура наружного блока публикуется только если кондиционер реально отдаёт её в статусе.
- В Matter/Home Assistant часть функций может отображаться отдельными сущностями.

## Безопасность

- Не подключайте ESP напрямую к 220V.
- Питайте ESP только от штатных 5V кондиционера или безопасного DC-источника.
- Перед подключением проверьте мультиметром 5V и GND.
- Не замыкайте TX/RX на питание.
- Если не уверены в уровнях UART, используйте level shifter.

## Структура проекта

```text
main/
  main.cpp            - Zigbee logic (ESP32-H2 / C6): router, OTA client, factory reset
  midea.cpp / midea.h - Midea UART protocol
  zb_signal_handler.c - Zigbee signal handling

zigbee2mqtt/
  esp-ac.js           - external converter for Zigbee2MQTT (PirogovX / ZB-MIDEA-AC)

release/
  wifi-esp32c6/  wifi-esp32c3/
  zigbee-esp32c6/  zigbee-esp32h2/   (+ ota/ с .ota-образами)
  matter-esp32c6/
```
