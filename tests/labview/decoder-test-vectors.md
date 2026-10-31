# Тест-векторы для `Decode Frames.vi`

Сгенерировано `generate_decoder_vectors.py` (Python 3, только стандартная библиотека).
Скрипт генерирует входные данные и ожидаемые результаты для запуска через `run_decoder_tests.py`.

## Принятый контракт в этих векторах

- CRC-32 — стандартный reflected CRC-32, trailer little-endian.
- Неполный заголовок/кадр — штатный остаток, не ошибка.
- Неверная CRC и payload length > 2048 — отбросить кандидата, продолжить поиск с `found index + 1`, не выставлять error cluster.
- Версия проверяется только после полного кадра и совпадения CRC. Версия не 1 выдаёт фатальную ошибку декодера: `5004`, `unsupported protocol version`; разбор этого вызова останавливается на таком кадре.
- При 5004 кадры после кандидата не декодируются. Выходные данные вызова с ошибкой не используются приложением.
- При уже установленном `error in` этот error cluster проходит без замены, данные не декодируются и сохраняются в `remaining bytes` как `pending + data`.

Для ручного прогона подавайте `data in` как бинарную строку (например, через `Byte Array To String`). Между вызовами передавайте `remaining bytes` предыдущего вызова во вход `pending bytes` следующего.

## Векторы

### T01_empty_input

Пустой вызов не выдаёт кадры или остаток.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
(empty)
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T02_noise_only

Мусор без сигнатуры отбрасывается.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
00 FF 00 7E 10
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T03_trailing_L

Одинокий первый байт сигнатуры сохраняется.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
00 FF 4C
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C`
- `error out`: `status=FALSE`, `code=0`

### T04_partial_signature

Неполная сигнатура LS сохраняется целиком.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
00 4C 53
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53`
- `error out`: `status=FALSE`, `code=0`

### T05_partial_header

Неполный заголовок сохраняется от сигнатуры.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09`
- `error out`: `status=FALSE`, `code=0`

### T06_valid_empty_payload

Кадр типа 0 с пустым payload.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 00 00 2E 88 5B 83
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=0, payload_hex=(empty)
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T07_valid_ascii_123456789

Контрольный непустой кадр типа 0.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T08_binary_payload

Payload содержит NUL и произвольные бинарные байты.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 41 06 00 00 4C 53 FF 00 39 AD B0 56 4F
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=2, payload_length=6, payload_hex=00 4C 53 FF 00 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T09_magic_inside_valid_payload

Сигнатура внутри валидного payload не начинает новый кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 41 10 00 70 72 65 66 69 78 4C 53 00 00 73
75 66 66 69 78 0A 96 D5 A1
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=2, payload_length=16, payload_hex=70 72 65 66 69 78 4C 53 00 00 73 75 66 66 69 78
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T10_reserved_type_7

Тип 7 декодируется как поле кадра; версия остаётся 1.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 E1 1E 00 72 65 73 65 72 76 65 64 2D 74 79
70 65 2D 69 73 2D 73 74 69 6C 6C 2D 61 2D 66 72
61 6D 65 36 F2 6E B8
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=7, payload_length=30, first_8=72 65 73 65 72 76 65 64, last_8=2D 61 2D 66 72 61 6D 65
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T11_two_frames_back_to_back

Два кадра без разделителя.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 00 00 2E 88 5B 83 4C 53 01 09 00 31 32
33 34 35 36 37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=0, payload_hex=(empty)
[1] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T12_noise_before_valid

Мусор перед корректной сигнатурой пропускается.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
00 99 4C 00 7F 4C 53 01 09 00 31 32 33 34 35 36
37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T13_overlap_and_oversize_then_valid

Ложный LS с длиной 2049 не блокирует следующий кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 4C 53 01 01 08 4C 53 01 09 00 31 32 33 34 35
36 37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T14_bad_crc_only

Кадр с неверной CRC отбрасывается без error cluster.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
A2 C2
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T15_bad_crc_then_valid

После неверной CRC декодер находит следующий кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
A2 C2 4C 53 01 09 00 31 32 33 34 35 36 37 38 39
C6 1B A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T16_bad_outer_crc_contains_valid_frame

При восстановлении после CRC ошибки находится вложенный валидный кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 1F 00 41 41 41 4C 53 21 10 00 69 6E 73
69 64 65 2D 62 61 64 2D 66 72 61 6D 65 59 A8 12
1A 5A 5A 5A 41 BE 54 A6
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=1, payload_length=16, payload_hex=69 6E 73 69 64 65 2D 62 61 64 2D 66 72 61 6D 65
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T17_valid_crc_unsupported_version

CRC проверяется до версии; валидный кадр версии 2 даёт фатальную ошибку 5004.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 02 0D 00 6F 74 68 65 72 2D 76 65 72 73 69
6F 6E 85 AB 0C EE
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `(empty)`
- `error out`: `status=TRUE`, `code=5004`, `source=unsupported protocol version`

### T18_unsupported_version_bad_crc

Неверная CRC не должна порождать 5004; следующий кадр декодируется.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 02 0D 00 6F 74 68 65 72 2D 76 65 72 73 69
6F 6E 85 AB 0C EF 4C 53 01 09 00 31 32 33 34 35
36 37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T19_unsupported_stops_before_later_valid_frame

После 5004 декодер останавливается; следующий корректный кадр не декодируется.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 02 0D 00 6F 74 68 65 72 2D 76 65 72 73 69
6F 6E 85 AB 0C EE 4C 53 01 09 00 31 32 33 34 35
36 37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `(empty)`
- `error out`: `status=TRUE`, `code=5004`, `source=unsupported protocol version`

### T20_valid_before_unsupported_stops_call

Кадр до версии 2 может быть найден, затем 5004 останавливает разбор до следующего кадра.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 00 00 2E 88 5B 83 4C 53 02 0D 00 6F 74
68 65 72 2D 76 65 72 73 69 6F 6E 85 AB 0C EE 4C
53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B A2
C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=0, payload_hex=(empty)
```
- `remaining bytes`: `(empty)`
- `error out`: `status=TRUE`, `code=5004`, `source=unsupported protocol version`

### T21_partial_unsupported_version

Неполный кадр версии 2 не выставляет 5004, пока не получен и проверен CRC.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 02 0D 00 6F 74 68 65 72 2D 76 65 72 73 69
6F 6E 85 AB 0C
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 02 0D 00 6F 74 68 65 72 2D 76 65 72 73 69
6F 6E 85 AB 0C`
- `error out`: `status=FALSE`, `code=0`

### T22_length_256_little_endian

Длина 256 кодируется байтами 00 01.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 41 00 01 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF 04 15 80 FC
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=2, payload_length=256, first_8=00 01 02 03 04 05 06 07, last_8=F8 F9 FA FB FC FD FE FF
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T23_max_payload_2048

Граничный допустимый payload размером 2048 байт.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 41 00 08 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF 00 01 02 03 04 05 06 07 08 09 0A
0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A
1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A
2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A
3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A
4B 4C 4D 4E 4F 50 51 52 53 54 55 56 57 58 59 5A
5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A
6B 6C 6D 6E 6F 70 71 72 73 74 75 76 77 78 79 7A
7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A
8B 8C 8D 8E 8F 90 91 92 93 94 95 96 97 98 99 9A
9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA
AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA
BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA
CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA
DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA
EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA
FB FC FD FE FF FA 5D CB C1
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=2, payload_length=2048, first_8=00 01 02 03 04 05 06 07, last_8=F8 F9 FA FB FC FD FE FF
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T24_oversize_2049_then_valid

Длина 2049 — помеха; ошибка не выставляется, следующий кадр декодируется.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 01 08 4C 53 01 09 00 31 32 33 34 35 36
37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T25_oversize_header_only

Один oversized-заголовок не создаёт 5003/5004 и не остаётся pending.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 01 08
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T26_incomplete_payload

Payload не пришёл целиком: весь кандидат остаётся в remaining bytes.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09 00 31 32 33 34`
- `error out`: `status=FALSE`, `code=0`

### T27_missing_crc

Payload полный, CRC ещё не пришла.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09 00 31 32 33 34 35 36 37 38 39`
- `error out`: `status=FALSE`, `code=0`

### T28_partial_crc_1_byte

В буфере только один байт CRC.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6`
- `error out`: `status=FALSE`, `code=0`

### T29_partial_crc_2_bytes

В буфере только два байта CRC.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B`
- `error out`: `status=FALSE`, `code=0`

### T30_partial_crc_3_bytes

В буфере только три байта CRC.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
A2
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
A2`
- `error out`: `status=FALSE`, `code=0`

### T31_split_after_first_magic_byte

Два последовательных вызова: первый возвращает хвост, второй завершает кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C`
- `error out`: `status=FALSE`, `code=0`

**Вызов 2 — вход**

- `pending bytes`: `4C`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B A2
C3
```

**Ожидается после вызова 2**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T32_split_after_signature

Два последовательных вызова: первый возвращает хвост, второй завершает кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53`
- `error out`: `status=FALSE`, `code=0`

**Вызов 2 — вход**

- `pending bytes`: `4C 53`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
01 09 00 31 32 33 34 35 36 37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 2**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T33_split_inside_header

Два последовательных вызова: первый возвращает хвост, второй завершает кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09`
- `error out`: `status=FALSE`, `code=0`

**Вызов 2 — вход**

- `pending bytes`: `4C 53 01 09`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
00 31 32 33 34 35 36 37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 2**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T34_split_inside_payload

Два последовательных вызова: первый возвращает хвост, второй завершает кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09 00 31 32 33 34 35`
- `error out`: `status=FALSE`, `code=0`

**Вызов 2 — вход**

- `pending bytes`: `4C 53 01 09 00 31 32 33 34 35`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
36 37 38 39 C6 1B A2 C3
```

**Ожидается после вызова 2**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T35_split_inside_crc

Два последовательных вызова: первый возвращает хвост, второй завершает кадр.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B`
- `error out`: `status=FALSE`, `code=0`

**Вызов 2 — вход**

- `pending bytes`: `4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
A2 C3
```

**Ожидается после вызова 2**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `(empty)`
- `error out`: `status=FALSE`, `code=0`

### T36_valid_frame_then_partial_magic

Полный кадр выдаётся, а завершающий 4C сохраняется до следующего чтения.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=FALSE`, `code=0`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
A2 C3 4C
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
[0] packet_type=0, payload_length=9, payload_hex=31 32 33 34 35 36 37 38 39
```
- `remaining bytes`: `4C`
- `error out`: `status=FALSE`, `code=0`

### T37_existing_error_in

Входная ошибка проходит без замены; байты сохраняются, декодирование не выполняется.

**Вызов 1 — вход**

- `pending bytes`: `(empty)`
- `error in`: `status=TRUE`, `code=12345`, `source=upstream test error`
- `data in`:

```text
4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
A2 C3
```

**Ожидается после вызова 1**

- `decoded frames`:

```text
(none)
```
- `remaining bytes`: `4C 53 01 09 00 31 32 33 34 35 36 37 38 39 C6 1B
A2 C3`
- `error out`: `status=TRUE`, `code=12345`, `source=upstream test error`
