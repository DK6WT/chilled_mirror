/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPmenu_Pages.ino
 * Zweck: Listen-, Auswahl-, Diagnose- und Infoseiten des Setup-Menues.
 *
 * Abgeleitet aus: PSWRmenu.ino
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2014 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

// =========================================================================
// TPmenu_Pages.ino
// Listen-, Auswahl- und Infoseiten - ausgelagert aus TPmenu.ino
// =========================================================================

// OBERSTE HAUPTMENÜ-EBENE
// =========================================================================
void FLASHMEM menu_level0(void)
{
  static int8_t current_selection = 0;
  static bool menu_boxen_bereit = false;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (!menu_boxen_bereit)
  {
    VirtLCDMenu->init(45, 12, 155, 40, DroidSansMono_20, WHITE, BLACK);
    VirtLCDMessage->init(45, 3, 140, 300, DroidSansMono_20, WHITE, BLACK);
    menu_boxen_bereit = true;
  }

  // Erzwungener Kaltstart beim Betreten von außen
  if (frisch_geoeffnet)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch_geoeffnet = false;
  }

  FirstBut = 0;
  LastBut = 4;   // Hauptmenue: zusaetzlicher FAN-Button ueber EXIT

  if (menuRunTextList(TXT_MENU_MAIN_TITLE,
                      level0_menu_items,
                      level0_menu_size,
                      current_selection,
                      3,
                      true,
                      false))
  {
    if (current_selection >= 0 && current_selection < level0_menu_size)
    {
      menu_level = level0_menu_next[current_selection];
    }
    else
    {
      menu_level = MENU_MAIN;
    }

    // Beim Wechsel in eine Unterseite verschwindet der zusaetzliche FAN-Button
    // wieder; alle Unterseiten behalten nur UP/DOWN/ENTER/EXIT.
    FirstBut = 0;
    LastBut = 3;

    flag.menu_lcd_upd = false;
  }
}


// =========================================================================
// REGLER-PARAMETER AUSWAHL
// =========================================================================
void FLASHMEM regler_parameter_auswahl_menu(void)
{
  static int8_t current_selection = 0;
  static bool untermenue_frisch = true;

  extern void controlParamsSaveAndLock(void);
  extern void controlParamsDiscardAndLock(void);
  extern bool controlParamsEditUnlocked(void);

  if (VirtLCDMenu == nullptr) return;

  if (untermenue_frisch)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    untermenue_frisch = false;
  }

  if (!controlParamsEditUnlocked())
  {
    const char *items[] = { "Werte ändern", "Zurück" };
    const uint8_t n = 2;

    if (menuListHandleInput(current_selection, n, 250))
    {
      if (current_selection == 0)
      {
        menu_level = MENU_CONTROL_UNLOCK;
      }
      else
      {
        menu_level = MENU_MAIN;
        untermenue_frisch = true;
      }
      flag.menu_lcd_upd = false;
      return;
    }

    if (!flag.menu_lcd_upd)
    {
      flag.menu_lcd_upd = true;
      FirstBut = 0;
      LastBut = 3;
      VirtLCDMenu->clear();
      VirtLCDMenu->setCursor(1, 1);
      VirtLCDMenu->print(T(TXT_MENU_CONTROL_TITLE));
      VirtLCDMenu->setCursor(1, 3);
      VirtLCDMenu->print("Status: gesperrt");
      VirtLCDMenu->setCursor(1, 4);
      snprintf(lcd_buf, 255, "Peltier max: %1u.%03u A", (unsigned)(peltierCurrentLimitGetMa() / 1000U), (unsigned)(peltierCurrentLimitGetMa() % 1000U));
      VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 5);
      snprintf(lcd_buf, 255, "Kp/Ki/Kd: %u / %.1f / %.1f", (unsigned)R.pid_kp, (double)R.pid_ki, (double)R.pid_kd);
      VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 6);
      snprintf(lcd_buf, 255, "Takt: %u ms  Lüfter: %u%%", (unsigned)R.regler_intervall_ms, (unsigned)R.fan_percent);
      VirtLCDMenu->print(lcd_buf);
      lcd_scroll_Menu(items, n, current_selection, 8, 1, 2);
      VirtLCDMenu->transfer();
      ReadButtons(true);
    }
    return;
  }

  const char *items[] = {
    "Peltier Maxstrom",
    "PID Kp",
    "PID Ki",
    "PID Kd",
    "Regelungs-Takt",
    "Optik-Zielwert",
    "Freiheiztemperatur",
    "Auto-Cal Intervall",
    "Messfilter",
    "ADC1 SFOCAL",
    "Werte speichern",
    "Änderungen verwerfen",
    "Zurück"
  };
  const uint16_t next[] = {
    MENU_PELTIER_CURRENT_LIMIT,
    MENU_PID_KP,
    MENU_PID_KI,
    MENU_PID_KD,
    MENU_CONTROL_PERIOD,
    MENU_OPTIK_TARGET,
    MENU_FREIHEIZ_TEMP,
    MENU_AUTOCAL_INTERVAL,
    MENU_ADC_FILTER_MODE,
    MENU_ADC1_SFOCAL_MODE,
    MENU_MAIN,
    MENU_MAIN,
    MENU_MAIN
  };
  const uint8_t n = sizeof(next) / sizeof(next[0]);

  if (current_selection >= n) current_selection = 0;

  if (menuListHandleInput(current_selection, n, 250))
  {
    if (current_selection == 10)
    {
      controlParamsSaveAndLock();
      menu_level = MENU_MAIN;
      untermenue_frisch = true;
    }
    else if (current_selection == 11 || current_selection == 12)
    {
      controlParamsDiscardAndLock();
      menu_level = MENU_MAIN;
      untermenue_frisch = true;
    }
    else
    {
      menu_level = next[current_selection];
    }
    flag.menu_lcd_upd = false;
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    if (VirtLCDMessage) VirtLCDMessage->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(TXT_MENU_CONTROL_TITLE));
    VirtLCDMenu->setCursor(1, 2);
    VirtLCDMenu->print("Status: entsperrt");
    lcd_scroll_Menu(items, n, current_selection, 4, 1, 5);
    VirtLCDMenu->transfer();
    ReadButtons(true);
  }
}

// =========================================================================
// PASSWORT FUER REGELPARAMETER: 5 Stellen, UP/DOWN je Stelle, ENTER weiter
// =========================================================================
void FLASHMEM control_password_menu(void)
{
  static uint8_t digit_index = 0;
  static uint8_t digits[DEVICE_SERIAL_DIGITS] = {0, 0, 0, 0, 0};
  static bool frisch = true;

  extern uint32_t menu_global_debounce;
  extern void controlParamsBeginEdit(void);

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (frisch)
  {
    digit_index = 0;
    for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) digits[i] = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  ReadButtons(false);
  uint8_t key = menuReadKey(220);

  if (key == MK_UP)
  {
    digits[digit_index] = (uint8_t)((digits[digit_index] + 1U) % 10U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_DOWN)
  {
    digits[digit_index] = (digits[digit_index] == 0U) ? 9U : (uint8_t)(digits[digit_index] - 1U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_ENTER)
  {
    if (digit_index < (DEVICE_SERIAL_DIGITS - 1U))
    {
      digit_index++;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else
    {
      char entered[DEVICE_SERIAL_DIGITS + 1U];
      for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) entered[i] = (char)('0' + digits[i]);
      entered[DEVICE_SERIAL_DIGITS] = '\0';

      if (strncmp(entered, deviceSerialGet(), DEVICE_SERIAL_DIGITS) == 0)
      {
        controlParamsBeginEdit();
        if (VirtLCDMessage) VirtLCDMessage->clear();
      }
      else
      {
        if (VirtLCDMessage) { VirtLCDMessage->clear(); VirtLCDMessage->setCursor(1, 1); VirtLCDMessage->print("Passwort falsch"); }
      }

      frisch = true;
      menu_level = MENU_CONTROL_PARAMETERS;
      flag.short_push = false;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
      return;
    }
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print("Passwort eingeben:");

    VirtLCDMenu->setCursor(1, 4);
    for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
    {
      if (i == digit_index) VirtLCDMenu->print("["); else VirtLCDMenu->print(" ");
      { char digitText[2] = { (char)('0' + digits[i]), '\0' }; VirtLCDMenu->print(digitText); }
      if (i == digit_index) VirtLCDMenu->print("]"); else VirtLCDMenu->print(" ");
    }

    VirtLCDMenu->setCursor(1, 8);
    VirtLCDMenu->print("UP/DOWN: Stelle");
    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print("ENTER: nächste/OK");
    VirtLCDMenu->transfer();
    ReadButtons(true);
  }
}

// =========================================================================
// SCHNITTSTELLEN AUSWAHL - VORBEREITUNG
// =========================================================================
void FLASHMEM interfaces_menu(void)
{
  static int8_t current_selection = 0;
  static bool untermenue_frisch = true;

  if (VirtLCDMenu == nullptr) return;

  if (untermenue_frisch)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    untermenue_frisch = false;
  }

  if (menuRunTextList(TXT_MENU_INTERFACES_TITLE,
                      interfaces_sub_menu_items,
                      interfaces_sub_menu_size,
                      current_selection,
                      3,
                      false,
                      false))
  {
    if (current_selection >= 0 && current_selection < interfaces_sub_menu_size)
    {
      menu_level = interfaces_sub_menu_next[current_selection];
    }
    else
    {
      menu_level = MENU_MAIN;
    }

    if (menu_level == MENU_MAIN)
    {
      untermenue_frisch = true;
    }

    flag.menu_lcd_upd = false;
  }
}

void FLASHMEM interface_placeholder_menu(TextId title)
{
  if (VirtLCDMenu == nullptr) return;

  FirstBut = 0;
  LastBut = 3;

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    VirtLCDMenu->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(title));
    VirtLCDMenu->setCursor(1, 4);
    VirtLCDMenu->print(T(TXT_MENU_INTERFACES_PLACEHOLDER));
    VirtLCDMenu->setCursor(1, 7);
    VirtLCDMenu->print(T(TXT_BACK));
    VirtLCDMenu->transfer();
    ReadButtons(true);
  }

  ReadButtons(false);

  if (menuReadKey(250) == MK_ENTER)
  {
    flag.short_push = false;
    menu_level = MENU_INTERFACES;
    flag.menu_lcd_upd = false;
  }
}


// KALIBRIERUNG HAUPTMENÜ
// =========================================================================
void FLASHMEM kalibrierung_haupt_menu(void)
{
  static int8_t current_selection = 0;

  if (VirtLCDMenu == nullptr) return;

  if (menuRunTextList(TXT_MENU_CAL_TITLE,
                      kalibrierung_menu_items,
                      kalibrierung_menu_size,
                      current_selection))
  {
    if (current_selection >= 0 && current_selection < kalibrierung_menu_size)
    {
      menu_level = kalibrierung_menu_next[current_selection];
    }
    else
    {
      menu_level = MENU_MAIN;
    }

    flag.menu_lcd_upd = false;
  }
}


// =========================================================================
// Pt100 2-PUNKT SENSORWAHL
// =========================================================================
void FLASHMEM pt100_2p_wahl_menu(void)
{
  static int8_t current_selection = 0;

  if (VirtLCDMenu == nullptr) return;

  if (menuRunTextList(TXT_MENU_PT100_2POINT_TITLE,
                      pt100_2p_wahl_menu_items,
                      pt100_2p_wahl_menu_size,
                      current_selection))
  {
    if (current_selection >= 0 && current_selection < pt100_2p_wahl_menu_size)
    {
      menu_level = pt100_2p_wahl_menu_next[current_selection];
    }
    else
    {
      menu_level = MENU_CALIBRATION;
    }

    flag.menu_lcd_upd = false;
  }
}

void FLASHMEM fühler_wahl_menu(void)
{
  static int8_t current_selection = 0;

  if (VirtLCDMenu == nullptr) return;

  if (menuRunTextList(TXT_MENU_SENSOR_SELECT_R0_TITLE,
                      sensor_select_menu_items,
                      sensor_select_menu_size,
                      current_selection))
  {
    if (current_selection >= 0 && current_selection < sensor_select_menu_size)
    {
      menu_level = sensor_select_menu_next[current_selection];
    }
    else
    {
      menu_level = MENU_CALIBRATION;
    }

    flag.menu_lcd_upd = false;
  }
}


// =========================================================================
// Pt100 EICHUNG R0 MIT 4 NACHKOMMASTELLEN
void FLASHMEM referenz_wahl_menu(void)
{
  static int8_t current_selection = 0;

  if (VirtLCDMenu == nullptr) return;

  if (menuRunTextList(TXT_MENU_REFERENCES_TITLE,
                      referenz_wahl_menu_items,
                      referenz_wahl_menu_size,
                      current_selection))
  {
    if (current_selection >= 0 && current_selection < referenz_wahl_menu_size)
    {
      menu_level = referenz_wahl_menu_next[current_selection];
    }
    else
    {
      menu_level = MENU_CALIBRATION;
    }

    flag.menu_lcd_upd = false;
  }
}


// =========================================================================
// GERAETE-SPEICHER / WERKSJUSTIERUNG
// ===========================================================================
void FLASHMEM device_storage_menu(void)
{
  static int8_t current_selection = 0;
  static bool untermenue_frisch = true;

  if (VirtLCDMenu == nullptr) return;

  if (untermenue_frisch || frisch_geoeffnet)
  {
    current_selection = 0;
    flag.menu_lcd_upd = false;
    untermenue_frisch = false;
    frisch_geoeffnet = false;
  }

  if (menuRunTextList(TXT_DEVICE_STORAGE_TITLE,
                      device_storage_menu_items,
                      device_storage_menu_size,
                      current_selection,
                      4,
                      false,
                      false))
  {
    if (current_selection >= 0 && current_selection < device_storage_menu_size)
    {
      menu_level = device_storage_menu_next[current_selection];
    }
    else
    {
      menu_level = MENU_MAIN;
    }

    if (menu_level == MENU_MAIN)
    {
      untermenue_frisch = true;
      frisch_geoeffnet = true;
    }

    flag.menu_lcd_upd = false;
  }
}

static void FLASHMEM deviceStoragePrintDate(uint32_t yyyymmdd)
{
  if (yyyymmdd == 0)
  {
    VirtLCDMenu->print("--.--.----");
    return;
  }

  uint16_t y = (uint16_t)(yyyymmdd / 10000UL);
  uint8_t m  = (uint8_t)((yyyymmdd / 100UL) % 100UL);
  uint8_t d  = (uint8_t)(yyyymmdd % 100UL);
  snprintf(lcd_buf, 255, "%02u.%02u.%04u", d, m, y);
  VirtLCDMenu->print(lcd_buf);
}

static void FLASHMEM deviceStorageBackToMenu()
{
  flag.short_push = false;
  menu_level = MENU_DEVICE_STORAGE;
  frisch_geoeffnet = true;
  flag.menu_lcd_upd = false;
}

static void FLASHMEM deviceStorageResetPassword(uint8_t& digit_index, uint8_t digits[])
{
  digit_index = 0;
  for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) digits[i] = 0;
}

static bool FLASHMEM deviceStoragePasswordMatches(const uint8_t digits[])
{
  char entered[DEVICE_SERIAL_DIGITS + 1U];
  for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) entered[i] = (char)('0' + digits[i]);
  entered[DEVICE_SERIAL_DIGITS] = '\0';
  return (strncmp(entered, deviceSerialGet(), DEVICE_SERIAL_DIGITS) == 0);
}

static bool FLASHMEM deviceStoragePasswordPage(TextId title,
                                               const char* line1,
                                               bool& password_ok,
                                               bool& resultMode,
                                               bool& lastResult,
                                               bool& passwordWrong,
                                               uint8_t& digit_index,
                                               uint8_t digits[])
{
  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return false;

  extern uint32_t menu_global_debounce;

  FirstBut = 0;
  LastBut = 3;

  ReadButtons(false);
  uint8_t key = menuReadKey(220);

  if (key == MK_UP)
  {
    digits[digit_index] = (uint8_t)((digits[digit_index] + 1U) % 10U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_DOWN)
  {
    digits[digit_index] = (digits[digit_index] == 0U) ? 9U : (uint8_t)(digits[digit_index] - 1U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_ENTER)
  {
    if (digit_index < (DEVICE_SERIAL_DIGITS - 1U))
    {
      digit_index++;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else
    {
      if (deviceStoragePasswordMatches(digits))
      {
        password_ok = true;
        flag.menu_lcd_upd = false;
        menu_global_debounce = millis();
        return true;
      }

      passwordWrong = true;
      lastResult = false;
      resultMode = true;
      flag.short_push = false;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
      return false;
    }
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;

    VirtLCDMenu->clear();
    VirtLCDMessage->clear();

    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(title));

    VirtLCDMenu->setCursor(1, 3);
    VirtLCDMenu->print(line1);

    VirtLCDMenu->setCursor(1, 5);
    VirtLCDMenu->print("Passwort eingeben:");

    VirtLCDMenu->setCursor(1, 7);
    for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
    {
      if (i == digit_index) VirtLCDMenu->print("["); else VirtLCDMenu->print(" ");
      { char digitText[2] = { (char)('0' + digits[i]), '\0' }; VirtLCDMenu->print(digitText); }
      if (i == digit_index) VirtLCDMenu->print("]"); else VirtLCDMenu->print(" ");
    }

    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print("ENTER: nächste/OK");

    VirtLCDMenu->transfer();
    ReadButtons(false);
  }

  return false;
}

static void FLASHMEM deviceStorageResultPage(TextId title, bool ok, const char* okText, const char* errText)
{
  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  FirstBut = 0;
  LastBut = 3;

  ReadButtons(false);
  if (menuReadKey(250) == MK_ENTER)
  {
    deviceStorageBackToMenu();
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;

    VirtLCDMenu->clear();
    VirtLCDMessage->clear();

    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(title));

    VirtLCDMenu->setCursor(1, 4);
    VirtLCDMenu->print(ok ? "OK" : "FEHLER");

    VirtLCDMenu->setCursor(1, 6);
    VirtLCDMenu->print(ok ? okText : errText);

    VirtLCDMenu->setCursor(1, 10);
    VirtLCDMenu->print(T(TXT_DEVICE_STORAGE_ENTER_BACK));

    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

static bool FLASHMEM deviceStorageRunAction(TextId title,
                                            const char* line1,
                                            const char* line2,
                                            bool (*actionFn)(void),
                                            bool requirePassword,
                                            bool& resultMode,
                                            bool& lastResult,
                                            int8_t& current_selection,
                                            bool& passwordOk,
                                            bool& passwordWrong,
                                            uint8_t& digit_index,
                                            uint8_t digits[])
{
  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return false;

  if (resultMode)
  {
    return false;
  }

  if (requirePassword && !passwordOk)
  {
    if (deviceStoragePasswordPage(title, line1, passwordOk, resultMode, lastResult, passwordWrong, digit_index, digits))
    {
      current_selection = 0;
    }
    return false;
  }

  FirstBut = 0;
  LastBut = 3;

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;

    VirtLCDMenu->clear();
    VirtLCDMessage->clear();

    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(title));

    VirtLCDMenu->setCursor(1, 3);
    VirtLCDMenu->print(line1);

    VirtLCDMenu->setCursor(1, 4);
    VirtLCDMenu->print(line2);

    lcd_scroll_MenuT(factory_menu_items, factory_menu_size, current_selection, 7, 1, 2);

    VirtLCDMenu->transfer();
    ReadButtons(false);
  }

  if (menuListHandleInput(current_selection, factory_menu_size, 250))
  {
    if (current_selection == 1)
    {
      lastResult = actionFn ? actionFn() : false;
      resultMode = true;
      flag.menu_lcd_upd = false;
    }
    else
    {
      deviceStorageBackToMenu();
    }
  }

  return resultMode;
}

void FLASHMEM device_storage_status_menu(void)
{
  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  FirstBut = 0;
  LastBut = 3;

  ReadButtons(false);

  if (menuReadKey(250) == MK_ENTER)
  {
    deviceStorageBackToMenu();
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;

    VirtLCDMenu->clear();
    VirtLCDMessage->clear();

    const bool settingsOk = deviceSettingsBackupValid();
    const bool factoryOk  = deviceFactoryCalBackupValid();

    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(TXT_DEVICE_STORAGE_STATUS_TITLE));

    VirtLCDMenu->setCursor(1, 3);
    snprintf(lcd_buf, 255, T(TXT_DEVICE_STORAGE_DEVICE_SN_FMT), deviceSerialGet());
    VirtLCDMenu->print(lcd_buf);

    VirtLCDMenu->setCursor(1, 4);
    snprintf(lcd_buf, 255, T(TXT_DEVICE_STORAGE_HEAD_FMT), headTypeTextGet(), (unsigned long)R.head_serial);
    VirtLCDMenu->print(lcd_buf);

    VirtLCDMenu->setCursor(1, 6);
    VirtLCDMenu->print("Einstellungen: ");
    VirtLCDMenu->print(settingsOk ? "OK " : "-- ");
    if (settingsOk) deviceStoragePrintDate(deviceSettingsBackupDate());

    VirtLCDMenu->setCursor(1, 7);
    VirtLCDMenu->print("Werksjust.:   ");
    VirtLCDMenu->print(factoryOk ? "OK " : "-- ");
    if (factoryOk) deviceStoragePrintDate(deviceFactoryCalBackupDate());

    VirtLCDMenu->setCursor(1, 8);
    VirtLCDMenu->print(T(TXT_DEVICE_STORAGE_FACTORY_SCOPE));

    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print(T(TXT_DEVICE_STORAGE_HEAD_SCOPE));

    VirtLCDMenu->setCursor(1, 10);
    VirtLCDMenu->print(T(TXT_DEVICE_STORAGE_ENTER_BACK));

    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

void FLASHMEM device_settings_load_menu(void)
{
  static bool resultMode = false;
  static bool lastResult = false;
  static bool passwordOk = false;
  static bool passwordWrong = false;
  static uint8_t digit_index = 0;
  static uint8_t digits[DEVICE_SERIAL_DIGITS] = {0, 0, 0, 0, 0};
  static int8_t current_selection = 0;

  if (frisch_geoeffnet)
  {
    resultMode = false;
    lastResult = false;
    passwordOk = false;
    passwordWrong = false;
    deviceStorageResetPassword(digit_index, digits);
    current_selection = 0;
    frisch_geoeffnet = false;
    flag.menu_lcd_upd = false;
  }

  if (resultMode)
  {
    deviceStorageResultPage(TXT_DEVICE_SETTINGS_LOAD,
                            lastResult,
                            "Einstellungen geladen.",
                            passwordWrong ? "Passwort falsch." : "Kein gültiges Backup.");
    return;
  }

  deviceStorageRunAction(TXT_DEVICE_SETTINGS_LOAD,
                         "Gespeicherte Einstellungen laden?",
                         "Werksjustierung bleibt unverändert.",
                         deviceSettingsBackupLoad,
                         false,
                         resultMode,
                         lastResult,
                         current_selection,
                         passwordOk,
                         passwordWrong,
                         digit_index,
                         digits);
}

void FLASHMEM device_factory_cal_load_menu(void)
{
  static bool resultMode = false;
  static bool lastResult = false;
  static bool passwordOk = false;
  static bool passwordWrong = false;
  static uint8_t digit_index = 0;
  static uint8_t digits[DEVICE_SERIAL_DIGITS] = {0, 0, 0, 0, 0};
  static int8_t current_selection = 0;

  if (frisch_geoeffnet)
  {
    resultMode = false;
    lastResult = false;
    passwordOk = false;
    passwordWrong = false;
    deviceStorageResetPassword(digit_index, digits);
    current_selection = 0;
    frisch_geoeffnet = false;
    flag.menu_lcd_upd = false;
  }

  if (resultMode)
  {
    deviceStorageResultPage(TXT_DEVICE_FACTORY_CAL_LOAD,
                            lastResult,
                            "Werksjustierung geladen.",
                            passwordWrong ? "Passwort falsch." : "Keine gültige Werksjustierung.");
    return;
  }

  deviceStorageRunAction(TXT_DEVICE_FACTORY_CAL_LOAD,
                         "Ref Low/High und Kanalwerte laden?",
                         "Kopfdaten bleiben unverändert.",
                         deviceFactoryCalBackupLoad,
                         false,
                         resultMode,
                         lastResult,
                         current_selection,
                         passwordOk,
                         passwordWrong,
                         digit_index,
                         digits);
}

void FLASHMEM device_settings_save_menu(void)
{
  static bool resultMode = false;
  static bool lastResult = false;
  static bool passwordOk = false;
  static bool passwordWrong = false;
  static uint8_t digit_index = 0;
  static uint8_t digits[DEVICE_SERIAL_DIGITS] = {0, 0, 0, 0, 0};
  static int8_t current_selection = 0;

  if (frisch_geoeffnet)
  {
    resultMode = false;
    lastResult = false;
    passwordOk = false;
    passwordWrong = false;
    deviceStorageResetPassword(digit_index, digits);
    current_selection = 0;
    frisch_geoeffnet = false;
    flag.menu_lcd_upd = false;
  }

  if (resultMode)
  {
    deviceStorageResultPage(TXT_DEVICE_SETTINGS_SAVE,
                            lastResult,
                            "Einstellungen gespeichert.",
                            passwordWrong ? "Passwort falsch." : "Speichern fehlgeschlagen.");
    return;
  }

  deviceStorageRunAction(TXT_DEVICE_SETTINGS_SAVE,
                         "Aktuelle Einstellungen speichern?",
                         "Bestehendes Backup wird ersetzt.",
                         deviceSettingsBackupSave,
                         false,
                         resultMode,
                         lastResult,
                         current_selection,
                         passwordOk,
                         passwordWrong,
                         digit_index,
                         digits);
}

void FLASHMEM device_factory_cal_save_menu(void)
{
  // Ablauf: altes Passwort pruefen, neue Geraete-SN eingeben, dann die
  // Werksjustierung mit dieser neuen Geraete-SN speichern und uebernehmen.
  static uint8_t phase = 0; // 0 = Passwort alt, 1 = neue Geraete-SN, 2 = Ergebnis
  static bool lastResult = false;
  static bool passwordWrong = false;
  static uint8_t digit_index = 0;
  static uint8_t digits[DEVICE_SERIAL_DIGITS] = {0, 0, 0, 0, 0};
  static char newSerial[DEVICE_SERIAL_DIGITS + 1U] = DEVICE_SERIAL_DEFAULT;
  extern uint32_t menu_global_debounce;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (frisch_geoeffnet)
  {
    phase = 0;
    lastResult = false;
    passwordWrong = false;
    deviceStorageResetPassword(digit_index, digits);
    strncpy(newSerial, deviceSerialGet(), sizeof(newSerial) - 1U);
    newSerial[sizeof(newSerial) - 1U] = '\0';
    frisch_geoeffnet = false;
    flag.menu_lcd_upd = false;
  }

  if (phase == 2)
  {
    deviceStorageResultPage(TXT_DEVICE_FACTORY_CAL_SAVE,
                            lastResult,
                            "Werksjustierung gespeichert.",
                            passwordWrong ? "Passwort falsch." : "Ref-/Kanalwerte ungültig.");
    return;
  }

  FirstBut = 0;
  LastBut = 3;

  ReadButtons(false);
  uint8_t key = menuReadKey(220);

  if (key == MK_UP)
  {
    digits[digit_index] = (uint8_t)((digits[digit_index] + 1U) % 10U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_DOWN)
  {
    digits[digit_index] = (digits[digit_index] == 0U) ? 9U : (uint8_t)(digits[digit_index] - 1U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_ENTER)
  {
    if (digit_index < (DEVICE_SERIAL_DIGITS - 1U))
    {
      digit_index++;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else if (phase == 0)
    {
      if (!deviceStoragePasswordMatches(digits))
      {
        passwordWrong = true;
        lastResult = false;
        phase = 2;
        flag.short_push = false;
        flag.menu_lcd_upd = false;
        menu_global_debounce = millis();
        return;
      }

      for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
      {
        const char c = deviceSerialGet()[i];
        digits[i] = (c >= '0' && c <= '9') ? (uint8_t)(c - '0') : 0U;
      }
      digit_index = 0;
      phase = 1;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else
    {
      for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) newSerial[i] = (char)('0' + digits[i]);
      newSerial[DEVICE_SERIAL_DIGITS] = '\0';
      lastResult = deviceFactoryCalBackupSaveForSerial(newSerial);
      passwordWrong = false;
      phase = 2;
      flag.short_push = false;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
      return;
    }
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;

    VirtLCDMenu->clear();
    VirtLCDMessage->clear();

    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(TXT_DEVICE_FACTORY_CAL_SAVE));

    VirtLCDMenu->setCursor(1, 3);
    if (phase == 0)
    {
      VirtLCDMenu->print("Aktuelle Geräte-SN prüfen");
      VirtLCDMenu->setCursor(1, 5);
      VirtLCDMenu->print("Passwort eingeben:");
    }
    else
    {
      snprintf(lcd_buf, 255, "Alt: G%s", deviceSerialGet());
      VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 5);
      VirtLCDMenu->print("Neue Geräte-SN:");
    }

    VirtLCDMenu->setCursor(1, 7);
    for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
    {
      if (i == digit_index) VirtLCDMenu->print("["); else VirtLCDMenu->print(" ");
      { char digitText[2] = { (char)('0' + digits[i]), '\0' }; VirtLCDMenu->print(digitText); }
      if (i == digit_index) VirtLCDMenu->print("]"); else VirtLCDMenu->print(" ");
    }

    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print("ENTER: nächste/OK");

    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

void FLASHMEM factory_menu(void)
{
  // Der alte Firmware-Default-Reset bleibt als Funktion im Code vorhanden,
  // ist aber nicht mehr im normalen Geraete-Speicher-Menue erreichbar.
  menu_level = MENU_DEVICE_STORAGE;
  frisch_geoeffnet = true;
  flag.menu_lcd_upd = false;
}


// =========================================================================
// SPRACHE AUSWAEHLEN
// =========================================================================
void FLASHMEM language_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    current_selection = (ui_language < LANG_COUNT) ? ui_language : LANG_DE;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  if (menuRunTextList(TXT_MENU_LANGUAGE_TITLE,
                      language_menu_items,
                      language_menu_size,
                      current_selection,
                      3,
                      false,
                      false))
  {
    if (current_selection == 0)
    {
      ui_language = LANG_DE;
      uiLanguageSave();
    }
    else if (current_selection == 1)
    {
      ui_language = LANG_EN;
      uiLanguageSave();
    }

    menu_level = MENU_MAIN;
    frisch = true;
    flag.menu_lcd_upd = false;
  }
}



// =========================================================================
// ANZEIGE / DIAGNOSE AUSWAHL
// =========================================================================
static void FLASHMEM displayModeConsumeTouch(void)
{
  TouchZ = false;
  TouchX = 0;
  TouchY = 0;
}

extern void invalidateAdcInfoDisplay(void);
extern void setupTouchLatchUntilRelease(void);

static void FLASHMEM displayModeApplyAndExit(uint8_t new_mode)
{
  extern uint32_t menu_global_debounce;

  if (new_mode > 3) new_mode = 0;

  // Die Diagnose-Seiten sind bewusst nur temporaer.
  // Sie werden nicht im EEPROM gespeichert, damit das Geraet nach
  // Power-Cycle/Reset immer wieder mit dem Hauptscreen startet.
  mode_display = new_mode;

  if (new_mode == 3)
  {
    invalidateAdcInfoDisplay();
  }

  tft.fillScreen(BLACK);
  delayMicroseconds(200);
  eraseDisplay();

  menu_level = MENU_MAIN;
  frisch_geoeffnet = true;

  flag.config_mode = false;
  flag.mode_change = true;
  flag.menu_lcd_upd = true;
  flag.short_push = false;

  FirstBut = -1;
  LastBut = -1;

  // ENTER und SETUP liegen beide unten links. Der aktuelle Finger darf nach
  // dem Screenwechsel nicht erneut als SETUP-Flanke erkannt werden. Der Latch
  // wird erst durch eine spaetere echte Touch-Freigabe geloescht.
  setupTouchLatchUntilRelease();
  displayModeConsumeTouch();
  menu_global_debounce = millis();
}

extern bool loopDebugDisplayGetEnabled(void);
extern void loopDebugDisplaySetEnabled(bool enabled);
extern uint8_t mainScreenLayoutGet(void);
extern void mainScreenLayoutSet(uint8_t layout);

const uint8_t main_layout_menu_size = 3;
const TextId main_layout_menu_items[] = {
  TXT_DISPLAY_MAIN_LAYOUT_STANDARD,
  TXT_DISPLAY_MAIN_LAYOUT_THREE_VALUES,
  TXT_BACK
};

const uint8_t loop_debug_menu_size = 3;
const TextId loop_debug_menu_items[] = {
  TXT_LOOP_DEBUG_OFF,
  TXT_LOOP_DEBUG_ON,
  TXT_BACK
};

void FLASHMEM display_mode_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (frisch || frisch_geoeffnet)
  {
    if (mode_display == 1) current_selection = 2;
    else if (mode_display == 2) current_selection = 3;
    else if (mode_display == 3) current_selection = 4;
    else current_selection = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
    frisch_geoeffnet = false;
  }

  if (menuRunTextList(TXT_DISPLAY_MODE_TITLE,
                      display_mode_menu_items,
                      display_mode_menu_size,
                      current_selection,
                      4,
                      false,
                      false))
  {
    if (current_selection == 0)
    {
      frisch = true;
      displayModeApplyAndExit(0);   // Hauptscreen
      return;
    }
    else if (current_selection == 1)
    {
      frisch = true;
      menu_level = MENU_MAIN_SCREEN_LAYOUT;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
      return;
    }
    else if (current_selection == 2)
    {
      frisch = true;
      displayModeApplyAndExit(1);   // Diagnose 1: Uebersicht
      return;
    }
    else if (current_selection == 3)
    {
      frisch = true;
      displayModeApplyAndExit(2);   // Diagnose 2: Details
      return;
    }
    else if (current_selection == 4)
    {
      frisch = true;
      displayModeApplyAndExit(3);   // ADC Info
      return;
    }
    else if (current_selection == 5)
    {
      frisch = true;
      menu_level = MENU_LOOP_DEBUG_DISPLAY;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
      return;
    }
    else
    {
      frisch = true;
      menu_level = MENU_MAIN;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
    }
  }
}

void FLASHMEM display_main_layout_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (frisch || frisch_geoeffnet)
  {
    current_selection = (mainScreenLayoutGet() == MAIN_SCREEN_LAYOUT_3VALUES) ? 1 : 0;
    flag.menu_lcd_upd = false;
    frisch = false;
    frisch_geoeffnet = false;
  }

  if (menuRunTextList(TXT_DISPLAY_MAIN_LAYOUT_TITLE,
                      main_layout_menu_items,
                      main_layout_menu_size,
                      current_selection,
                      3,
                      false,
                      false))
  {
    if (current_selection == 0)
    {
      mainScreenLayoutSet(MAIN_SCREEN_LAYOUT_STANDARD);
      frisch = true;
      menu_level = MENU_DISPLAY_MODE;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
      return;
    }
    else if (current_selection == 1)
    {
      mainScreenLayoutSet(MAIN_SCREEN_LAYOUT_3VALUES);
      frisch = true;
      menu_level = MENU_DISPLAY_MODE;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
      return;
    }
    else
    {
      frisch = true;
      menu_level = MENU_DISPLAY_MODE;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
      return;
    }
  }
}

void FLASHMEM display_loop_debug_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (frisch || frisch_geoeffnet)
  {
    current_selection = loopDebugDisplayGetEnabled() ? 1 : 0;
    flag.menu_lcd_upd = false;
    frisch = false;
    frisch_geoeffnet = false;
  }

  if (menuRunTextList(TXT_DISPLAY_LOOP_DEBUG_TITLE,
                      loop_debug_menu_items,
                      loop_debug_menu_size,
                      current_selection,
                      3,
                      false,
                      false))
  {
    if (current_selection == 0)
    {
      loopDebugDisplaySetEnabled(false);
      frisch = true;
      menu_level = MENU_DISPLAY_MODE;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
      return;
    }
    else if (current_selection == 1)
    {
      loopDebugDisplaySetEnabled(true);
      frisch = true;
      menu_level = MENU_DISPLAY_MODE;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
      return;
    }
    else
    {
      frisch = true;
      menu_level = MENU_DISPLAY_MODE;
      frisch_geoeffnet = true;
      flag.menu_lcd_upd = false;
      return;
    }
  }
}


// =========================================================================
// SENSOR-KOPF / KOPFDATEN
// =========================================================================
static char     headcal_selected_type_text[HEAD_TYPE_TEXT_LEN + 1U] = HEAD_TYPE_DEFAULT_TEXT;
static uint32_t headcal_selected_serial = HEAD_SERIAL_DEFAULT;
static char     headcal_message[96] = "";
static char     headcal_type_list[8][HEAD_TYPE_TEXT_LEN + 1U];
static uint8_t  headcal_type_count = 0;
static uint32_t headcal_serial_list[16];
static uint8_t  headcal_serial_count = 0;

void FLASHMEM sensorkopf_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr) return;

  const char *items[] = {
    "Kopftyp",
    "Kopf-Seriennummer",
    "Kopfdaten Status",
    "Kopfdaten laden",
    "Kopfdaten speichern",
    "Zurück"
  };
  const uint16_t next[] = {
    MENU_HEAD_TYPE_SELECT,
    MENU_HEAD_SERIAL,
    MENU_HEAD_CAL_STATUS,
    MENU_HEAD_CAL_LOAD_TYPE,
    MENU_HEAD_CAL_SAVE,
    MENU_MAIN
  };
  const uint8_t n = sizeof(next) / sizeof(next[0]);

  if (frisch)
  {
    current_selection = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, n, 250))
  {
    menu_level = next[current_selection];
    if (menu_level == MENU_MAIN)
    {
      frisch = true;
      frisch_geoeffnet = true;
    }
    flag.menu_lcd_upd = false;
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    if (VirtLCDMessage) VirtLCDMessage->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(TXT_SENSOR_HEAD_TITLE));
    VirtLCDMenu->setCursor(1, 2);
    snprintf(lcd_buf, 255, "%s K%05lu", headTypeTextGet(), (unsigned long)R.head_serial);
    VirtLCDMenu->print(lcd_buf);
    lcd_scroll_Menu(items, n, current_selection, 4, 1, 5);
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

// =========================================================================
// KOPFTYP AUSWAHL: STP-XXXX, STP-3001..3004 laden interne Profile
// =========================================================================
void FLASHMEM head_type_menu(void)
{
  static uint8_t digit_index = 0;
  static uint8_t digits[4] = {3,0,0,1};
  static bool frisch = true;
  extern uint32_t menu_global_debounce;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (frisch)
  {
    uint16_t nr = headTypeTextNumber(headTypeTextGet());
    for (int8_t i = 3; i >= 0; i--)
    {
      digits[i] = (uint8_t)(nr % 10U);
      nr /= 10U;
    }
    digit_index = 0;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  ReadButtons(false);
  uint8_t key = menuReadKey(220);

  if (key == MK_UP)
  {
    digits[digit_index] = (uint8_t)((digits[digit_index] + 1U) % 10U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_DOWN)
  {
    digits[digit_index] = (digits[digit_index] == 0U) ? 9U : (uint8_t)(digits[digit_index] - 1U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_ENTER)
  {
    if (digit_index < 3U)
    {
      digit_index++;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else
    {
      uint16_t nr = 0;
      for (uint8_t i = 0; i < 4; i++) nr = (uint16_t)(nr * 10U + digits[i]);
      char typeText[HEAD_TYPE_TEXT_LEN + 1U];
      if (headTypeTextFromNumber(nr, typeText, sizeof(typeText)))
      {
        const int8_t profile = headTypeProfileFromText(typeText);
        if (profile >= 0)
        {
          applySensorHeadProfile((uint8_t)profile);
        }
        else
        {
          headTypeTextSet(typeText);
        }
        tpMainConfigSave();
      }
      frisch = true;
      menu_level = MENU_SENSOR_HEAD;
      flag.short_push = false;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
      return;
    }
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    VirtLCDMessage->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print(T(TXT_SENSOR_HEAD_TITLE));
    VirtLCDMenu->setCursor(1, 3);
    VirtLCDMenu->print("Kopftyp: STP-");
    for (uint8_t i = 0; i < 4; i++)
    {
      if (i == digit_index) VirtLCDMenu->print("["); else VirtLCDMenu->print(" ");
      { char digitText[2] = { (char)('0' + digits[i]), '\0' }; VirtLCDMenu->print(digitText); }
      if (i == digit_index) VirtLCDMenu->print("]"); else VirtLCDMenu->print(" ");
    }
    VirtLCDMenu->setCursor(1, 7);
    VirtLCDMenu->print("STP-3001..3004 laden Profil");
    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print("ENTER: nächste/OK");
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

// =========================================================================
// KOPF-SERIENNUMMER: 5 Stellen, UP/DOWN je Stelle, ENTER weiter/speichern
// =========================================================================
void FLASHMEM head_serial_menu(void)
{
  static uint8_t digit_index = 0;
  static uint8_t digits[DEVICE_SERIAL_DIGITS] = {0,0,0,0,0};
  static bool frisch = true;
  extern uint32_t menu_global_debounce;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (frisch)
  {
    uint32_t sn = R.head_serial;
    if (sn > HEAD_SERIAL_MAX) sn = HEAD_SERIAL_DEFAULT;
    for (int8_t i = DEVICE_SERIAL_DIGITS - 1; i >= 0; i--)
    {
      digits[i] = (uint8_t)(sn % 10UL);
      sn /= 10UL;
    }
    digit_index = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  ReadButtons(false);
  uint8_t key = menuReadKey(220);

  if (key == MK_UP)
  {
    digits[digit_index] = (uint8_t)((digits[digit_index] + 1U) % 10U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_DOWN)
  {
    digits[digit_index] = (digits[digit_index] == 0U) ? 9U : (uint8_t)(digits[digit_index] - 1U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_ENTER)
  {
    if (digit_index < (DEVICE_SERIAL_DIGITS - 1U))
    {
      digit_index++;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else
    {
      uint32_t sn = 0;
      for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) sn = sn * 10UL + digits[i];
      R.head_serial = sn;
      tpMainConfigSave();
      frisch = true;
      menu_level = MENU_SENSOR_HEAD;
      flag.short_push = false;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
      return;
    }
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    VirtLCDMessage->clear();
    VirtLCDMenu->setCursor(1, 1);
    snprintf(lcd_buf, 255, T(TXT_SERIAL_TITLE_FMT), headTypeTextGet());
    VirtLCDMenu->print(lcd_buf);
    VirtLCDMenu->setCursor(1, 4);
    for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
    {
      if (i == digit_index) VirtLCDMenu->print("["); else VirtLCDMenu->print(" ");
      { char digitText[2] = { (char)('0' + digits[i]), '\0' }; VirtLCDMenu->print(digitText); }
      if (i == digit_index) VirtLCDMenu->print("]"); else VirtLCDMenu->print(" ");
    }
    VirtLCDMenu->setCursor(1, 8);
    VirtLCDMenu->print("UP/DOWN: Stelle");
    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print("ENTER: nächste/OK");
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

void FLASHMEM head_cal_status_menu(void)
{
  if (VirtLCDMenu == nullptr) return;

  FirstBut = 0;
  LastBut = 3;

  ReadButtons(false);
  if (menuReadKey(250) == MK_ENTER)
  {
    flag.short_push = false;
    menu_level = MENU_SENSOR_HEAD;
    flag.menu_lcd_upd = false;
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    char l1[64], l2[64];
    headCalStatusText(l1, sizeof(l1), l2, sizeof(l2));

    flag.menu_lcd_upd = true;
    VirtLCDMenu->clear();
    if (VirtLCDMessage) VirtLCDMessage->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print("--- KOPFDATEN STATUS ---");
    VirtLCDMenu->setCursor(1, 4);
    VirtLCDMenu->print(l1);
    VirtLCDMenu->setCursor(1, 5);
    VirtLCDMenu->print(l2);
    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print(T(TXT_DEVICE_STORAGE_ENTER_BACK));
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

void FLASHMEM head_cal_load_type_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr) return;
  if (frisch)
  {
    headcal_type_count = headCalListTypes(headcal_type_list, (uint8_t)(sizeof(headcal_type_list) / sizeof(headcal_type_list[0])));
    current_selection = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (headcal_type_count == 0)
  {
    snprintf(headcal_message, sizeof(headcal_message), "Keine Kopfdaten für G%s", deviceSerialGet());
    frisch = true;
    menu_level = MENU_HEAD_CAL_MESSAGE;
    flag.menu_lcd_upd = false;
    return;
  }

  if (menuListHandleInput(current_selection, headcal_type_count, 250))
  {
    strncpy(headcal_selected_type_text, headcal_type_list[current_selection], sizeof(headcal_selected_type_text) - 1U);
    headcal_selected_type_text[sizeof(headcal_selected_type_text) - 1U] = '\0';
    frisch = true;
    menu_level = MENU_HEAD_CAL_LOAD_SERIAL;
    flag.menu_lcd_upd = false;
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    const char* items[8];
    for (uint8_t i = 0; i < headcal_type_count; i++) items[i] = headcal_type_list[i];
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print("Kopftyp von SD wählen");
    lcd_scroll_Menu(items, headcal_type_count, current_selection, 4, 1, 5);
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

void FLASHMEM head_cal_load_serial_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr) return;
  if (frisch)
  {
    headcal_serial_count = headCalListSerials(headcal_selected_type_text, headcal_serial_list, sizeof(headcal_serial_list) / sizeof(headcal_serial_list[0]));
    current_selection = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (headcal_serial_count == 0)
  {
    snprintf(headcal_message, sizeof(headcal_message), "Keine Kopf-SN für %s", headcal_selected_type_text);
    frisch = true;
    menu_level = MENU_HEAD_CAL_MESSAGE;
    flag.menu_lcd_upd = false;
    return;
  }

  if (menuListHandleInput(current_selection, headcal_serial_count, 250))
  {
    headcal_selected_serial = headcal_serial_list[current_selection];
    bool ok = headCalLoadLatest(headcal_selected_type_text, headcal_selected_serial, headcal_message, sizeof(headcal_message));
    (void)ok;
    frisch = true;
    menu_level = MENU_HEAD_CAL_MESSAGE;
    flag.menu_lcd_upd = false;
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    char serial_items[16][16];
    const char* items[16];
    for (uint8_t i = 0; i < headcal_serial_count; i++)
    {
      snprintf(serial_items[i], sizeof(serial_items[i]), "K%05lu", (unsigned long)headcal_serial_list[i]);
      items[i] = serial_items[i];
    }
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    VirtLCDMenu->setCursor(1, 1);
    snprintf(lcd_buf, 255, "%s Kopf-SN", headcal_selected_type_text);
    VirtLCDMenu->print(lcd_buf);
    lcd_scroll_Menu(items, headcal_serial_count, current_selection, 4, 1, 5);
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

void FLASHMEM head_cal_save_menu(void)
{
  // Speichern fragt immer den Kopftyp STP-XXXX ab. Dadurch kann ein neuer
  // Kopftyp erzeugt werden, ohne vorher das Sensorkopf-Menue umzuschalten.
  static uint8_t digit_index = 0;
  static uint8_t digits[4] = {3,0,0,1};
  static bool frisch = true;
  extern uint32_t menu_global_debounce;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    uint16_t nr = headTypeTextNumber(headTypeTextGet());
    for (int8_t i = 3; i >= 0; i--)
    {
      digits[i] = (uint8_t)(nr % 10U);
      nr /= 10U;
    }
    digit_index = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  ReadButtons(false);
  uint8_t key = menuReadKey(220);
  if (key == MK_UP)
  {
    digits[digit_index] = (uint8_t)((digits[digit_index] + 1U) % 10U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_DOWN)
  {
    digits[digit_index] = (digits[digit_index] == 0U) ? 9U : (uint8_t)(digits[digit_index] - 1U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_ENTER)
  {
    if (digit_index < 3U)
    {
      digit_index++;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else
    {
      uint16_t nr = 0;
      for (uint8_t i = 0; i < 4; i++) nr = (uint16_t)(nr * 10U + digits[i]);
      if (!headTypeTextFromNumber(nr, headcal_selected_type_text, sizeof(headcal_selected_type_text)))
      {
        snprintf(headcal_message, sizeof(headcal_message), "Kopftyp ungültig");
        frisch = true;
        menu_level = MENU_HEAD_CAL_MESSAGE;
      }
      else
      {
        frisch = true;
        menu_level = MENU_HEAD_CAL_SAVE_PASSWORD;
      }
      flag.short_push = false;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
      return;
    }
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    if (VirtLCDMessage) VirtLCDMessage->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print("Kopfdaten speichern");
    VirtLCDMenu->setCursor(1, 3);
    VirtLCDMenu->print("Kopftyp: STP-");
    for (uint8_t i = 0; i < 4; i++)
    {
      if (i == digit_index) VirtLCDMenu->print("["); else VirtLCDMenu->print(" ");
      { char digitText[2] = { (char)('0' + digits[i]), '\0' }; VirtLCDMenu->print(digitText); }
      if (i == digit_index) VirtLCDMenu->print("]"); else VirtLCDMenu->print(" ");
    }
    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print("ENTER: nächste/OK");
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

void FLASHMEM head_cal_save_password_menu(void)
{
  static uint8_t phase = 0; // 0 = Kopf-SN, 1 = Passwort
  static uint8_t digit_index = 0;
  static uint8_t digits[DEVICE_SERIAL_DIGITS] = {0,0,0,0,0};
  static bool frisch = true;
  extern uint32_t menu_global_debounce;

  if (VirtLCDMenu == nullptr) return;
  if (frisch)
  {
    phase = 0;
    uint32_t sn = R.head_serial;
    if (sn > HEAD_SERIAL_MAX) sn = HEAD_SERIAL_DEFAULT;
    headcal_selected_serial = sn;
    for (int8_t i = DEVICE_SERIAL_DIGITS - 1; i >= 0; i--)
    {
      digits[i] = (uint8_t)(sn % 10UL);
      sn /= 10UL;
    }
    digit_index = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  ReadButtons(false);
  uint8_t key = menuReadKey(220);
  if (key == MK_UP)
  {
    digits[digit_index] = (uint8_t)((digits[digit_index] + 1U) % 10U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_DOWN)
  {
    digits[digit_index] = (digits[digit_index] == 0U) ? 9U : (uint8_t)(digits[digit_index] - 1U);
    flag.menu_lcd_upd = false;
    menu_global_debounce = millis();
  }
  else if (key == MK_ENTER)
  {
    if (digit_index < (DEVICE_SERIAL_DIGITS - 1U))
    {
      digit_index++;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else if (phase == 0)
    {
      uint32_t sn = 0;
      for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) sn = sn * 10UL + digits[i];
      headcal_selected_serial = sn;
      phase = 1;
      digit_index = 0;
      for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) digits[i] = 0;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }
    else
    {
      char entered[DEVICE_SERIAL_DIGITS + 1U];
      for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++) entered[i] = (char)('0' + digits[i]);
      entered[DEVICE_SERIAL_DIGITS] = '\0';
      if (strncmp(entered, deviceSerialGet(), DEVICE_SERIAL_DIGITS) == 0)
      {
        headCalSave(headcal_selected_type_text, headcal_selected_serial, headcal_message, sizeof(headcal_message));
      }
      else
      {
        snprintf(headcal_message, sizeof(headcal_message), "Passwort falsch");
      }
      frisch = true;
      menu_level = MENU_HEAD_CAL_MESSAGE;
      flag.short_push = false;
      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
      return;
    }
  }

  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    if (VirtLCDMessage) VirtLCDMessage->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print("Kopfdaten speichern");
    VirtLCDMenu->setCursor(1, 2);
    snprintf(lcd_buf, 255, "%s K%05lu", headcal_selected_type_text, (unsigned long)headcal_selected_serial);
    VirtLCDMenu->print(lcd_buf);
    VirtLCDMenu->setCursor(1, 4);
    VirtLCDMenu->print(phase == 0 ? "Kopf-SN eingeben:" : "Passwort eingeben:");
    VirtLCDMenu->setCursor(1, 6);
    for (uint8_t i = 0; i < DEVICE_SERIAL_DIGITS; i++)
    {
      if (i == digit_index) VirtLCDMenu->print("["); else VirtLCDMenu->print(" ");
      { char digitText[2] = { (char)('0' + digits[i]), '\0' }; VirtLCDMenu->print(digitText); }
      if (i == digit_index) VirtLCDMenu->print("]"); else VirtLCDMenu->print(" ");
    }
    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print("ENTER: nächste/OK");
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

void FLASHMEM head_cal_message_menu(void)
{
  if (VirtLCDMenu == nullptr) return;

  FirstBut = 0;
  LastBut = 3;

  ReadButtons(false);
  if (menuReadKey(250) == MK_ENTER)
  {
    flag.short_push = false;
    menu_level = MENU_SENSOR_HEAD;
    frisch_geoeffnet = true;
    flag.menu_lcd_upd = false;
    return;
  }
  if (!flag.menu_lcd_upd)
  {
    flag.menu_lcd_upd = true;
    FirstBut = 0;
    LastBut = 3;
    VirtLCDMenu->clear();
    if (VirtLCDMessage) VirtLCDMessage->clear();
    VirtLCDMenu->setCursor(1, 1);
    VirtLCDMenu->print("--- KOPFDATEN ---");
    VirtLCDMenu->setCursor(1, 4);
    VirtLCDMenu->print(headcal_message);
    VirtLCDMenu->setCursor(1, 9);
    VirtLCDMenu->print(T(TXT_DEVICE_STORAGE_ENTER_BACK));
    VirtLCDMenu->transfer();
    ReadButtons(false);
  }
}

// =========================================================================
// LÜFTER-SPEED SETUP: Bypass-Luftwechsel 50..100 % in 1-%-Schritten
// INFO / LICENSE
// =========================================================================
static void FLASHMEM infoLicenseClearArea(void)
{
  // Nur den mittleren Textbereich loeschen, die Setup-Buttons links/rechts
  // bleiben dabei unveraendert stehen. Wichtig, weil der Lizenztext direkt
  // auf das TFT geschrieben wird und nicht aus dem TextBox-Puffer kommt.
  tft.fillRect(140, 20, 520, 445, BLACK);
}

static const char* const infoLicenseLinesDE[] =
{
  "TP-3000 Taupunktspiegel-Firmware",
  "Version " VERSION " | " DATE,
  "Copyright (C) 2025/2026 S. Brachtl",
  "",
  "Basierend auf LJ2000M T_2.06c von:",
  "Loftur E. Jonasson",
  "J.G. Holstein",
  "",
  "Projektlizenz: GNU GPL Version 3 only",
  "SPDX-License-Identifier: GPL-3.0-only",
  "Freie Software: Weitergabe und",
  "Änderung nach GNU GPLv3-only erlaubt.",
  "Fremdkomponenten behalten ihre",
  "Lizenzen.",
  "Dieses Programm kommt OHNE JEDE",
  "GEWÄHRLEISTUNG.",
  "Quellcode und vollständige Lizenz:",
  "github.com/DK6WT/chilled_mirror",
  "Siehe auch LICENSE, README und",
  "THIRD_PARTY_NOTICES im Quellpaket.",
  "",
  "ALMEMO ist eine Marke von AHLBORN.",
  "AMR WinControl wird von akrobit",
  "entwickelt und über AHLBORN vertrieben.",
  "Die Namen beschreiben ausschließlich:",
  "Serielle Anbindung an ALMEMO-Geräte",
  "und V6-Ausgabe für WinControl über",
  "RS232 oder Ethernet/TCP.",
  "Keine allgemeine Kompatibilitätszusage.",
  "TP-3000 ist ein unabhängiges Projekt.",
  "Keine Verbindung, Unterstützung,",
  "Freigabe, Prüfung oder Zertifizierung",
  "durch AHLBORN oder akrobit.",
  "Keine Logos dieser Unternehmen werden",
  "verwendet.",
  "",
  "Ausführliche Hinweise stehen im",
  "Quellpaket unter LICENSES/",
  "ALMEMO-TRADEMARK-NOTICE.txt."
};

static const char* const infoLicenseLinesEN[] =
{
  "TP-3000 Dew Point Mirror Firmware",
  "Version " VERSION " | 2026-07-11",
  "Copyright (C) 2025/2026 S. Brachtl",
  "",
  "Based on LJ2000M T_2.06c by:",
  "Loftur E. Jonasson",
  "J.G. Holstein",
  "",
  "Project license: GNU GPL v3 only",
  "SPDX-License-Identifier: GPL-3.0-only",
  "Free software: redistribution and",
  "modification permitted under GPLv3-only.",
  "Third-party parts keep their licenses.",
  "This program comes with ABSOLUTELY",
  "NO WARRANTY.",
  "Source code and complete license:",
  "github.com/DK6WT/chilled_mirror",
  "See also LICENSE, README and",
  "THIRD_PARTY_NOTICES in the source.",
  "",
  "ALMEMO is a trademark used by AHLBORN.",
  "AMR WinControl is developed by akrobit",
  "and distributed through AHLBORN.",
  "The names describe only:",
  "serial input from ALMEMO instruments",
  "and V6-format output for WinControl",
  "over RS232 or Ethernet/TCP.",
  "No general compatibility claim.",
  "TP-3000 is an independent project.",
  "No affiliation, support, endorsement,",
  "approval, testing or certification by",
  "AHLBORN or akrobit.",
  "No logos of these companies are used.",
  "",
  "The full notice is included in",
  "LICENSES/ALMEMO-TRADEMARK-NOTICE.txt."
};

static void FLASHMEM infoLicensePrintUtf8(const char* text)
{
  tpTftPrintUtf8(text);
}

static void FLASHMEM infoLicenseDraw(uint8_t firstLine)
{
  const char* const* lines = (ui_language == LANG_EN)
                           ? infoLicenseLinesEN
                           : infoLicenseLinesDE;
  const uint8_t lineCount = (ui_language == LANG_EN)
                          ? (uint8_t)(sizeof(infoLicenseLinesEN) / sizeof(infoLicenseLinesEN[0]))
                          : (uint8_t)(sizeof(infoLicenseLinesDE) / sizeof(infoLicenseLinesDE[0]));
  const uint8_t visibleLines = 15;

  infoLicenseClearArea();
  tft.setFont(DroidSansMono_16);
  tft.setTextColor(YELLOW, BLACK);
  tft.setCursor(160, 32);
  infoLicensePrintUtf8(T(TXT_MENU_INFO_LICENSE));

  char counter[20];
  uint16_t lastVisible16 = (uint16_t)firstLine + visibleLines;
  if (lastVisible16 > lineCount) lastVisible16 = lineCount;
  const uint8_t lastVisible = (uint8_t)lastVisible16;
  snprintf(counter, sizeof(counter), "%u-%u/%u",
           (unsigned)(firstLine + 1),
           (unsigned)lastVisible,
           (unsigned)lineCount);
  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(565, 32);
  tft.print(counter);

  tft.setTextColor(WHITE, BLACK);
  int16_t y = 72;
  for (uint8_t row = 0; row < visibleLines; row++)
  {
    const uint8_t index = (uint8_t)(firstLine + row);
    if (index >= lineCount) break;
    tft.setCursor(160, y);
    infoLicensePrintUtf8(lines[index]);
    y += 22;
  }

  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(160, 418);
  if (ui_language == LANG_EN)
    infoLicensePrintUtf8("UP/DOWN: 2 lines   ENTER/EXIT: back");
  else
    infoLicensePrintUtf8("UP/DOWN: 2 Zeilen  ENTER/EXIT: zurück");
}

void FLASHMEM info_license_menu(void)
{
  static bool frisch = true;
  static uint8_t firstLine = 0;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  FirstBut = 0;
  LastBut  = 3;

  // EXIT wird in ReadButtons() ausgewertet; UP/DOWN/ENTER ueber menuReadKey().
  ReadButtons(false);

  if (!flag.config_mode)
  {
    frisch = true;
    firstLine = 0;
    TouchZ = false;
    return;
  }

  const uint8_t lineCount = (ui_language == LANG_EN)
                          ? (uint8_t)(sizeof(infoLicenseLinesEN) / sizeof(infoLicenseLinesEN[0]))
                          : (uint8_t)(sizeof(infoLicenseLinesDE) / sizeof(infoLicenseLinesDE[0]));
  const uint8_t visibleLines = 15;
  const uint8_t maxFirstLine = (lineCount > visibleLines)
                             ? (uint8_t)(lineCount - visibleLines)
                             : 0;

  uint8_t key = menuReadKey(220);

  if (key == MK_UP)
  {
    if (firstLine > 0)
    {
      firstLine = (firstLine >= 2) ? (uint8_t)(firstLine - 2) : 0;
      flag.menu_lcd_upd = false;
    }
  }
  else if (key == MK_DOWN)
  {
    if (firstLine < maxFirstLine)
    {
      const uint16_t nextLine = (uint16_t)firstLine + 2U;
      firstLine = (nextLine < maxFirstLine) ? (uint8_t)nextLine : maxFirstLine;
      flag.menu_lcd_upd = false;
    }
  }
  else if (key == MK_ENTER)
  {
    infoLicenseClearArea();
    frisch = true;
    firstLine = 0;
    menu_level = MENU_MAIN;
    flag.menu_lcd_upd = false;
    TouchZ = false;
    return;
  }

  if (!flag.menu_lcd_upd || frisch)
  {
    frisch = false;
    flag.menu_lcd_upd = true;

    // Alte TextBox-Inhalte sauber aus dem Setup-Bereich entfernen. Der Inhalt
    // wird direkt mit 16px gezeichnet, damit ausreichend Textzeilen sichtbar
    // bleiben und die normale Menue-TextBox unveraendert bleibt.
    VirtLCDMenu->clear();
    VirtLCDMenu->transfer();
    VirtLCDMessage->clear();
    VirtLCDMessage->transfer();

    infoLicenseDraw(firstLine);
    ReadButtons(true);
  }
}

// =========================================================================
// ZENTRALE HAUPT-VERTEILER-WEICHE

// =========================================================================
// ALARM HAUPTMENUE
// =========================================================================
void FLASHMEM alarm_haupt_menu(void)
{
  static int8_t current_selection = 0;
  static bool untermenue_frisch = true;
  static uint8_t last_mode = 255;

  extern uint8_t alarmGetMode(void);

  if (VirtLCDMenu == nullptr) return;

  const uint8_t mode = alarmGetMode();

  // Im Bereich-Modus sind alle vier Grenzen sichtbar.
  // Im Grenzwert-/Aus-Modus werden nur die beiden oberen Grenzen angeboten,
  // weil nur diese beim Grenzwertalarm ausgewertet werden.
  const TextId alarm_items_limit[] = {
    TXT_ALARM_MODE,
    TXT_ALARM_TAU_HIGH,
    TXT_ALARM_RH_HIGH,
    TXT_ALARM_BUZZER,
    TXT_BACK
  };
  const uint16_t alarm_next_limit[] = {
    MENU_ALARM_MODE,
    MENU_ALARM_TAU_HIGH,
    MENU_ALARM_RH_HIGH,
    MENU_ALARM_BUZZER,
    MENU_MAIN
  };

  const TextId alarm_items_range[] = {
    TXT_ALARM_MODE,
    TXT_ALARM_TAU_LOW,
    TXT_ALARM_TAU_HIGH,
    TXT_ALARM_RH_LOW,
    TXT_ALARM_RH_HIGH,
    TXT_ALARM_BUZZER,
    TXT_BACK
  };
  const uint16_t alarm_next_range[] = {
    MENU_ALARM_MODE,
    MENU_ALARM_TAU_LOW,
    MENU_ALARM_TAU_HIGH,
    MENU_ALARM_RH_LOW,
    MENU_ALARM_RH_HIGH,
    MENU_ALARM_BUZZER,
    MENU_MAIN
  };

  const bool range_mode = (mode == 2);
  const TextId* items = range_mode ? alarm_items_range : alarm_items_limit;
  const uint16_t* next = range_mode ? alarm_next_range : alarm_next_limit;
  const uint8_t menu_size = range_mode ? 7 : 5;

  if (untermenue_frisch || mode != last_mode)
  {
    if (untermenue_frisch)
    {
      current_selection = 0;
      untermenue_frisch = false;
    }

    if (current_selection >= (int8_t)menu_size)
    {
      current_selection = menu_size - 1;
    }

    flag.menu_lcd_upd = false;
    last_mode = mode;
  }

  if (menuRunTextList(TXT_MENU_ALARM_TITLE,
                      items,
                      menu_size,
                      current_selection,
                      3,
                      false,
                      false))
  {
    if (current_selection >= 0 && current_selection < menu_size)
    {
      menu_level = next[current_selection];
    }
    else
    {
      menu_level = MENU_MAIN;
    }

    if (menu_level == MENU_MAIN)
    {
      untermenue_frisch = true;
    }

    flag.menu_lcd_upd = false;
  }
}

// ============================================================================
// STATUS INFORMATION: Optikqualitaet / Auto-Cal-Diagnose
// ============================================================================
extern bool opticHealthIsValid();
extern uint8_t opticHealthGetTotal();
extern uint8_t opticHealthGetLed();
extern uint8_t opticHealthGetTarget();
extern uint8_t opticHealthGetStability();
extern uint8_t opticHealthGetDark();
extern uint8_t opticHealthGetTime();
extern uint8_t opticHealthGetMin();
extern uint8_t opticHealthGetMinReason();
extern uint8_t opticHealthGetStatus();
extern float opticHealthGetLedMA();
extern float opticHealthGetTargetRaw();
extern float opticHealthGetBrutto();
extern float opticHealthGetDarkRaw();
extern float opticHealthGetNetto();
extern float opticHealthGetTrockenRef();
extern float opticHealthGetRestError();
extern float opticHealthGetNoisePp();
extern uint32_t opticHealthGetDurationMs();
extern uint16_t opticHealthGetCoarseSteps();
extern uint16_t opticHealthGetFineSteps();
extern const char* opticHealthStatusTextDE();
extern const char* opticHealthStatusTextEN();

static uint16_t statusInfoQualityColor(uint8_t q)
{
  if (q >= 80) return GREEN;
  if (q >= 60) return YELLOW;
  if (q >= 40) return ORANGE;
  return RED;
}

static const char* statusInfoStatusText()
{
  return (ui_language == LANG_EN) ? opticHealthStatusTextEN() : opticHealthStatusTextDE();
}

static void statusInfoClearArea()
{
  // Status-Information nutzt die Flaeche links der Setup-Buttons.
  // Beim Zurueck mit ENTER blieben rechts am letzten Textfeld noch
  // ca. 10...20 Pixel stehen; deshalb die Loeschflaeche bewusst
  // etwas weiter nach links/rechts ziehen, aber nicht in den
  // Buttonbereich ab x=675 hineinlaufen.
  tft.fillRect(120, 20, 555, 445, BLACK);
}

static void statusInfoPrintPair(int16_t x, int16_t y,
                                const char* label, const char* value,
                                uint16_t valueColor = WHITE)
{
  tft.setFont(DroidSansMono_14);
  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(x, y);
  tpTftPrintUtf8(label);

  tft.setTextColor(valueColor, BLACK);
  tft.setCursor(x + 145, y);
  tpTftPrintUtf8(value);
}

void FLASHMEM status_information_menu(void)
{
  static bool frisch = true;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  FirstBut = 0;
  LastBut  = 3;

  // Wie beim License-Screen: Buttons lesen, aber den Inhalt nicht zyklisch
  // neu zeichnen. ENTER geht zurueck, EXIT wird zentral verarbeitet.
  ReadButtons(false);

  if (!flag.config_mode)
  {
    frisch = true;
    TouchZ = false;
    return;
  }

  uint8_t key = menuReadKey(250);
  if (key == MK_ENTER)
  {
    statusInfoClearArea();
    frisch = true;
    menu_level = MENU_MAIN;
    flag.menu_lcd_upd = false;
    TouchZ = false;
    return;
  }

  // Status-Information ist eine Momentaufnahme. Nur beim Betreten oder nach
  // expliziter Menue-Aktualisierung neu zeichnen, nicht dauerhaft updaten.
  if (flag.menu_lcd_upd && !frisch)
  {
    return;
  }

  frisch = false;
  flag.menu_lcd_upd = true;

  VirtLCDMenu->clear();
  VirtLCDMenu->transfer();
  VirtLCDMessage->clear();
  VirtLCDMessage->transfer();

  statusInfoClearArea();

  tft.setFont(DroidSansMono_16);
  tft.setTextColor(YELLOW, BLACK);
  tft.setCursor(160, 30);
  tft.print("STATUS INFORMATION");

  if (!opticHealthIsValid())
  {
    tft.setFont(DroidSansMono_16);
    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(160, 90);
    tpTftPrintUtf8("Noch keine Auto-Cal-Daten.");
    tft.setCursor(160, 120);
    tpTftPrintUtf8("Bitte Freiheizen/Auto-Cal abwarten.");
    tft.setCursor(160, 405);
    tpTftPrintUtf8("ENTER: Zurück   EXIT: Setup verlassen");
    ReadButtons(true);
    return;
  }

  char v[32];
  int16_t y = 72;
  const int16_t dy = 24;

  snprintf(v, sizeof(v), "%3u %%", (unsigned)opticHealthGetLed());
  statusInfoPrintPair(160, y, "LED-Reserve", v, statusInfoQualityColor(opticHealthGetLed()));
  snprintf(v, sizeof(v), "%5.1f mA", (double)opticHealthGetLedMA());
  statusInfoPrintPair(440, y, "LED-Strom", v); y += dy;

  snprintf(v, sizeof(v), "%3u %%", (unsigned)opticHealthGetTarget());
  statusInfoPrintPair(160, y, "Zielwert", v, statusInfoQualityColor(opticHealthGetTarget()));
  snprintf(v, sizeof(v), "%7ld", (long)opticHealthGetRestError());
  statusInfoPrintPair(440, y, "Restfehler", v); y += dy;

  snprintf(v, sizeof(v), "%3u %%", (unsigned)opticHealthGetStability());
  statusInfoPrintPair(160, y, "Stabilität", v, statusInfoQualityColor(opticHealthGetStability()));
  snprintf(v, sizeof(v), "%7ld", (long)opticHealthGetNoisePp());
  statusInfoPrintPair(440, y, "Rauschen", v); y += dy;

  snprintf(v, sizeof(v), "%3u %%", (unsigned)opticHealthGetDark());
  statusInfoPrintPair(160, y, "Dunkelwert", v, statusInfoQualityColor(opticHealthGetDark()));
  snprintf(v, sizeof(v), "%7ld", (long)opticHealthGetDarkRaw());
  statusInfoPrintPair(440, y, "Dunkel ADC2", v); y += dy;

  snprintf(v, sizeof(v), "%3u %%", (unsigned)opticHealthGetTime());
  statusInfoPrintPair(160, y, "Auto-Cal-Zeit", v, statusInfoQualityColor(opticHealthGetTime()));
  snprintf(v, sizeof(v), "%4.1f s", (double)((float)opticHealthGetDurationMs() / 1000.0f));
  statusInfoPrintPair(440, y, "Dauer", v); y += dy * 2;

  snprintf(v, sizeof(v), "%8ld", (long)opticHealthGetTrockenRef());
  statusInfoPrintPair(160, y, "Trockenref", v);
  snprintf(v, sizeof(v), "%8ld", (long)opticHealthGetNetto());
  statusInfoPrintPair(440, y, "ADC2 netto", v); y += dy;

  snprintf(v, sizeof(v), "%8ld", (long)opticHealthGetBrutto());
  statusInfoPrintPair(160, y, "Brutto", v);
  snprintf(v, sizeof(v), "%u / %u", (unsigned)opticHealthGetCoarseSteps(), (unsigned)opticHealthGetFineSteps());
  statusInfoPrintPair(440, y, "Schritte", v); y += dy;

  snprintf(v, sizeof(v), "%8ld", (long)opticHealthGetTargetRaw());
  statusInfoPrintPair(160, y, "ADC2 Ziel", v);
  snprintf(v, sizeof(v), "%3u %%", (unsigned)opticHealthGetMin());
  statusInfoPrintPair(440, y, "Schw. Wert", v, statusInfoQualityColor(opticHealthGetMin()));

  // Gesamtergebnis unten gross.
  uint8_t total = opticHealthGetTotal();
  uint16_t c = statusInfoQualityColor(total);
  tft.drawFastHLine(150, 365, 500, DKBLUE);
  tft.setFont(DroidSansMono_20);
  tft.setTextColor(c, BLACK);
  tft.setCursor(160, 382);
  snprintf(v, sizeof(v), "GESAMT: %3u %%", (unsigned)total);
  tft.print(v);
  tft.setCursor(405, 382);
  tpTftPrintUtf8(statusInfoStatusText());

  tft.setFont(DroidSansMono_14);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(160, 430);
  tpTftPrintUtf8("ENTER: Zurück   EXIT: Setup verlassen");

  ReadButtons(true);
}
