# Lengmur Socket

Lengmur Socket — устройство на ESP32-WROOM-32 для измерения напряжения и тока.
Прошивка обменивается с компьютером через USB Serial со скоростью 115200 бод.
Ранее использовавшееся Python-приложение больше не поддерживается; работа над
клиентом на LabVIEW ведётся отдельно. Поддержка Ethernet запланирована, но ещё
не реализована.

## Быстрый старт

Скомпилировать прошивку без загрузки на плату:

```powershell
arduino-cli compile --profile esp32_wroom_32 esp
```

> [!NOTE]
> Исходный код неподдерживаемого Python-клиента сохранён в `laptop/python/`.
> Не используйте его как актуальное приложение.

Перед работой с LabVIEW ознакомьтесь с [`laptop/labview/README.md`](laptop/labview/README.md).

## Документация

Оглавление и описание разделов находятся в [`docs/README.md`](docs/README.md).

- [Аппаратная часть](docs/hardware/overview.md)
- [Архитектура прошивки](docs/firmware/architecture.md)
- [Прошивка и драйверы](docs/firmware/configuration.md)
- [Serial-протокол](docs/protocols/serial.md)
- [Команды протокола](docs/protocols/commands.md)
- [Компьютерное приложение](docs/client/desktop-app.md)
- [Сборка и разработка](docs/development/build.md)

Текущие задачи перечислены в [`TODO.md`](TODO.md), а общие правила работы с
репозиторием — в [`AGENTS.md`](AGENTS.md).
