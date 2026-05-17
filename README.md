# Royal Clima / Midea AC на ESP32: Matter и Zigbee

Локальный умный модуль для кондиционеров Royal Clima / Midea OEM без родного Wi-Fi USB-свистка и без облака.

Проект содержит два варианта:

- **Matter** для ESP32-C6 SuperMini.
- **Zigbee** для ESP32-H2 SuperMini через Zigbee2MQTT.

Оба варианта подключаются к внутреннему UART кондиционера и управляют им напрямую.

## Что работает

- Включение и выключение.
- Режимы: auto, cool, heat, dry, fan only.
- Установка температуры.
- Скорость вентилятора: auto, low, medium, high.
- Swing шторки: off, horizontal, vertical, both.
- Preset: none, sleep, turbo.
- Температура внутреннего блока.
- Температура наружного блока, если кондиционер ее отдает.
- Home Assistant.
- Zigbee2MQTT для ESP32-H2 версии.
- Matter pairing для ESP32-C6 версии.

## Совместимость

Прошивка рассчитана на кондиционеры с **Midea UART protocol**. Это может быть не только Midea, но и OEM-бренды на той же платформе.

Самые хорошие признаки совместимости:

- Родной модуль похож на **OSK102 / OSK103 / OSK104 / OSK105 / OSK302 / SK10x / SK11x**.
- В инструкции указано приложение **NetHome Plus**, **Midea Air**, **MSmartHome** или похожее Midea-приложение.
- Внутри кондиционера есть USB-A или 4-проводный UART-разъем для Wi-Fi модуля.

Потенциально совместимые бренды и линейки:

- Royal Clima.
- Midea.
- Comfee.
- Pioneer.
- Inventor.
- Lessar, часть моделей.
- Marsalle, часть моделей.
- Electrolux, часть моделей.
- Carrier, часть моделей.
- Toshiba/Midea, часть моделей.
- Cooper&Hunter, часть моделей.
- Senville / MrCool / Klimaire, часть моделей.

Не подойдут напрямую кондиционеры на других протоколах:

- Gree / Ballu / часть TCL / часть Hisense.
- Haier.
- Daikin.
- Mitsubishi.
- Hitachi.

Для них нужна другая реализация протокола.

## Подключение

Типовая распиновка:

```text
Кондиционер 5V   -> ESP 5V
Кондиционер GND  -> ESP GND
Кондиционер TX   -> ESP RX
Кондиционер RX   -> ESP TX
```

Для ESP32-H2 SuperMini в текущей Zigbee-прошивке:

```text
ESP GPIO5  = TX к кондиционеру RX
ESP GPIO8  = RX от кондиционера TX
```

Если кондиционер не реагирует, но питание есть, сначала поменяйте местами только TX/RX.

Важно: ESP работает на 3.3V логике. У некоторых кондиционеров UART может быть 5V. На практике у меня заработало напрямую, но правильнее ставить согласование уровней хотя бы на линию **TX кондиционера -> RX ESP**.

## Готовые прошивки

Готовые файлы лежат в папке:

```text
release/
```

### Zigbee ESP32-H2

Папка:

```text
release/zigbee-esp32h2/
```

Файлы:

```text
bootloader.bin
partition-table.bin
ota_data_initial.bin
zb_midea_ac.bin
esp-ac.js
flash_args
```

Команда прошивки через `esptool.py`:

```powershell
esptool.py --chip esp32h2 -p COM11 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 48m --flash_size 2MB 0x0 bootloader.bin 0x20000 zb_midea_ac.bin 0x8000 partition-table.bin 0xf000 ota_data_initial.bin
```

Замените `COM11` на свой порт.

### Matter ESP32-C6

Папка:

```text
release/matter-esp32c6/
```

Файлы:

```text
bootloader.bin
partition-table.bin
ac_matter.bin
flash_args
```

Команда прошивки через `esptool.py`:

```powershell
esptool.py --chip esp32c6 -p COM9 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader.bin 0x10000 ac_matter.bin 0x8000 partition-table.bin
```

Замените `COM9` на свой порт.

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

Если в Zigbee2MQTT осталось старое имя модели вроде `GasMeter1`, это обычно кэш/старое interview. Конвертер содержит fallback fingerprint для такого случая.

## Home Assistant

В HA устройство появляется как climate-сущность и набор дополнительных entities:

- Climate: режим, температура, текущее значение.
- Fan mode.
- Swing mode.
- Preset.
- Display.
- Outdoor temperature.
- Firmware version.

Часть параметров Zigbee2MQTT/HA не всегда красиво встраивает внутрь одной climate-карточки. Поэтому fan/swing/preset могут быть отдельными сущностями. Это нормально: управление работает, просто оформление отличается от родной интеграции Midea.

## Что пока не идеально

- Кнопка `Display` зависит от конкретной прошивки кондиционера. На некоторых моделях Midea/Royal Clima команда дисплея отличается.
- Не все OEM-бренды одинаково реализуют swing/display/sound.
- Для новых моделей может потребоваться поправить `midea.cpp`.
- Проект тестировался на Royal Clima / Midea UART, но не на всех возможных OEM-моделях.

## Алиса

Прямое добавление самодельного Zigbee-устройства в Алису зависит от того, умеет ли конкретный хаб и экосистема распознать нужный тип устройства.

Практичный вариант:

```text
ESP32-H2 Zigbee -> Zigbee2MQTT -> Home Assistant -> Yandex Smart Home
```

Так кондиционер можно пробросить в Алису как климатическое устройство через Home Assistant.

## Безопасность

- Не подключайте ESP напрямую к 220V.
- Питайте ESP только от штатных 5V кондиционера или от безопасного DC-источника.
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
  zigbee-esp32h2/
  matter-esp32c6/
```

