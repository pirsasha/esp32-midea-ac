# Royal Clima / Midea AC on ESP32

Локальный модуль управления кондиционерами Royal Clima / Midea OEM и совместимыми моделями через внутренний UART кондиционера.

Прошить модуль из браузера можно на сайте:

[flash.pirogovx.ru](https://flash.pirogovx.ru)

Проект содержит готовые варианты прошивок:

- **Wi-Fi ESP32-C6**: WQTT + Алиса, настройка через веб-портал.
- **Wi-Fi ESP32-C3**: WQTT + Алиса, настройка через веб-портал.
- **Matter ESP32-C6**: локальная Matter-интеграция.
- **Zigbee ESP32-H2**: интеграция через Zigbee2MQTT.

Все варианты подключаются к UART кондиционера и управляют им напрямую без родного USB Wi-Fi модуля.

## Что работает

- Включение и выключение.
- Режимы: auto, cool, heat, dry, fan only.
- Установка температуры.
- Скорость вентилятора: auto, low, medium, high.
- Шторки: stationary, horizontal, vertical.
- Preset/program: quiet, turbo.
- Температура внутреннего блока.
- Температура наружного блока, если кондиционер отдает ее в UART-статусе.

Wi-Fi версия дополнительно поддерживает:

- captive portal для первой настройки;
- WQTT token вместо ручного ввода MQTT;
- автоматическое создание устройства в WQTT;
- интеграцию с Алисой через WQTT;
- OTA и локальную веб-панель.

## Совместимость

Прошивки рассчитаны на кондиционеры с **Midea UART protocol**. Это не только Midea, но и OEM-бренды на той же платформе.

Хорошие признаки совместимости:

- Родной модуль похож на **OSK102 / OSK103 / OSK104 / OSK105 / OSK302 / SK10x / SK11x**.
- В инструкции указано приложение **NetHome Plus**, **Midea Air**, **MSmartHome** или похожее Midea-приложение.
- Внутри кондиционера есть USB-A или 4-проводной UART-разъем для Wi-Fi модуля.

Проверенные модели:

- Royal Clima RCI-TWA22HN TRIUMPH.
- Kentatsu KSGYK35HZRN1 / KSRYK35HZRN1.
- Kentatsu KSGA26HZRN1.

Потенциально совместимые бренды и линейки:

- Royal Clima.
- Midea.
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

Текущая Wi-Fi прошивка для ESP32-C6 SuperMini:

```text
ESP GPIO6  = TX к кондиционеру RX
ESP GPIO7  = RX от кондиционера TX
UART       = 9600 baud
```

### Wi-Fi ESP32-C3

Текущая Wi-Fi прошивка для ESP32-C3:

```text
ESP GPIO20 = TX к кондиционеру RX
ESP GPIO21 = RX от кондиционера TX
UART       = 9600 baud
```

В C3 версии консоль ESP-IDF перенесена на USB Serial/JTAG, а UART0 отключен, чтобы GPIO20/GPIO21 не спамили логами в линию кондиционера.

### Zigbee ESP32-H2

Текущая Zigbee прошивка для ESP32-H2 SuperMini:

```text
ESP GPIO5  = TX к кондиционеру RX
ESP GPIO8  = RX от кондиционера TX
UART       = 9600 baud
```

## Готовые прошивки

Готовые файлы лежат в папке:

```text
release/
```

Также можно прошивать через сайт:

[flash.pirogovx.ru](https://flash.pirogovx.ru)

### Wi-Fi ESP32-C6

Папка:

```text
release/wifi-esp32c6/
```

Файлы:

```text
bootloader.bin
partition-table.bin
ota_data_initial.bin
ac_wifi_module.bin
manifest-royal-clima-midea.json
build-info.json
flash_args
```

Команда прошивки через `esptool.py`:

```powershell
esptool.py --chip esp32c6 -p COM9 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin 0x20000 ac_wifi_module.bin
```

Замените `COM9` на свой порт.

### Wi-Fi ESP32-C3

Папка:

```text
release/wifi-esp32c3/
```

Файлы:

```text
bootloader.bin
partition-table.bin
ota_data_initial.bin
ac_wifi_module.bin
manifest-royal-clima-midea.json
build-info.json
flash_args
```

Команда прошивки через `esptool.py`:

```powershell
esptool.py --chip esp32c3 -p COM14 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin 0x20000 ac_wifi_module.bin
```

Замените `COM14` на свой порт.

### Matter ESP32-C6

Папка:

```text
release/matter-esp32c6/
```

Команда прошивки через `esptool.py`:

```powershell
esptool.py --chip esp32c6 -p COM9 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader.bin 0x10000 ac_matter.bin 0x8000 partition-table.bin
```

Замените `COM9` на свой порт.

### Zigbee ESP32-H2

Папка:

```text
release/zigbee-esp32h2/
```

Команда прошивки через `esptool.py`:

```powershell
esptool.py --chip esp32h2 -p COM11 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 48m --flash_size 2MB 0x0 bootloader.bin 0x20000 zb_midea_ac.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin
```

Замените `COM11` на свой порт.

## Первичная настройка Wi-Fi версии

1. Прошейте ESP32-C6 или ESP32-C3.
2. После первой загрузки плата поднимет Wi-Fi точку доступа.
3. Подключитесь к этой точке с телефона или компьютера.
4. Введите Wi-Fi сеть, пароль, WQTT token и имя кондиционера.
5. После сохранения модуль подключится к Wi-Fi, создаст устройство в WQTT и отправит MQTT state.
6. В Алисе устройство появляется через привязанный WQTT аккаунт.

Пользователь не должен вручную видеть MQTT broker, JSON, YAML или UART-настройки.

## Zigbee2MQTT

Для Zigbee-версии нужен внешний конвертер:

```text
release/zigbee-esp32h2/esp-ac.js
```

Скопируйте его в папку external converters Zigbee2MQTT, например:

```text
/config/zigbee2mqtt/external_converters/esp-ac.js
```

После этого перезапустите Zigbee2MQTT и добавьте устройство заново или нажмите reconfigure.

Устройство может определиться как:

```text
ZB-MIDEA-AC
```

Если в Zigbee2MQTT осталось старое имя модели вроде `GasMeter1`, это обычно кэш или старое interview.

## Home Assistant и Алиса

Wi-Fi версия идет в Алису через WQTT:

```text
ESP32 -> Wi-Fi -> WQTT -> Алиса
```

Matter и Zigbee версии удобнее использовать через локальную инфраструктуру:

```text
ESP32-C6 Matter -> Matter controller / Home Assistant
ESP32-H2 Zigbee -> Zigbee2MQTT -> Home Assistant -> Yandex Smart Home
```

## Что пока не идеально

- Не все OEM-бренды одинаково реализуют swing/display/sound.
- Для новых моделей может потребоваться поправить Midea UART parser.
- Температура наружного блока публикуется только если кондиционер реально отдает ее в статусе.
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
  main.cpp       - Zigbee logic for ESP32-H2
  midea.cpp      - Midea UART protocol
  midea.h

zigbee2mqtt/
  esp-ac.js      - external converter for Zigbee2MQTT

release/
  wifi-esp32c6/
  wifi-esp32c3/
  matter-esp32c6/
  zigbee-esp32h2/
```
