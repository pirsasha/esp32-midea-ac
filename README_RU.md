<div align="center">

<a href="https://pirogovx.ru">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/pirogovx-logo-horizontal-dark.webp">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/pirogovx-logo-horizontal.webp">
    <img src="docs/assets/pirogovx-logo-horizontal.webp" width="460" alt="PirogovX">
  </picture>
</a>

# PirogovX ESP32 AC Controller

Локальное управление кондиционерами через ESP32: UART, Home Assistant, Zigbee2MQTT, MQTT и Matter.

[English](README.md) | **Русский**

[![Web installer](https://img.shields.io/badge/Web%20installer-flash.pirogovx.ru-2563eb?style=for-the-badge)](https://flash.pirogovx.ru)
[![License](https://img.shields.io/badge/License-Apache%202.0-0f766e?style=for-the-badge)](LICENSE)
[![ESP32](https://img.shields.io/badge/ESP32-C3%20%7C%20C6%20%7C%20H2-e11d48?style=for-the-badge)](https://www.espressif.com/)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Local%20control-41BDF5?style=for-the-badge)](https://www.home-assistant.io/)

<br>

[![Telegram](https://img.shields.io/badge/Telegram-@pirogovc-26A5E4?style=flat-square&logo=telegram&logoColor=white)](https://t.me/pirogovc)
[![YouTube](https://img.shields.io/badge/YouTube-@pirogovx-FF0000?style=flat-square&logo=youtube&logoColor=white)](https://www.youtube.com/@pirogovx)
[![Instagram](https://img.shields.io/badge/Instagram-@pirogovx-E4405F?style=flat-square&logo=instagram&logoColor=white)](https://www.instagram.com/pirogovx/)

</div>

Локальный модуль управления кондиционерами Royal Clima / Midea OEM и совместимыми моделями через внутренний UART кондиционера. Заменяет штатный Wi-Fi модуль и даёт полное локальное управление.

Прошить модуль из браузера можно на сайте:

**[flash.pirogovx.ru](https://flash.pirogovx.ru)** — выбор платы и модели, прошивка в один клик, без установки программ.


> [!IMPORTANT]
> **GitHub-репозиторий не содержит все актуальные профили и универсальные прошивки PirogovX.**
>
> Исходники здесь в первую очередь служат открытой reference-реализацией для **Midea / Midea OEM UART**, а также содержат примеры Wi-Fi, Zigbee и Matter.
>
> Для самой широкой совместимости используйте **[flash.pirogovx.ru](https://flash.pirogovx.ru)**. Универсальная прошивка ESP32-C6 на сайте поддерживает автоматическое определение нескольких протоколов, включая Midea, TCL, Haier, Hisense RS-485, Gree и Samsung NASA F1/F2.
>
> Поддержка моделей и протоколов развивается быстрее, чем этот репозиторий, поэтому актуальную матрицу совместимости лучше проверять на сайте.

## Совместимость с первого взгляда

### Этот репозиторий: Midea UART / Midea OEM

<div align="center">

![Midea UART](https://img.shields.io/badge/Midea-UART%20protocol-2563eb?style=flat-square)
![Royal Clima](https://img.shields.io/badge/Royal%20Clima-проверено-16a34a?style=flat-square)
![Kentatsu](https://img.shields.io/badge/Kentatsu-проверено-16a34a?style=flat-square)
![Hommyn](https://img.shields.io/badge/Hommyn-Midea%20%2F%20Syncleo-16a34a?style=flat-square)
![Neoline](https://img.shields.io/badge/Neoline-проверено-16a34a?style=flat-square)

</div>

Открытая прошивка в этом репозитории ориентирована прежде всего на **Midea UART** и совместимые OEM-реализации. Точная совместимость всё равно зависит от электроники конкретного внутреннего блока и реализации UART.

### Универсальная прошивка на flash.pirogovx.ru

<div align="center">

[![Midea](https://img.shields.io/badge/Midea-поддержка-0f766e?style=flat-square)](https://flash.pirogovx.ru)
[![TCL](https://img.shields.io/badge/TCL-поддержка-0f766e?style=flat-square)](https://flash.pirogovx.ru)
[![Haier](https://img.shields.io/badge/Haier-SmartAir2%20%2F%20hOn-0f766e?style=flat-square)](https://flash.pirogovx.ru)
[![Hisense](https://img.shields.io/badge/Hisense-RS--485-0f766e?style=flat-square)](https://flash.pirogovx.ru)
[![Gree](https://img.shields.io/badge/Gree-поддержка-0f766e?style=flat-square)](https://flash.pirogovx.ru)
[![Samsung](https://img.shields.io/badge/Samsung-NASA%20F1%20%2F%20F2-0f766e?style=flat-square)](https://flash.pirogovx.ru)

</div>

Универсальная сборка ESP32-C6 на сайте умеет автоматически определять поддерживаемые семейства протоколов. **Набор функций может отличаться в зависимости от конкретной модели**, поэтому актуальную совместимость лучше проверять через веб-установщик.

## Что выбрать

| Задача | Куда идти |
|---|---|
| Изучить или изменить открытую реализацию Midea UART | **Этот GitHub-репозиторий** |
| Разрабатывать Zigbee2MQTT, ZHA, Matter или UART | **Этот GitHub-репозиторий** |
| Прошить ESP32 прямо из браузера | **[flash.pirogovx.ru](https://flash.pirogovx.ru)** |
| Нужна самая широкая текущая поддержка кондиционеров | **[Универсальная прошивка PirogovX](https://flash.pirogovx.ru)** |

## Быстрый старт

1. Откройте **[flash.pirogovx.ru](https://flash.pirogovx.ru)** в Chromium-браузере.
2. Подключите ESP32 по USB и выберите плату / платформу кондиционера.
3. Прошейте модуль, подключите **5V / GND / TX / RX** и настройте нужную интеграцию.

### Поддерживаемые платы ESP32

| Плата | Основное применение |
|---|---|
| **ESP32-C3** | Wi-Fi / MQTT |
| **ESP32-C6** | Wi-Fi, Zigbee, Matter, универсальные multi-protocol сборки |
| **ESP32-H2** | Zigbee Router / Zigbee2MQTT |

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

## Поддержать проект ❤️

Если PirogovX оказался полезен, можно поддержать дальнейшую разработку, покупку оборудования для тестов, исследование протоколов и добавление новых моделей кондиционеров.

### Поддержать в России

<div align="center">

[![CloudTips](https://img.shields.io/badge/Поддержать-CloudTips-7C3AED?style=for-the-badge&logo=heart&logoColor=white)](https://pay.cloudtips.ru/p/b9d5fc99)

</div>

### Криптовалюта

![USDT TRC20](https://img.shields.io/badge/USDT-TRON%20%2F%20TRC20-26A17B?style=flat-square)

```text
TU6h4ycD2cALZVTYidQxqigux5ce1swJv6
```

![USDT TON](https://img.shields.io/badge/USDT-TON-0098EA?style=flat-square)

```text
UQDhOx70Zg48VI2FzBc_3QvwdwOMXurkzgl8doFUgobpHDdZ
```

![Bitcoin](https://img.shields.io/badge/Bitcoin-BTC-F7931A?style=flat-square&logo=bitcoin&logoColor=white)

```text
1Q3koHNsypvvhrpRfSDxFZWAN3nwYRXuHt
```

> [!IMPORTANT]
> Перед отправкой обязательно проверьте **монету и сеть**. Криптовалютные транзакции необратимы. Эти адреса предназначены только для добровольной поддержки проекта.

Спасибо всем, кто помогает тестировать новое оборудование и развивать PirogovX. ❤️

## Лицензия

Основной код проекта распространяется по лицензии **Apache License 2.0**.

Это означает, что код можно использовать, изменять, распространять и применять
в коммерческих проектах, включая продажу готовых модулей и устройств, при
соблюдении условий лицензии и сохранении необходимых уведомлений об авторстве.

Название **PirogovX** и связанные с проектом названия/обозначения не передаются
по лицензии как торговая марка или бренд. Использовать их можно только в
обычном описательном смысле, например чтобы указать происхождение проекта.

Полный текст: [LICENSE](LICENSE)

Уведомления об авторстве и использованных open-source проектах:
[NOTICE](NOTICE) и [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

