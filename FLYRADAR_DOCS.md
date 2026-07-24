# FlyRadar — Dokumentacja projektu

> Stan na: **lipiec 2026**  
> Sprzęt: **ESP32-S3 + wyświetlacz 800×480 JC8048W550 (RGB panel)**  
> Środowisko: **Arduino IDE + ESP32 Arduino Core + LVGL 8.x**

---

## Opis projektu

FlyRadar to domowe urządzenie IoT wyświetlające w czasie rzeczywistym informacje o samolotach przelatujących nad Krakow area, Poland. Pokazuje loty podchodzące do EPKK Kraków-Balice, aktualną pogodę, fazę księżyca z ikonką, imieniny z karty SD, logo linii lotniczej oraz godzinę. Działa na dwóch rdzeniach ESP32-S3 — sieć na Core 0, interfejs LVGL na Core 1.

---

## Architektura systemu

```
Core 0 — networkTask (FreeRTOS)        Core 1 — loop() Arduino
─────────────────────────────────       ──────────────────────────
wifiEnsureConnected()                   lv_timer_handler()   (5ms)
timeServiceLoop()                       uiRouterRefresh()
adsbFetchNearest()   ← co 12s          Auto/Manual mode logic
flightSelectorUpdate()
weatherFetch()       ← co 15 min
flightRouteFetch()   ← na nowy callsign

       ↓↑ gState (AppState) — współdzielony struct
```

Brak mutexa — operacje na `gState` są krótkie; `bool`/`int` są atomowe na Xtensa.

---

## Struktura plików

```
FlyRadar/
├── FlyRadar.ino           ← entry point, setup/loop, networkTask (Core 0)
├── config.h               ← stałe (coords, progi, piny SD/I2C, NVS keys)
├── config_secrets.h       ← WiFi SSID/PASS (nie commitować!)
│
├── app_types.h            ← AircraftState, WeatherCurrent, FlightCandidate, RouteInfo...
├── app_state.h/.cpp       ← AppState singleton, NVS save/load (Preferences)
│
├── wifi_manager.h/.cpp    ← WiFi connect/reconnect
├── http_client_helper.h/.cpp ← HTTP GET wrapper
├── time_service.h/.cpp    ← NTP + TZ (configTzTime, DST automatyczny)
├── adsb_service.h/.cpp    ← opendata.adsb.fi → aircraft[]
├── weather_service.h/.cpp ← open-meteo → WeatherCurrent + sunrise/sunset
├── flight_filter.h/.cpp   ← klasyfikacja: Arrival/Departure/Transit
├── flight_selector.h/.cpp ← scoring, wybór najlepszego kandydata
├── flight_route.h/.cpp    ← api.adsbdb.com/v0 → IATA origin/dest (LRU cache 5×10min)
├── geo_utils.h/.cpp       ← Haversine, bearing, sector check
├── moon_nameday.h/.cpp    ← faza księżyca (algorytm Meeus)
├── sd_manager.h/.cpp      ← microSD: init SPI, BMP logo linii, BMP moon, imieniny CSV
│
├── hal_display.h/.cpp     ← Arduino_GFX + LVGL flush callback
├── hal_touch.h/.cpp       ← GT911 I2C → LVGL indev
├── hal_backlight.h/.cpp   ← LEDC PWM backlight
│
├── ui_theme.h/.cpp        ← dark color palette
├── ui_router.h/.cpp       ← screen switcher, status bar, nav bar
├── ui_weather.h/.cpp      ← ekran Pogoda (ambient, ticker rotacyjny)
├── ui_home.h/.cpp         ← ekran Lot (logo + szczegóły)
├── ui_radar.h/.cpp        ← ekran Radar (canvas PSRAM 580×430)
└── ui_settings.h/.cpp     ← ekran Ustawienia (NVS)

prepare_sd.py              ← skrypt Python: pobiera loga + moon BMP, generuje CSV
```

---

## Konfiguracja lokalizacji

Edytuj `config.h`:

```cpp
#define HOME_LAT    YOUR_LAT   // your home
#define HOME_LON    YOUR_LON
#define AIRPORT_LAT 50.0777    // EPKK
#define AIRPORT_LON 19.7848
#define AIRPORT_ICAO "EPKK"
```

---

## Ekrany UI

### Pogoda (domyślny / ambient) — Balanced layout

```
┌──────────────────────────────────────────────────────────────────┐
│ [status bar — zegar, WiFi, NTP]                       40px      │
├──────────────────┬────────┬─────────────────────────────────────┤
│  17:42  (30px)   │ [ico]  │  19°C  (30px)                       │
│  piątek 10 lip   │ 90×90  │  Czesc. zachmurzenie  (20px)        │
│  (18px)          │        │  ^04:42  v20:48        (16px)       │
├──────────────────┴────────┴──────── separator ──────────────────┤
│  WIATR │ WILGOTNOSC │  OPAD │  TEMPERATURA          74px tiles  │
│  3.2m/s│    65%     │ 0mm  │  12/19°                            │
├───────────────────────── separator ─────────────────────────────┤
│  Pn    │  Wt        │  Sr                       64px forecast   │
│  12..18│  10..15    │  9..14                                    │
├─────────────────────────────────────────────────────────────────┤
│ [moon] 3 dni do pelni          ← slot 0: ikonka + tekst         │
│        Imieniny: Sylwii        ← slot 1: tylko jeśli karta SD   │
│        Lot w poblizu: WZZ2705  ← slot 2: lot / cisza            │
└─────────────────────────────────────────────────────────────────┘
│ [nav bar — Lot | Radar | Pogoda | Ustaw]              50px      │
```

- Ticker rotuje co 6s: księżyc (ikonka BMP 40×40) → imieniny (jeśli SD) → lot/cisza
- Wschód/zachód słońca: `^HH:MM  vHH:MM` (ASCII, bez unicode)
- Ikona pogody: ręcznie rysowana 90×90 px (słońce, chmury, deszcz, śnieg, burza, mgła)

### Lot

```
┌──────────────────────────────────────────────────────────────────┐
│ [status bar]                                                     │
├──────────────────────────────────────────────────────────────────┤
│ [logo 120×50]        WZZ2705          [LADUJE]                  │
│                      BCN -> KRK                                 │
│                  Wizz Air • A21N • Widoczny od zachodu          │
├────────────────────────────────────────────────────────────────┤
│  WYSOKOSC │ PREDKOSC │  DYSTANS │  KURS        4 karty 92px   │
│   3350 ft │  174 kt  │  2.5 km  │  258°                        │
├────────────────────────────────────────────────────────────────┤
│    Rej. HA-LVE  •  Opada 896 ft/min  •  EPKK 0.9 km           │
└─────────────────────────────────────────────────────────────────┘
│ [nav bar]                                                       │
```

- Logo linii: BMP 120×50 z karty SD (`/airlines/WZZ.bmp`), ukryte gdy brak
- Trasa: IATA kody (np. `BCN -> KRK`) z `api.adsbdb.com/v0/callsign/`
- Odświeżany tylko przy zmianie `adsbVersion`

### Radar

Canvas 580×430 w PSRAM. Środek: (260, 215), promień siatki: 170px.

- 4 okręgi koncentryczne (skala dynamiczna = `radarRadiusKm` z ustawień)
- Trójkąty kierunkowe (zielony = wschód, szary = zachód, niebieski = wybrany)
- Etykiety: tylko callsign z semi-transparentnym tłem
- Czerwone kropki na krawędzi = samoloty poza zasięgiem
- Panel boczny: callsign, status, wys/prędkość/dystans

### Ustawienia (NVS — trwałe po resecie)

| Kontrolka | Klucz NVS | Zakres |
|---|---|---|
| Zasięg radaru | `radar_km` | 5–50 km |
| Pokaż starty | `show_deps` | bool |
| Pokaż lądowania | `show_arr` | bool |
| Tylko od wschodu | `east_only` | bool |
| Jasność | `brightness` | 0–100% |
| Czas bezczynności | `idle_min` | 1–60 min |

Każda zmiana zapisywana natychmiast przez `appSettingsSave()` → Arduino `Preferences`.

---

## API i źródła danych

| Źródło | URL | Dane | Koszt |
|---|---|---|---|
| ADS-B | `opendata.adsb.fi/api/v3/lat/.../lon/.../dist/NM` | Aircraft JSON | Darmowe |
| Pogoda | `api.open-meteo.com/v1/forecast` | Temp, wiatr, WMO, prognoza 3d, sunrise/sunset | Darmowe |
| Trasy lotów | `api.adsbdb.com/v0/callsign/CALLSIGN` | IATA origin/dest, nazwa lotniska | Darmowe, bez klucza |
| NTP | `pool.ntp.org`, `time.nist.gov` | Czas (SNTP) | Darmowe |
| Imieniny | `/namedays.csv` z karty SD | Polskie imieniny | Lokalnie |
| Faza księżyca | Algorytm Meeus w `moon_nameday.cpp` | Faza 0.0–1.0, dni do pełni | Offline |
| Loga linii | `github.com/Jxck-S/airline-logos` (skrypt) | PNG 90×90 transparent | Open source |
| Ikonki księżyca | Wikimedia Commons (skrypt) | PNG 240×240 | Public domain |

### Uwaga o jakości tras

`api.adsbdb.com` to dane **crowd-sourced** — mogą być błędne dla nieregularnych/charter lotów. Nie istnieje darmowe API bez klucza które daje 100% pewność. AirLabs ma live dane ale limit 1000 req/mies. (za mało przy poll co 12s).

### Strefa czasowa

```cpp
// config.h
#define NTP_TZ_STRING "CET-1CEST-2,M3.5.0/2,M10.5.0/3"
// Zimą: UTC+1 (CET), Latem: UTC+2 (CEST)
// DST: ostatnia niedziela marca o 2:00 → lato
//      ostatnia niedziela października o 3:00 → zima
```

Konfiguracja przez `configTzTime(NTP_TZ_STRING, ...)` — DST automatyczny.

---

## Karta microSD

### Piny (potwierdzone schematem JC8048W550-2)

| Sygnał | GPIO |
|---|---|
| TF_CS  | **10** |
| MOSI   | **11** |
| TF_CLK | **12** |
| MISO   | **13** |

Biblioteka: `SD.h` + `SPI.h`, 4 MHz SPI.

### Struktura katalogów

```
/
├── airlines/
│   ├── WZZ.bmp    ← Wizz Air        120×50 px, BMP 24-bit, ciemne tło
│   ├── LOT.bmp    ← LOT Polish
│   ├── RYR.bmp    ← Ryanair
│   ├── EZY.bmp    ← easyJet
│   ├── DLH.bmp    ← Lufthansa
│   └── ...        ← ICAO 3-literowy = pierwsze 3 znaki callsign
├── moon/
│   ├── 0.bmp      ← Nów             40×40 px, BMP 24-bit
│   ├── 1.bmp      ← Przybyw. sierp
│   ├── 2.bmp      ← Pierwsza kwadra
│   ├── 3.bmp      ← Przybyw. garb
│   ├── 4.bmp      ← Pełnia
│   ├── 5.bmp      ← Ubyw. garb
│   ├── 6.bmp      ← Ostatnia kwadra
│   └── 7.bmp      ← Ubyw. sierp
└── namedays.csv   ← format: MM,DD,Imiona
```

### Przygotowanie karty SD (skrypt Python)

```bash
cd /Users/kamilkubicki/FlyRadar
source .venv/bin/activate
pip install requests Pillow
python prepare_sd.py              # tworzy ./sd_card/
python prepare_sd.py --output /Volumes/NazwaKarty   # bezpośrednio na kartę
```

Skrypt pobiera loga z **`github.com/Jxck-S/airline-logos`** (FlightAware PNG, 90×90, transparent) i ikonki księżyca z Wikimedia Commons. Nie wymaga cairosvg ani brew cairo.

### Logika fallback

```
sdInit() w setup()
    ├── sukces → sdAvailable() = true
    │   ├── sdLoadAirlineLogo("WZZ", buf) → /airlines/WZZ.bmp → canvas widoczny
    │   ├── sdLoadMoonIcon(4, buf)         → /moon/4.bmp → ikonka pełni
    │   └── sdNameday(7, 10)              → /namedays.csv → "Amelii"
    └── błąd → sdAvailable() = false
        ├── logo:    canvas ukryty (LV_OBJ_FLAG_HIDDEN)
        ├── księżyc: canvas ukryty, slot 0 pomijany w tickerze
        └── imieniny: "" → slot 1 pomijany w tickerze
```

---

## Logika wyboru samolotu

### Filtry (`flight_filter.cpp`)

| Warunek | Próg |
|---|---|
| Dystans od domu | ≤ 25 km |
| Dystans od lotniska | ≤ 25 km |
| Wysokość | 0–7000 ft |
| Kierunek na pas | ±30° od RWY 07 (72°) lub RWY 25 (252°) |
| Sektor wschodni | Bearing 20°–160° od domu |

### Scoring (`flight_selector.cpp`)

```
+40  samolot w sektorze wschodnim
+30  Arrival (ląduje)
+25  Departure (startuje)
+20  bliskość domu (proporcjonalnie do dystansu)
+10  niska wysokość
-30  Transit (przelot bez związku z EPKK)
-50  samolot po zachodniej stronie
```

---

## Konfiguracja LVGL (`lv_conf.h`)

**Lokalizacja:** `/Users/kamilkubicki/Documents/Arduino/libraries/lv_conf.h`

### Włączone fonty

`montserrat_12`, `montserrat_14` (LV_FONT_DEFAULT), `montserrat_16`, `montserrat_18`, `montserrat_20`, `montserrat_30`

### Włączone widgety

`LV_USE_ARC`, `LV_USE_BAR`, `LV_USE_BTN`, `LV_USE_CANVAS`, `LV_USE_IMG`, `LV_USE_LABEL`, `LV_USE_LINE`, `LV_USE_SLIDER`, `LV_USE_SWITCH`, `LV_USE_THEME_DEFAULT`

### Wyłączone (redukcja flash)

`LV_USE_FLEX`, `LV_USE_GRID`, `LV_USE_SJPG`, `LV_BUILD_EXAMPLES`

> ⚠️ `LV_BUILD_EXAMPLES 0` oszczędza ~30KB flash — kluczowe dla zmieszczenia w 1.25MB

---

## Piny sprzętowe

| Sygnał | GPIO |
|---|---|
| TFT Backlight | 2 |
| I2C SDA (config.h, nieużywany) | 38 |
| I2C SCL (config.h, nieużywany) | 8 |
| Touch SDA (hal_touch.cpp) | 19 |
| Touch SCL (hal_touch.cpp) | 20 |
| SD CS | 10 |
| SD MOSI | 11 |
| SD CLK | 12 |
| SD MISO | 13 |

> ⚠️ Touch używa pinów 19/20 hardcoded w `hal_touch.cpp` — niezgodność z `config.h` (38/8). Dwie osobne szyny I2C.

---

## Zależności bibliotek (Arduino)

| Biblioteka | Zastosowanie |
|---|---|
| `lvgl` 8.x | UI rendering |
| `Arduino_GFX_Library` | Sterownik RGB panelu |
| `TAMC_GT911` | Kontroler dotykowy GT911 |
| `ArduinoJson` 6.x | Parsowanie JSON |
| `SD` + `SPI` | Karta microSD (wbudowane) |
| `Preferences` | NVS persistent storage (wbudowana) |
| `HTTPClient` | HTTP GET (wbudowana) |
| `WiFi` | WiFi STA (wbudowana) |

---

## Znane ograniczenia i TODO

| # | Problem | Status |
|---|---|---|
| 1 | `config_secrets.h` zawiera hasło WiFi w kodzie | Do przeniesienia do NVS |
| 2 | Brak HTTPS weryfikacji certyfikatów (`setInsecure()`) | Akceptowalne dla home use |
| 3 | Race condition na `gState` między Core 0/1 (brak mutexa) | Akceptowalne (atomowe odczyty) |
| 4 | Touch hardcoded piny 19/20 vs config.h 38/8 | Działa, ale niespójne |
| 5 | `api.adsbdb.com` crowd-sourced — trasy czasem błędne | Brak lepszej darmowej alternatywy |
| 6 | Flash usage ~95-100% | `LV_BUILD_EXAMPLES 0` rozwiązuje, mało miejsca na nowe funkcje |
| 7 | Radar canvas redraw blokuje UI przez ~200ms | Akceptowalne przy 15s interwale |

---

## Odtworzenie projektu od zera

### 1. Środowisko

1. Arduino IDE 2.x
2. ESP32 Arduino Core: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Board: `ESP32S3 Dev Module`, Flash: `16MB`, PSRAM: `OPI PSRAM`

### 2. Biblioteki (Library Manager)

```
lvgl                 @ 8.3.x
Arduino_GFX_Library  @ latest
TAMC_GT911           @ latest
ArduinoJson          @ 6.x
```

### 3. lv_conf.h

Skopiuj do `/Documents/Arduino/libraries/lv_conf.h` i ustaw:

```c
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1   // LV_FONT_DEFAULT
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_30 1

#define LV_USE_FLEX 0
#define LV_USE_GRID 0
#define LV_USE_SJPG 0
#define LV_BUILD_EXAMPLES 0   // KRYTYCZNE — ~30KB flash
```

### 4. config_secrets.h

```cpp
#pragma once
#define WIFI_SSID        "TwojaSiec"
#define WIFI_PASSWORD    "TwojeHaslo"
#define OPENWEATHER_API_KEY "REPLACE_ME"
```

### 5. Karta SD

```bash
cd /Users/kamilkubicki/FlyRadar
source .venv/bin/activate
python prepare_sd.py --output /Volumes/NazwaKarty
```

### 6. Kompilacja

- Board: `ESP32S3 Dev Module`
- Partition Scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`
- Upload Speed: `921600`

---

## Changelog (sesja lipiec 2026)

| Zmiana |
|---|
| HTTP na Core 0 (FreeRTOS `xTaskCreatePinnedToCore`) — UI nigdy nie blokuje |
| `uiRouterShow` — fix: `lv_scr_load` 200×/s → early return gdy już na ekranie |
| WiFi/NTP ikony — change-guard (eliminacja ciągłej invalidacji LVGL) |
| `uiHomeRefresh`, `uiWeatherRefresh` — change-guard, update tylko przy nowych danych |
| `getLocalTime(500ms)` → `getLocalTime(0)` — eliminacja 500ms bloku w pętli |
| `appSettingsSave/Load` — Arduino Preferences (NVS), wszystkie ustawienia trwałe |
| Czas: `configTime` → `configTzTime(NTP_TZ_STRING)` — poprawka DST Polska |
| `flight_route.cpp` — poprawka URL: `adsbdb.com/api/v2` → `api.adsbdb.com/v0` |
| `flight_route.cpp` — poprawka kluczy JSON: `iata_code` zamiast `icao` |
| `ui_home.cpp` — trasa `BCN -> KRK` zamiast `LFPO -> EPKK` (IATA priorytet) |
| `ui_home.cpp` — canvas 120×50 z logo linii lotniczej z karty SD |
| `moon_nameday.cpp` — usunięta wbudowana tablica imienin (tylko SD) |
| `sd_manager.cpp` — microSD SPI (GPIO 10-13): loga linii, ikonki księżyca, imieniny |
| `ui_weather.cpp` — ikonki księżyca BMP 40×40 z SD zamiast tekstu unicode |
| `ui_weather.cpp` — wschód/zachód: `^HH:MM vHH:MM` (ASCII, bez unicode strzałek) |
| `ui_weather.cpp` — Balanced layout: ticker 3-slotowy, ikonka pogody 90×90 |
| `weather_service.cpp` — sunrise/sunset z open-meteo daily |
| `prepare_sd.py` — skrypt pobierający loga z `Jxck-S/airline-logos` + moon z Wikimedia |
| `LV_BUILD_EXAMPLES 0` — zaoszczędzone ~30KB flash |
| Radar — callsign-only tagi, usunięty wektor prędkości, NaN guards |
| Radar — skalowanie dynamiczne wg `radarRadiusKm` z ustawień |
| `s_iconBuf` (weather) → PSRAM, `static` na mapie linii w flight_selector |
