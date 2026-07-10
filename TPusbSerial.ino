/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPusbSerial.ino
 * Zweck: USB-Serial-Ausgabe und Protokoll-Hilfen.
 *
 * Abgeleitet aus: PSWRusbSerial.ino
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2014 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>
#include "TP_T.h"

// Externe Klimavariablen für die USB-Ausgabe bereitstellen
extern float tempSpiegel; 
extern float tempUmgebung; 
extern float relativeFeuchte; 
extern float präziserTaupunkt;
extern float baroDruckHPa;
extern double amp_avg;
extern uint8_t interfaceOutputFilterIndex(void);
extern bool outputDataGetSample(uint8_t filterIndex, output_data_sample_t* out);

extern char lcd_buf[];

static void FLASHMEM usbGetOutputSample(output_data_sample_t* s)
{
  if (s == nullptr) return;
  if (!outputDataGetSample(interfaceOutputFilterIndex(), s))
  {
    memset(s, 0, sizeof(*s));
    s->tMirror = tempSpiegel;
    s->tAmbient = tempUmgebung;
    s->dewpoint = präziserTaupunkt;
    s->rh = relativeFeuchte;
    s->pressure = baroDruckHPa;
  }
}

// -------------------------------------------------------------------------
// Klimadaten kompakt an den PC senden (Befehl: $ppoll)
// -------------------------------------------------------------------------
void FLASHMEM usb_poll_data(void) {
  output_data_sample_t s;
  usbGetOutputSample(&s);
  Serial.print("T_Spiegel:"); Serial.print(s.tMirror, 2);
  Serial.print(" T_Luft:"); Serial.print(s.tAmbient, 2);
  Serial.print(" rF:"); Serial.print(s.rh, 1);
  Serial.print(" Taupunkt:"); Serial.println(s.dewpoint, 2);
}

// -------------------------------------------------------------------------
// Klimadaten im Klartext an den PC senden (Befehl: $pinst oder $plong)
// -------------------------------------------------------------------------
void FLASHMEM usb_poll_inst(void) {
  output_data_sample_t s;
  usbGetOutputSample(&s);
  Serial.println("=========================================");
  Serial.println("TAUPUNKTSPIEGEL HYGROMETER - STATUSREPORT");
  Serial.println("=========================================");
  Serial.print("Spiegeltemperatur : "); Serial.print(s.tMirror, 3); Serial.println(" C");
  Serial.print("Lufttemperatur    : "); Serial.print(s.tAmbient, 3); Serial.println(" C");
  Serial.print("Rel. Feuchtigkeit : "); Serial.print(s.rh, 2); Serial.println(" % rF");
  Serial.print("Präziser Taupunkt : "); Serial.print(s.dewpoint, 3); Serial.println(" C");
  Serial.print("Luftdruck         : "); Serial.print(s.pressure, 1); Serial.println(" hPa");
  Serial.print("Peltierstrom      : "); Serial.print(amp_avg, 2); Serial.println(" A");
  Serial.println("=========================================");
}

// Dummy-Funktionen für die Takt-Schleife der Hauptdatei, um Fehler zu verhindern
void FLASHMEM usb_poll_pk(void) { usb_poll_data(); }
void FLASHMEM usb_poll_pep(void) { usb_poll_data(); }
void FLASHMEM usb_poll_avg(void) { usb_poll_data(); }
void FLASHMEM usb_poll_1savg(void) { usb_poll_data(); }
void FLASHMEM usb_poll_instdb(void) { usb_poll_data(); }
void FLASHMEM usb_poll_pkdb(void) { }
void FLASHMEM usb_poll_pepdb(void) { }
void FLASHMEM usb_poll_avgdb(void) { }
void FLASHMEM usb_poll_1savgdb(void) { }
void FLASHMEM usb_poll_long(void) { usb_poll_inst(); }
void FLASHMEM usb_poll_ad_debug(void) { Serial.println("ADS1263 online."); }

// -------------------------------------------------------------------------
// Hilfe-Menü über USB ausgeben
// -------------------------------------------------------------------------
void FLASHMEM usb_print_help(void) {
  Serial.println(F(
    "Verfuegbare USB-Kommandos (Start immer mit $):\r\n"
    "\r\n"
    "$ppoll     Gibt die Klimadaten kompakt aus (1 Zeile).\r\n"
    "$pinst     Gibt den ausfuehrlichen Klartext-Statusreport aus.\r\n"
    "$plong     Gibt den ausfuehrlichen Klartext-Statusreport aus.\r\n"
    "$pcont     Aktiviert die dauerhafte Datenausgabe (10x pro Sekunde).\r\n"
    "\r\n"
    "$dim x     Setzt die TFT-Helligkeit direkt per PC. x = 1 bis 10.\r\n"
    "$sleepmsg=xxx  Aendert den Geraetenamen auf dem Display (max 19 Zeichen).\r\n"
    "\r\n"
    "$version   Zeigt Firmware-Version und Build-Datum.\r\n"
    "$memorywipe Fuehrt einen vollstaendigen Factory-Reset durch.\r\n"
    "$help      Oeffnet diese Hilfeuebersicht.\r\n"
  )); 
}
extern char incoming_command_string[50]; // Stammt aus dem globalen Speicher

void FLASHMEM usb_parse_incoming(void) {
  char *pEnd;
  double dim_display;

  if (!strcasecmp("ppoll", incoming_command_string)) {
    if ((R.usb_report_type != REPORT_DATA) || R.usb_report_cont) {
      tpMainConfigLoad();
      R.usb_report_type = REPORT_DATA;
      R.usb_report_cont = false;
      tpMainConfigSave(); 
    }
    usb_poll_data();
  }
  else if (!strcasecmp("pinst", incoming_command_string) || !strcasecmp("plong", incoming_command_string)) {
    if ((R.usb_report_type != REPORT_INST) || R.usb_report_cont) {
      tpMainConfigLoad();
      R.usb_report_type = REPORT_INST;
      R.usb_report_cont = false;
      tpMainConfigSave(); 
    }
    usb_poll_inst();
  }
  else if (!strcasecmp("addebug", incoming_command_string)) {
    if (R.usb_report_cont) { R.usb_report_cont = false; }
    R.usb_report_type = REPORT_AD_DEBUG;
    usb_poll_ad_debug();
  }
  else if (!strcasecmp("pcont", incoming_command_string)) {
    if (!R.usb_report_cont) {
      tpMainConfigLoad();
      R.usb_report_cont = true;
      tpMainConfigSave();
    }
  }
  else if (!strncasecmp("dim", incoming_command_string, 3)) {
    dim_display = strtod(incoming_command_string + 3, &pEnd);
    if (dim_display >= 0 && dim_display <= 10) {
      R.display_konfig.tft_backlight = (uint8_t)dim_display;
      tft.brightness((uint8_t)map(R.display_konfig.tft_backlight, 0, 10, 5, 230));
      tpMainConfigSave();
    }
  }
  else if (!strncasecmp("sleepmsg=", incoming_command_string, 9)) {
    tpMainConfigLoad();
    if (deviceSerialSet(incoming_command_string + 9)) {
      tpMainConfigSave();
      Serial.print("Neue Geraete-SN: "); Serial.println(deviceSerialGet());
    } else {
      Serial.println("FEHLER: Geraete-SN muss exakt 5 Ziffern haben (00000..99999).");
    }
  }
  else if (!strcasecmp("version", incoming_command_string)) {
    Serial.print("Teensy 4.0 Metrologischer Taupunktspiegel. Version: ");
    Serial.print(VERSION); Serial.print(" vom "); Serial.println(DATE);
  }
  else if (!strcasecmp("memorywipe", incoming_command_string)) {
    Serial.println("Factory Reset erzwungen... Starte neu.");
    tpMainConfigFactoryReset();
    delay(100);
    SOFT_RESET();
  }
  else if (!strcasecmp("softreset", incoming_command_string)) {
    Serial.println("Rebooting...");
    delay(100);
    SOFT_RESET();
  }
  else if (!strcasecmp("help", incoming_command_string)) {
    usb_print_help();
  }
}

void FLASHMEM usb_read_serial(void) {
  static uint8_t a; 
  static uint8_t Incoming; 
  uint8_t ReceivedChar;
  uint8_t waiting; 
  
  waiting = Serial.available(); 
  if (waiting && !Incoming) {
    ReceivedChar = Serial.read();
    if (ReceivedChar == '$') { 
      Incoming = true;
      a = 0;
      waiting--;
    }
  }
  while (waiting && Incoming) {
    ReceivedChar = Serial.read();
    waiting--;
    if (a == sizeof(incoming_command_string) - 1) {
      Incoming = false;
      a = 0;
    }
    else if ((ReceivedChar == '\r') || (ReceivedChar == '\n') || (ReceivedChar == ';')) {
      incoming_command_string[a] = 0; 
      usb_parse_incoming(); 
      Incoming = false;
      a = 0;
    }
    else {
      incoming_command_string[a] = ReceivedChar;
    }
    a++; 
  }
}

void FLASHMEM usb_cont_report(void) {
  if (R.usb_report_cont) {
    if (R.usb_report_type == REPORT_DATA)        usb_poll_data();
    else if (R.usb_report_type == REPORT_INST)   usb_poll_inst();
    else if (R.usb_report_type == REPORT_AD_DEBUG) usb_poll_ad_debug();
  }
}
