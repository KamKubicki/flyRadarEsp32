HOW TO BUILD

1.  Skopiuj cały katalog FlyRadar/FlyRadar/ do katalogu szkiców Arduino IDE
    (lub otwórz w Arduino IDE: File → Open → wybierz FlyRadar/FlyRadar.ino).

2.  Zainstaluj wymagane biblioteki (przez Library Manager):
    - lvgl              (wersja ~8.3.x, np. 8.3.11)
    - Arduino_GFX       (z repozytorium moononournation)
    - ArduinoJson       (wersja 6.x)
    - TFT_eSPI          (opcjonalnie, jeśli nie używasz RGB; na RGB potrzebny
                         jest TFT_eSPI tylko dla TFT_BL — ale RGB panel używa
                         własnego sterownika z Arduino_GFX)
    - GT911 / gt911-arduino (dotyk; z paczki producenta)

    Biblioteki z paczki producenta (JC8048W550/Libraries/):
        - Arduino_GFX-master          → wrzuć do ~/Documents/Arduino/libraries/
        - Gt911-arduino-main          → to samo
        - lvgl/                      → lvgl ~8.3.3, wrzuć całość (albo zainstaluj
                                         przez Library Manager jako LVGL 8.3.x)

3.  Skonfiguruj lv_conf.h:
    - skopiuj lv_conf.h z Libraries/lvgl/ projektu albo z producenta
    - ustaw:
        #define LV_COLOR_DEPTH     16
        #define LV_TICK_CUSTOM     1
        #ifdef LV_CONF_SKIP
        #define LV_CONF_SKIP       1
        #endif
    - reszta domyślnie (LV_USE_CANVAS, LV_USE_CHART, LV_USE_SWITCH itp.)

4.  Edytuj config_secrets.h (nigdy nie commituj go do repo):
        #define WIFI_SSID           "TwojaNazwaSieci"
        #define WIFI_PASSWORD       "TwojeHaslo"
        #define OPENWEATHER_API_KEY ""

    Jeśli używasz Open-Meteo (bez klucza), OPENWEATHER_API_KEY może być pusty
    — weather_service.cpp nie będzie go używał (zostanie przerobiony na Open-Meteo).

5.  Wybierz płytkę:
    - ESP32S3 Dev Module (lub odpowiednik z 8 MB PSRAM, RGB panel 800×480).

    Ustawienia płytki:
        Upload Speed: 921600
        Flash Mode: QIO
        Flash Size: 16 MB (lub 8 MB)
        Partition Scheme: Huge App (lub No OTA / 8MB with spiffs)
        PSRAM: OPI PSRAM

6.  (Opcjonalnie) Dotyk — w hal_touch.cpp wstaw własną inicjalizację GT911
    z pliku touch.h producenta, jeśli chcesz testować dotyk. W MVP dotyk
    działa przez dummy read, więc UI jest widoczne, ale nie reaguje.

7.  Kompilacja (Ctrl+R / Sketch → Verify/Compile).

    Oczekiwany rozmiar: ~1.2–1.5 MB Flash, ~200–400 kB RAM (bez PSRAM).
    Jeśli używasz LVGL canvas na radar — ok. 800*480*2 = 768 kB dla bufora
    canvasowego. W projekcie używamy s_cbuf[480][800] co daje 768 kB — to
    wymaga PSRAM (MALLOC_CAP_SPIRAM). Jeśli nie masz PSRAM, zmniejsz rozdzielczość
    canvasa lub zamień radar na lv_chart.

    W obecnym kodzie canvas alokuje statycznie s_cbuf[480][800] — to ~768 kB
    w PSRAM. Na ESP32-S3 z 8 MB PSRAM to jest OK. Dla wersji bez PSRAM
    usuń s_cbuf i zastąp wykres punktowy LV chart lub zmniejsz do 400×240.

8.  Po wgraniu urządzenie uruchomi się, połączy z Wi‑Fi i zacznie pobierać dane
    z adsb.fi. Ekran główny pokazuje samolot od wschodu lub przechodzi do
    pogody jeśli nie ma ruchu przez >15 min.

    Przyciski na dole przełączają widoki: Lot / Radar / Pogoda / Ustawienia.

STRUCTURE (pliki w katalogu FlyRadar/FlyRadar):
    FlyRadar.ino              — główna pętla i scheduler
    config.h                  — współrzędne, progi, interwały
    config_secrets.h          — WiFi SSID/hasło, klucz OpenWeather (nie commituj)
    app_types.h               — struktury AircraftState, WeatherCurrent, FlightKind …
    app_state.h / .cpp        — centralny stan aplikacji + logika pogody/bezczynności
    geo_utils.h / .cpp        — Haversine, bearing, filtr sektora
    flight_filter.h / .cpp    — klasyfikacja operacji (start/lądowanie od wschodu/zachodu)
    flight_selector.h / .cpp  — punktowanie i wybór samolotu głównego
    hal_display.h / .cpp      — RGB panel, flush_cb, LVGL init
    hal_touch.h / .cpp        — dummy dotyk (placeholder)
    hal_backlight.h / .cpp    — sterowanie PWM podświetlenia
    wifi_manager.h / .cpp     — łączenie Wi‑Fi
    time_service.h / .cpp     — NTP
    http_client_helper.h / .cpp — prosty GET do JSON
    adsb_service.h / .cpp     — odczyt z adsb-fi i parsowanie
    weather_service.h / .cpp  — OpenWeather (do wymiany na Open-Meteo)
    ui_router.h / .cpp        — przełączanie ekranów
    ui_theme.h / .cpp         — stałe kolorystyczne
    ui_home.h / .cpp          — ekran główny: 1 samolot
    ui_radar.h / .cpp         — ekran radar: canvas z pozycjami
    ui_weather.h / .cpp      — ekran pogody
    ui_settings.h / .cpp      — ekran ustawień (suwaki, switche, dropdown)