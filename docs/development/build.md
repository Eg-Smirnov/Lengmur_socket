# Сборка прошивки

Целевая плата — ESP32-WROOM-32, скетч находится в [`esp/esp.ino`](../../esp/esp.ino).
Профиль [`esp/sketch.yaml`](../../esp/sketch.yaml) закрепляет версии ESP32 core
и ArduinoJson. Для стандартного ESP32 Dev Module используется FQBN
`esp32:esp32:esp32`.

## Генерация конфигурации

После изменения [`config.json`](../../config.json) обновите сгенерированный
заголовок:

```powershell
python tools/generate_config.py
```

Подробнее: [конфигурация прошивки](../firmware/configuration.md).

## Компиляция

Из корня репозитория выполните:

```powershell
arduino-cli compile --profile esp32_wroom_32 esp
```

Команда только компилирует прошивку и не загружает её на плату.

> [!WARNING]
> Проверка проекта заканчивается компиляцией и анализом кода. Не запускайте
> `arduino-cli upload`, `arduino-cli compile --upload`, `esptool` для записи или
> стирания flash и другие команды загрузки прошивки. Полные правила находятся в
> корневом [`AGENTS.md`](../../AGENTS.md).
