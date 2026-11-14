/*******************************************************************************
 * AI Bijin Tokei
 ******************************************************************************/
const char *SSID_NAME = "YourAP";
const char *SSID_PASSWORD = "PleaseInputYourPasswordHere";
const long gmtOffset_sec = 8 * 60 * 60; // +8, timezone

/* Uncomment one and only one below Dev Device pins header */
// #include <PINS_MAKERFABS_ESP32_S3_TFT_4_0.h>
#include <PINS_ESP32-C5-LCD-2_73.h>

#ifndef DEV_DEVICE_PINS
  #error "Please include one of the Dev Device pins header"
#endif

#ifndef GFX_SUPPORTED
  #error "No display in this Dev Device"
#endif

#ifdef RTC_SUPPORT
RTC_CLASS rtc;
#endif

#include <WiFi.h>
#include <WiFiMulti.h>
WiFiMulti WiFiMulti;

const char *root = "/root";
#include <FFat.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <SD.h>
#ifdef SOC_SDMMC_HOST_SUPPORTED
#include <SD_MMC.h>
#endif

#include "JpegFunc.h"

// Not sure if NetworkClientSecure checks the validity date of the certificate.
// Setting clock just to be sure...
void setClock()
{
  configTime(0, 0, "pool.ntp.org");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  char buf[26];
  Serial.println(asctime_r(&timeinfo, buf));
#ifdef RTC_SUPPORT
  rtc.set(&timeinfo);
#endif
}

// pixel drawing callback
static int jpegDrawCallback(JPEGDRAW *pDraw)
{
  // Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  gfx->draw16bitRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

void setup()
{
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("AI Bijin Tokei");

  // If display and SD shared same interface, init SPI first
#ifdef SPI_SCK
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
#endif

  // Init Display
  if (!gfx->begin(GFX_SPEED))
  {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->flush();

#ifdef GFX_BL
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR < 3)
  ledcSetup(0, 1000, 8);
  ledcAttachPin(GFX_BL, 0);
  ledcWrite(0, 63);
#else  // ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(GFX_BL, 1000, 8, 1);
  ledcWrite(GFX_BL, 63);
#endif // ESP_ARDUINO_VERSION_MAJOR >= 3
#endif // GFX_BL

  Serial.println(F("Connection to WiFi..."));
  gfx->setTextColor(RGB565_RED);
  gfx->println(F("Connection to WiFi..."));
  gfx->flush();
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID_NAME, SSID_PASSWORD);

  gfx->setTextColor(RGB565_ORANGE);
#if defined(SD_D1) && defined(SOC_SDMMC_HOST_SUPPORTED)
#define FILESYSTEM SD_MMC
  Serial.println("mount SD_MMC 4-bit");
  SD_MMC.setPins(SD_SCK, SD_MOSI /* CMD */, SD_MISO /* D0 */, SD_D1, SD_D2, SD_CS /* D3 */);
  if (!SD_MMC.begin(root, false /* mode1bit */, false /* format_if_mount_failed */, SDMMC_FREQ_HIGHSPEED))
#elif defined(SD_SCK) && defined(SOC_SDMMC_HOST_SUPPORTED)
#define FILESYSTEM SD_MMC
  Serial.println("mount SD_MMC 1-bit");
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SD_MMC.setPins(SD_SCK, SD_MOSI /* CMD */, SD_MISO /* D0 */);
  if (!SD_MMC.begin(root, true /* mode1bit */, false /* format_if_mount_failed */, SDMMC_FREQ_HIGHSPEED))
#elif defined(SD_CS)
#define FILESYSTEM SD
  Serial.println("mount SPI SD");
  if (!SD.begin(SD_CS, SPI, SPI_SPEED, root))
#else
#define FILESYSTEM FFat
  Serial.println("mount FFat");
  if (!FFat.begin(false, root))
  // Serial.println("mount LittleFS");
  // if (!LittleFS.begin(false, root))
  // Serial.println("mount SPIFFS");
  // if (!SPIFFS.begin(false, root))
#endif
  {
    Serial.println(F("ERROR: File System Mount Failed!"));
    gfx->println(F("ERROR: File System Mount Failed!"));
  }
  else
  {
    Serial.println(F("File System Mounted"));
    gfx->println(F("File System Mounted"));
  }
  gfx->flush();
}

unsigned long next_update_ntp_ms = 0;
char timePath[7]; // e.g. /00/00
time_t rounding_time;
time_t last_drawn_time = 0;
void loop()
{
#ifdef RTC_SUPPORT
  if (!rtc.oscillator_stop())
  {
    rounding_time = rtc.time(NULL);
#else
  if ((millis() < next_update_ntp_ms)
  {
    time(&rounding_time);
#endif
    rounding_time += gmtOffset_sec;
    rounding_time /= 60; // round to minutes
    rounding_time *= 60;

    if (rounding_time > last_drawn_time) {
      last_drawn_time = rounding_time;

      struct tm *tmLocal = localtime(&rounding_time);
      strftime(timePath, sizeof(timePath), "/%H/%M", tmLocal);

      Serial.printf("Open folder: %s\n", timePath);
      File dir = FILESYSTEM.open(timePath);
      if (!dir.isDirectory())
      {
        Serial.println("Not a directory");
        delay(5000); // avoid error repeat too fast
      }
      else
      {
        int file_count = 0;
        File file = dir.openNextFile();
        while (file)
        {
          if (!file.isDirectory())
          {
            // Serial.println(file.name());
            std::string s = file.name();
            // if ((!s.starts_with(".")) && (s.ends_with(".avi")))
            if ((s.rfind(".", 0) != 0) && ((int)s.find(".jpg", 0) > 0))
            {
              ++file_count;
            }
          }
          file = dir.openNextFile();
        }

        int selected_file_idx = 0;
        if (file_count > 1) {
          selected_file_idx = random(file_count);
        }

        dir.rewindDirectory();
        file_count = 0;
        file = dir.openNextFile();
        std::string f;
        while (file)
        {
          if (!file.isDirectory())
          {
            std::string s = file.name();
            if ((s.rfind(".", 0) != 0) && ((int)s.find(".jpg", 0) > 0))
            {
              if (selected_file_idx == file_count++) {
                f = file.path();
                break;
              }
            }
          }
          file = dir.openNextFile();
        }
        dir.close();

        if (file_count > 0) {
          jpegDraw((char *)f.c_str(), jpegDrawCallback, false /* useBigEndian */,
            0 /* x */, 0 /* y */, gfx->width() /* widthLimit */, gfx->height() /* heightLimit */);
        }
      }
    }
  }

#ifdef RTC_SUPPORT
  if ((millis() > next_update_ntp_ms) && (WiFiMulti.run() == WL_CONNECTED))
  {
    Serial.println(F("setClock();"));
    setClock();
    next_update_ntp_ms = millis() + (1000 * 60 * 60 * 24);
  }
#endif

  delay(1000);
}
