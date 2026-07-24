#include "sd_manager.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

static bool s_sdOk = false;

// ============================================================================
// sdInit
// ============================================================================
bool sdInit() {
    if (s_sdOk) return true;
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, SPI, 4000000)) {
        Serial.println("[SD] Mount FAILED");
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        Serial.println("[SD] Brak karty");
        return false;
    }
    Serial.printf("[SD] OK — %llu MB\n", SD.cardSize() / (1024 * 1024));
    s_sdOk = true;
    return true;
}

bool sdAvailable() { return s_sdOk; }

// ============================================================================
// Wspólna funkcja ładowania BMP (24-bit lub 16-bit RGB565)
// expectedW/H = oczekiwane wymiary, -1 = nie sprawdzaj
// ============================================================================
static bool loadBmp(const char *path, lv_color_t *buf, int expectedW, int expectedH) {
    File f = SD.open(path, FILE_READ);
    if (!f) { Serial.printf("[SD] Brak pliku: %s\n", path); return false; }

    uint8_t hdr[54];
    if (f.read(hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        f.close(); return false;
    }

    uint32_t dataOffset = hdr[10]|(hdr[11]<<8)|(hdr[12]<<16)|(hdr[13]<<24);
    int32_t  bmpW       = hdr[18]|(hdr[19]<<8)|(hdr[20]<<16)|(hdr[21]<<24);
    int32_t  bmpH       = (int32_t)(hdr[22]|(hdr[23]<<8)|(hdr[24]<<16)|(hdr[25]<<24));
    uint16_t bpp        = hdr[28]|(hdr[29]<<8);
    uint32_t compress   = hdr[30]|(hdr[31]<<8)|(hdr[32]<<16)|(hdr[33]<<24);

    bool flipV = (bmpH > 0);
    int  absH  = flipV ? bmpH : -bmpH;

    if ((expectedW > 0 && bmpW != expectedW) || (expectedH > 0 && absH != expectedH)) {
        Serial.printf("[SD] %s — zly rozmiar %dx%d (oczekiwano %dx%d)\n",
                      path, bmpW, absH, expectedW, expectedH);
        f.close(); return false;
    }

    int W = bmpW, H = absH;
    f.seek(dataOffset);

    if (bpp == 24 && compress == 0) {
        int rowBytes = ((W * 3 + 3) / 4) * 4;
        uint8_t rowBuf[rowBytes];
        for (int row = 0; row < H; ++row) {
            int dst = flipV ? (H - 1 - row) : row;
            f.read(rowBuf, rowBytes);
            for (int col = 0; col < W; ++col)
                buf[dst * W + col] = lv_color_make(
                    rowBuf[col*3+2], rowBuf[col*3+1], rowBuf[col*3]);
        }
    } else if (bpp == 16 && compress == 3) {
        int rowBytes = ((W * 2 + 3) / 4) * 4;
        uint8_t rowBuf[rowBytes];
        for (int row = 0; row < H; ++row) {
            int dst = flipV ? (H - 1 - row) : row;
            f.read(rowBuf, rowBytes);
            for (int col = 0; col < W; ++col) {
                uint16_t px = rowBuf[col*2] | (rowBuf[col*2+1] << 8);
                buf[dst * W + col] = lv_color_make(
                    (px>>11)<<3, ((px>>5)&0x3F)<<2, (px&0x1F)<<3);
            }
        }
    } else {
        Serial.printf("[SD] %s — nieobslugiwany BMP bpp=%d compress=%u\n", path, bpp, compress);
        f.close(); return false;
    }

    f.close();
    Serial.printf("[SD] %s OK\n", path);
    return true;
}

// ============================================================================
// sdLoadAirlineLogo — /airlines/WZZ.bmp (120×50)
// ============================================================================
bool sdLoadAirlineLogo(const char *prefix, lv_color_t *buf) {
    if (!s_sdOk || !prefix || !buf) return false;
    char up[4] = {};
    for (int i = 0; i < 3 && prefix[i]; ++i)
        up[i] = (prefix[i] >= 'a' && prefix[i] <= 'z') ? prefix[i]-32 : prefix[i];
    char path[32];
    snprintf(path, sizeof(path), "/airlines/%s.bmp", up);
    return loadBmp(path, buf, LOGO_W, LOGO_H);
}

// ============================================================================
// sdLoadMoonIcon — /moon/0.bmp .. /moon/7.bmp (40×40)
// phase0to7: 0=nów 1=przybyw.serp 2=pierwsza kwadra 3=przybyw.garb
//            4=pełnia 5=ubyw.garb 6=ostatnia kwadra 7=ubyw.serp
// ============================================================================
bool sdLoadMoonIcon(int phase0to7, lv_color_t *buf) {
    if (!s_sdOk || !buf) return false;
    if (phase0to7 < 0 || phase0to7 > 7) return false;
    char path[20];
    snprintf(path, sizeof(path), "/moon/%d.bmp", phase0to7);
    return loadBmp(path, buf, MOON_W, MOON_H);
}

// ============================================================================
// sdNameday — /namedays.csv  (MM,DD,Imiona)
// ============================================================================
static char **s_ndTable  = nullptr;
static bool   s_ndLoaded = false;

static void loadNamedays() {
    s_ndLoaded = true;
    s_ndTable = (char **)heap_caps_calloc(12 * 31, sizeof(char *), MALLOC_CAP_SPIRAM);
    if (!s_ndTable) { Serial.println("[SD] Namedays: brak RAM"); return; }

    File f = SD.open("/namedays.csv", FILE_READ);
    if (!f) { Serial.println("[SD] Brak /namedays.csv"); return; }

    int n = 0;
    while (f.available()) {
        char line[64]; int len = 0;
        while (f.available() && len < 63) {
            char c = f.read();
            if (c == '\n') break;
            if (c != '\r') line[len++] = c;
        }
        line[len] = '\0';
        if (len < 6 || line[2] != ',' || line[5] != ',') continue;
        int mo = (line[0]-'0')*10 + (line[1]-'0');
        int da = (line[3]-'0')*10 + (line[4]-'0');
        if (mo < 1 || mo > 12 || da < 1 || da > 31) continue;
        const char *names = line + 6;
        int nlen = len - 6;
        if (nlen <= 0) continue;
        int idx = (mo-1)*31 + (da-1);
        if (s_ndTable[idx]) free(s_ndTable[idx]);
        s_ndTable[idx] = (char *)heap_caps_malloc(nlen + 1, MALLOC_CAP_SPIRAM);
        if (s_ndTable[idx]) {
            memcpy(s_ndTable[idx], names, nlen);
            s_ndTable[idx][nlen] = '\0';
            n++;
        }
    }
    f.close();
    Serial.printf("[SD] Namedays: %d wpisow\n", n);
}

const char *sdNameday(int month, int day) {
    if (!s_sdOk || month < 1 || month > 12 || day < 1 || day > 31) return "";
    if (!s_ndLoaded) loadNamedays();
    if (!s_ndTable)  return "";
    const char *s = s_ndTable[(month-1)*31 + (day-1)];
    return (s && s[0]) ? s : "";
}

// ============================================================================
// sdFamilyEvent — /family.csv  (MM,DD,Opis swieta)
// tomorrow=true → zwraca wpis na jutro (zapowiedz "Jutro: ...")
// ============================================================================
static char **s_famTable  = nullptr;
static bool   s_famLoaded = false;

static void loadFamilyEvents() {
    s_famLoaded = true;
    s_famTable = (char **)heap_caps_calloc(12 * 31, sizeof(char *), MALLOC_CAP_SPIRAM);
    if (!s_famTable) { Serial.println("[SD] Family: brak RAM"); return; }

    File f = SD.open("/family.csv", FILE_READ);
    if (!f) { Serial.println("[SD] Brak /family.csv"); return; }

    int n = 0;
    while (f.available()) {
        char line[80]; int len = 0;
        while (f.available() && len < 79) {
            char c = f.read();
            if (c == '\n') break;
            if (c != '\r') line[len++] = c;
        }
        line[len] = '\0';
        if (len < 6 || line[2] != ',' || line[5] != ',') continue;
        int mo = (line[0]-'0')*10 + (line[1]-'0');
        int da = (line[3]-'0')*10 + (line[4]-'0');
        if (mo < 1 || mo > 12 || da < 1 || da > 31) continue;
        const char *desc = line + 6;
        int dlen = len - 6;
        if (dlen <= 0) continue;
        int idx = (mo-1)*31 + (da-1);
        if (s_famTable[idx]) free(s_famTable[idx]);
        s_famTable[idx] = (char *)heap_caps_malloc(dlen + 1, MALLOC_CAP_SPIRAM);
        if (s_famTable[idx]) {
            memcpy(s_famTable[idx], desc, dlen);
            s_famTable[idx][dlen] = '\0';
            n++;
        }
    }
    f.close();
    Serial.printf("[SD] Family events: %d wpisow\n", n);
}

const char *sdFamilyEvent(int month, int day, bool tomorrow) {
    if (!s_sdOk) return "";
    if (!s_famLoaded) loadFamilyEvents();
    if (!s_famTable)  return "";

    // Jeśli tomorrow=true — przesuń datę o +1
    if (tomorrow) {
        day++;
        int dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        // Rok bieżący — uproszczone (nie liczymy przestępności, różnica to max 1 dzień)
        if (day > dim[month]) { day = 1; month++; }
        if (month > 12) { month = 1; }
    }

    if (month < 1 || month > 12 || day < 1 || day > 31) return "";
    const char *s = s_famTable[(month-1)*31 + (day-1)];
    return (s && s[0]) ? s : "";
}

// ============================================================================
// sdHoliday — /holidays.csv  (popularne swieta polskie)
// ============================================================================
static char **s_holTable  = nullptr;
static bool   s_holLoaded = false;

static void loadHolidays() {
    s_holLoaded = true;
    s_holTable = (char **)heap_caps_calloc(12 * 31, sizeof(char *), MALLOC_CAP_SPIRAM);
    if (!s_holTable) { Serial.println("[SD] Holidays: brak RAM"); return; }

    File f = SD.open("/holidays.csv", FILE_READ);
    if (!f) { Serial.println("[SD] Brak /holidays.csv"); return; }

    int n = 0;
    while (f.available()) {
        char line[80]; int len = 0;
        while (f.available() && len < 79) {
            char c = f.read();
            if (c == '\n') break;
            if (c != '\r') line[len++] = c;
        }
        line[len] = '\0';
        if (len < 6 || line[2] != ',' || line[5] != ',') continue;
        int mo = (line[0]-'0')*10 + (line[1]-'0');
        int da = (line[3]-'0')*10 + (line[4]-'0');
        if (mo < 1 || mo > 12 || da < 1 || da > 31) continue;
        const char *desc = line + 6;
        int dlen = len - 6;
        if (dlen <= 0) continue;
        int idx = (mo-1)*31 + (da-1);
        if (s_holTable[idx]) free(s_holTable[idx]);
        s_holTable[idx] = (char *)heap_caps_malloc(dlen + 1, MALLOC_CAP_SPIRAM);
        if (s_holTable[idx]) {
            memcpy(s_holTable[idx], desc, dlen);
            s_holTable[idx][dlen] = '\0';
            n++;
        }
    }
    f.close();
    Serial.printf("[SD] Holidays: %d wpisow\n", n);
}

const char *sdHoliday(int month, int day, bool tomorrow) {
    if (!s_sdOk) return "";
    if (!s_holLoaded) loadHolidays();
    if (!s_holTable)  return "";
    if (tomorrow) {
        day++;
        int dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        if (day > dim[month]) { day = 1; month++; }
        if (month > 12) { month = 1; }
    }
    if (month < 1 || month > 12 || day < 1 || day > 31) return "";
    const char *s = s_holTable[(month-1)*31 + (day-1)];
    return (s && s[0]) ? s : "";
}
