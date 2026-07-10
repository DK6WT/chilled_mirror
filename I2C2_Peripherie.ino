/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: I2C2_Peripherie.ino
 * Zweck: Ansteuerung von RV-3129 RTC und BMP585 Drucksensor.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Dieses Programm wird ohne jede Gewaehrleistung bereitgestellt.
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#include <Wire.h>
#include <TimeLib.h>
#include <SparkFun_BMP581_Arduino_Library.h>
#include <RV3129.h>

BMP581 pressureSensor;
RV3129 rtc;

// Globale Ausgabewerte für Zeit und Druck
uint8_t rtcSekunde = 0;
uint8_t rtcMinute  = 0;
uint8_t rtcStunde  = 0;

uint8_t rtcTag   = 1;
uint8_t rtcMonat = 1;
uint8_t rtcJahr  = 26;

float baroDruckHPa = 1013.25;

bool rtc_online = false;
bool bmp_online = false;

// RTC-Jahresbereich fuer das Setup-Menue und die Plausibilitaetspruefung.
// Die RV-3129 speichert das Jahr intern als 20xx-BCD-Zaehler.
// Laut Application Manual ist der gueltige Jahreszaehler 00..79,
// also 2000..2079. Fuer das TP-3000 wird bewusst erst ab 2026
// akzeptiert, damit alte Default-/Fehldaten nicht plausibel werden.
static const uint16_t RTC_FULL_YEAR_MIN = 2026;
static const uint16_t RTC_FULL_YEAR_MAX = 2079;

static uint16_t FLASHMEM rtcJahrVoll()
{
  return (uint16_t)2000 + (uint16_t)rtcJahr;
}

static bool FLASHMEM rtcDatumGueltig(uint8_t tag, uint8_t monat, uint16_t jahr)
{
  if (monat < 1 || monat > 12) return false;

  uint8_t tageImMonat = 31;

  if (monat == 4 || monat == 6 || monat == 9 || monat == 11)
  {
    tageImMonat = 30;
  }
  else if (monat == 2)
  {
    bool schaltjahr = ((jahr % 4) == 0 && ((jahr % 100) != 0 || (jahr % 400) == 0));
    tageImMonat = schaltjahr ? 29 : 28;
  }

  return (tag >= 1 && tag <= tageImMonat);
}


// =========================================================================
// Hilfsfunktion: RTC-Werte plausibel?
// =========================================================================
static bool FLASHMEM rtcZeitPlausibel()
{
  if (rtcSekunde > 59) return false;
  if (rtcMinute  > 59) return false;
  if (rtcStunde  > 23) return false;

  uint16_t jahrVoll = rtcJahrVoll();

  if (jahrVoll < RTC_FULL_YEAR_MIN || jahrVoll > RTC_FULL_YEAR_MAX) return false;
  if (!rtcDatumGueltig(rtcTag, rtcMonat, jahrVoll)) return false;

  return true;
}

static void FLASHMEM rtcSyncTeensyClockFromTimeLib()
{
#if defined(__arm__) && defined(TEENSYDUINO)
  // Die Teensy-SD-Library erzeugt FAT-Zeitstempel ueber Teensy3Clock,
  // nicht direkt ueber TimeLib. Deshalb muss die interne Teensy-Uhr nach
  // jeder gueltigen RV-3129/TimeLib-Synchronisation mitgezogen werden.
  uint16_t y = (uint16_t)year();
  if (y >= RTC_FULL_YEAR_MIN && y <= RTC_FULL_YEAR_MAX)
  {
    Teensy3Clock.set((uint32_t)now());
  }
#endif
}


// =========================================================================
// Hilfsfunktion: RTC sauber auslesen
// =========================================================================
static void FLASHMEM rtcAuslesen()
{
  rtc.updateTime();
  delay(10);

  rtcSekunde = rtc.getSeconds();
  rtcMinute  = rtc.getMinutes();
  rtcStunde  = rtc.getHours();

  rtcTag     = rtc.getDate();
  rtcMonat   = rtc.getMonth();
  rtcJahr    = rtc.getYear();
}


// =========================================================================
// 1. INITIALISIERUNG FÜR I2C BUS 2
// =========================================================================
void FLASHMEM initI2C2Peripherie()
{
  Wire2.begin();
  Wire2.setClock(400000);
  delay(200);

  // -------------------------------------------------------------------------
  // RTC RV-3129
  // -------------------------------------------------------------------------
  if (rtc.begin(Wire2) == true)
  {
    Serial.println("RTC: RV-3129 erfolgreich initialisiert!");
    rtc_online = true;

    delay(100);

    // Wichtig:
    // RV3129 beim Start in 24h-Modus bringen.
    // Die Uhrzeit wird dabei NICHT auf Default gesetzt.
    rtc.set24Hour();
    delay(20);

    // Zweimal lesen, damit die Library sicher frische Registerwerte hat
    rtcAuslesen();
    delay(50);
    rtcAuslesen();

    if (rtcZeitPlausibel())
    {
      setTime(
        rtcStunde,
        rtcMinute,
        rtcSekunde,
        rtcTag,
        rtcMonat,
        rtcJahrVoll()
      );
      rtcSyncTeensyClockFromTimeLib();

      Serial.print("RTC gelesen: ");
      Serial.print(rtcTag);
      Serial.print(".");
      Serial.print(rtcMonat);
      Serial.print(".");
      Serial.print(rtcJahrVoll());
      Serial.print("  ");
      Serial.print(rtcStunde);
      Serial.print(":");
      Serial.print(rtcMinute);
      Serial.print(":");
      Serial.println(rtcSekunde);
    }
    else
    {
      Serial.println("RTC WARNUNG: unplausible Zeit gelesen - TimeLib nicht gesetzt!");
    }
  }
  else
  {
    Serial.println("WARNUNG: RV-3129 RTC reagiert nicht!");
    rtc_online = false;
  }

  // -------------------------------------------------------------------------
  // BMP585 Drucksensor
  // -------------------------------------------------------------------------
  delay(50);

  if (pressureSensor.beginI2C(0x47, Wire2) == BMP5_OK)
  {
    Serial.println("BMP585: Sensor erfolgreich auf Adresse 0x47 gestartet!");
    bmp_online = true;
  }
  else
  {
    Serial.println("WARNUNG: BMP585 Drucksensor antwortet nicht auf Adresse 0x47!");
    bmp_online = false;
  }
}


// =========================================================================
// 2. UHRZEIT AUSLESEN
// =========================================================================
void FLASHMEM leseEchtzeitUhr()
{
  if (!rtc_online) return;

  rtcAuslesen();

  if (!rtcZeitPlausibel())
  {
    Serial.println("RTC WARNUNG: unplausible Lesung verworfen!");
    return;
  }

  setTime(
    rtcStunde,
    rtcMinute,
    rtcSekunde,
    rtcTag,
    rtcMonat,
    rtcJahrVoll()
  );
  rtcSyncTeensyClockFromTimeLib();
}


// =========================================================================
// 3. LUFTDRUCK EINLESEN
// =========================================================================
void FLASHMEM leseDruckSensor()
{
  if (!bmp_online) return;

  bmp5_sensor_data data = {0, 0};

  int8_t err = pressureSensor.getSensorData(&data);

  if (err == BMP5_OK)
  {
    if (data.pressure > 0.0f)
    {
      baroDruckHPa = data.pressure / 100.0f;
    }
  }
}


// =========================================================================
// 4. UHRZEIT IN RTC SCHREIBEN
// =========================================================================
void FLASHMEM schreibeEchtzeitUhr()
{
  if (!rtc_online) return;

  uint16_t jahrVoll = year();

  // Sekunden ebenfalls uebernehmen. Das RTC-Setup-Menue setzt vorher bewusst
  // setTime(..., 0, ...), Web-Zeitsync kann dagegen die PC-Sekunde uebergeben.
  rtcSekunde = second();
  rtcMinute  = minute();
  rtcStunde  = hour();
  rtcTag     = day();
  rtcMonat   = month();

  if (jahrVoll < RTC_FULL_YEAR_MIN || jahrVoll > RTC_FULL_YEAR_MAX)
  {
    Serial.println("RTC FEHLER: TimeLib-Jahr ausserhalb des erlaubten Bereichs, nicht gespeichert!");
    return;
  }

  rtcJahr = jahrVoll - 2000;

  if (!rtcZeitPlausibel())
  {
    Serial.println("RTC FEHLER: TimeLib-Zeit unplausibel, nicht gespeichert!");
    return;
  }

  // Vor dem Schreiben 24h-Modus erzwingen
  rtc.set24Hour();
  delay(20);

  rtc.setTime(
    rtcSekunde,
    rtcMinute,
    rtcStunde,
    rtcTag,
    rtcMonat,
    jahrVoll,
    1
  );

  delay(50);

  rtcAuslesen();

  if (rtcZeitPlausibel())
  {
    setTime(
      rtcStunde,
      rtcMinute,
      rtcSekunde,
      rtcTag,
      rtcMonat,
      rtcJahrVoll()
    );
    rtcSyncTeensyClockFromTimeLib();

    Serial.println("RTC: Neue Uhrzeit erfolgreich gespeichert.");
  }
  else
  {
    Serial.println("RTC FEHLER: Zeit nach dem Schreiben unplausibel!");
  }
}