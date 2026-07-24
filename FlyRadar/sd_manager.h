#pragma once
// ============================================================================
// sd_manager — obsługa karty microSD (SPI, GPIO 10-13)
//
// Logo linii:   /airlines/WZZ.bmp   (120×50, BMP 24-bit)
// Ikonki moon:  /moon/0.bmp..7.bmp  (40×40, BMP 24-bit)
//               0=now, 1=przybyw.serp, 2=pierwsza kwadra, 3=przybyw.garb
//               4=pelnia, 5=ubyw.garb, 6=ostatnia kwadra, 7=ubyw.serp
// Imieniny:     /namedays.csv       (MM,DD,Imiona)
// ============================================================================

#include <stdint.h>
#include <lvgl.h>

static constexpr int LOGO_W = 120;
static constexpr int LOGO_H =  50;
static constexpr int MOON_W =  40;
static constexpr int MOON_H =  40;

bool        sdInit();
bool        sdAvailable();
bool        sdLoadAirlineLogo(const char *prefix, lv_color_t *buf);
bool        sdLoadMoonIcon(int phase0to7, lv_color_t *buf);
const char *sdNameday(int month, int day);

// Swieta rodzinne z /family.csv
const char *sdFamilyEvent(int month, int day, bool tomorrow = false);

// Popularne swieta polskie z /holidays.csv (zimni ogrodnicy, Wigilia, itp.)
const char *sdHoliday(int month, int day, bool tomorrow = false);
