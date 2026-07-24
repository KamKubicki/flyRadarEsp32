#pragma once
// ============================================================================
// FlyRadar — operational configuration
//
// Edit this file to match your location and nearby reference airport.
// All operational thresholds are defined here; no need to touch .ino files.
// ============================================================================

// --- Location (set to your home coordinates) --------------------------------
// Example: Krakow area, Poland
#define HOME_LAT                50.0000     // <-- set your latitude
#define HOME_LON                20.0000     // <-- set your longitude

// Reference airport (closest to your home)
#define AIRPORT_LAT             50.0777     // EPKK Krakow-Balice
#define AIRPORT_LON             19.7848
#define AIRPORT_ICAO            "EPKK"

// --- Data sources -----------------------------------------------------------
#define ADSB_FI_BASE_URL        "https://opendata.adsb.fi/api/v3"
#define ADSB_FI_RADIUS_NM       20
#define ADSB_FI_USER_AGENT      "FlyRadar/1.0 (ESP32-S3)"
#define ADSB_FI_TIMEOUT_MS      10000

#define OPENWEATHER_BASE_URL    "https://api.openweathermap.org/data/3.0/onecall"
#define OPENWEATHER_TIMEOUT_MS  8000

// --- Flight filter thresholds -----------------------------------------------
#define MAX_HOME_DISTANCE_KM    25.0
#define MAX_AIRPORT_DISTANCE_KM 25.0
#define MAX_ALTITUDE_FT         7000
#define MIN_ALTITUDE_FT         0

// Runway headings (magnetic, degrees) — adjust for your airport
#define RWY_HEADING_07          72.0    // eastbound runway
#define RWY_HEADING_25          252.0   // westbound runway
#define COURSE_TOLERANCE_DEG    30.0

// Vertical rate thresholds (ft/min)
#define VR_DESCEND_LANDING_FPM  -200
#define VR_CLIMB_TAKEOFF_FPM     300

// --- Visibility sector (bearing from home) ----------------------------------
// Aircraft visible from the east-facing window: 20° (NNE) to 160° (SSE)
// Adjust to match the direction your window faces
#define EAST_SECTOR_MIN_DEG     20.0
#define EAST_SECTOR_MAX_DEG     160.0

// --- Refresh intervals (ms) -------------------------------------------------
#define ADSB_REFRESH_MS         12000
#define WEATHER_REFRESH_MS      900000    // 15 min
#define NTP_REFRESH_MS          21600000  // 6 h
#define WIFI_RECHECK_MS         5000
#define UI_RADAR_REDRAW_MS      1000

// --- Limits -----------------------------------------------------------------
#define MAX_AIRCRAFT            80
#define HTTP_RESPONSE_BUF_SIZE  2048

// --- Timezone ---------------------------------------------------------------
// POSIX TZ string — handles DST automatically
// Poland: CET-1CEST-2,M3.5.0/2,M10.5.0/3
#define NTP_TZ_STRING           "CET-1CEST-2,M3.5.0/2,M10.5.0/3"
#define NTP_SERVER_1            "pool.ntp.org"
#define NTP_SERVER_2            "time.nist.gov"

// --- Hardware pins (JC8048W550 display board) --------------------------------
#define TFT_BL                  2
#define I2C_SDA                 38
#define I2C_SCL                 8
#define TOUCH_INT               -1
#define TOUCH_RST               -1

// --- MicroSD card (SPI, confirmed by JC8048W550 schematic) ------------------
#define SD_CS_PIN               10
#define SD_MOSI_PIN             11
#define SD_SCK_PIN              12
#define SD_MISO_PIN             13
#define SD_AIRLINES_DIR         "/airlines"
#define SD_NAMEDAYS_FILE        "/namedays.csv"

// --- Backlight PWM ----------------------------------------------------------
#define BACKLIGHT_PWM_CHANNEL   0
#define BACKLIGHT_PWM_FREQ_HZ   4000    // 4 kHz — above audible range
#define BACKLIGHT_PWM_RES_BITS  8
#define BACKLIGHT_DEFAULT_PCT   80

// --- NVS (Preferences) keys -------------------------------------------------
#define NVS_NAMESPACE           "flyradar"
#define NVS_KEY_WIFI_SSID       "wifi_ssid"
#define NVS_KEY_WIFI_PASS       "wifi_pass"
#define NVS_KEY_OWM_KEY         "owm_key"
#define NVS_KEY_RADAR_KM        "radar_km"
#define NVS_KEY_SHOW_DEPS       "show_deps"
#define NVS_KEY_SHOW_ARR        "show_arr"
#define NVS_KEY_EAST_ONLY       "east_only"
#define NVS_KEY_IDLE_MIN        "idle_min"
#define NVS_KEY_BRIGHTNESS      "brightness"
#define NVS_KEY_ADSB_INTERVAL   "adsb_int"
