# Lengmur Socket

Lengmur Socket — устройство на ESP32-WROOM-32 для измерения напряжения и тока.
Сейчас ESP32 обменивается с компьютерным приложением через USB Serial со
скоростью 115200 бод. Поддержка Ethernet запланирована, но ещё не реализована.

## Быстрый старт

Установить зависимости и запустить клиентское приложение:

```powershell
python -m pip install -r laptop/requirements.txt
python laptop/app.py
```

Скомпилировать прошивку без загрузки на плату:

```powershell
arduino-cli compile --profile esp32_wroom_32 esp
```

## Документация

Оглавление и описание разделов находятся в [`docs/README.md`](docs/README.md).

- [Аппаратная часть](docs/hardware/overview.md)
- [Прошивка и драйверы](docs/firmware/configuration.md)
- [Serial-протокол](docs/protocols/serial.md)
- [Команды протокола](docs/protocols/commands.md)
- [Компьютерное приложение](docs/client/desktop-app.md)
- [Сборка и разработка](docs/development/build.md)

Текущие задачи перечислены в [`TODO.md`](TODO.md), а общие правила работы с
репозиторием — в [`AGENTS.md`](AGENTS.md).
