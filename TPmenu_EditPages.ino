/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPmenu_EditPages.ino
 * Zweck: Numerische Einstellseiten, aus dem historischen Menuesystem ausgelagert.
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
// TPmenu_EditPages.ino
// numerische Einstellseiten - ausgelagert aus TPmenu.ino
// =========================================================================

// DISPLAY DIMMER - SAFE / APPLY ON SAVE
// =========================================================================
// Stufen: 0..10
// realer RA8875-Backlightwert: 5..230
// Wichtig fuer RA8875-Stabilitaet:
// - Beim Betreten der Seite wird die Helligkeit NICHT neu gesetzt.
// - UP/DOWN aendert nur den angezeigten Sollwert.
// - tft.brightness() wird genau einmal beim Speichern/ENTER gesetzt.

static uint8_t FLASHMEM displayDimmerClampLevel(uint8_t level)
{
  if (level > 10) level = 10;
  return level;
}

static uint8_t FLASHMEM displayDimmerLevelToBrightness(uint8_t level)
{
  level = displayDimmerClampLevel(level);

  // Stufe 0 ist sehr dunkel, aber nicht ganz aus.
  // Stufe 10 bleibt unter dem harten Maximum.
  return (uint8_t)map(level, 0, 10, 5, 230);
}

static void FLASHMEM displayDimmerApply(void)
{
  R.display_konfig.tft_backlight =
    displayDimmerClampLevel(R.display_konfig.tft_backlight);

  tft.brightness(displayDimmerLevelToBrightness(R.display_konfig.tft_backlight));
}

void FLASHMEM displaydim_menu(void)
{
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;
  static uint8_t edit_level = 7;
  static uint8_t last_level = 255;
  static uint16_t last_pwm = 65535;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    R.display_konfig.tft_backlight =
      displayDimmerClampLevel(R.display_konfig.tft_backlight);

    edit_level = R.display_konfig.tft_backlight;

    // Absichtlich KEIN displayDimmerApply() beim Betreten der Seite.
    // Die Helligkeit ist bereits gesetzt; ein unnoetiger RA8875-PWM-Zugriff
    // waehrend Seitenaufbau/Button-Redraw kann Grafikfehler verursachen.

    page_drawn = false;
    last_level = 255;
    last_pwm = 65535;
    flag.menu_lcd_upd = false;
    frisch = false;
  }

  // Normale Bedienung wie bei den anderen Wert-Menues.
  // on_change ist absichtlich nullptr: UP/DOWN aendert nur den Sollwert,
  // nicht die echte RA8875-Helligkeit.
  if (menuValueHandleUInt8(edit_level,
                           0,
                           10,
                           1,
                           nullptr,
                           250))
  {
    R.display_konfig.tft_backlight = displayDimmerClampLevel(edit_level);

    // Helligkeit erst beim Speichern setzen, dann EEPROM schreiben
    // und zurueck ins vorherige Menue.
    displayDimmerApply();

    frisch = true;
    page_drawn = false;
    last_level = 255;
    last_pwm = 65535;
    menuValueSaveAndReturn(MENU_MAIN, true);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    uint16_t pwm_now = displayDimmerLevelToBrightness(edit_level);

    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_MENU_DISPLAY_DIMMER_TITLE, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print(T(TXT_DIMMER_HINT));

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "Level:" : "Stufe:");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print("PWM:");

      VirtLCDMenu->setCursor(1, 10);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_level = 255;
      last_pwm = 65535;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_level != edit_level)
    {
      snprintf(lcd_buf, 255, "%2u / 10", edit_level);
      menuDirectWriteFixedRow(0, 8, 6, lcd_buf, 7, WHITE);
      last_level = edit_level;
    }

    if (last_pwm != pwm_now)
    {
      snprintf(lcd_buf, 255, "%3u", pwm_now);
      menuDirectWriteFixedRow(1, 6, 8, lcd_buf, 3, WHITE);
      last_pwm = pwm_now;
    }

    ReadButtons(true);
  }
}

// =========================================================================
// RTC STELLEN
// =========================================================================
void FLASHMEM rtc_stellen_menu(void)
{
  const int16_t RTC_MENU_YEAR_MIN = 2026;
  const int16_t RTC_MENU_YEAR_MAX = 2079;
  const int16_t RTC_MENU_YEAR_DEFAULT = 2026;

  static int16_t s_tag = 1;
  static int16_t s_monat = 1;
  static int16_t s_jahr = RTC_MENU_YEAR_DEFAULT;
  static int16_t s_stunde = 12;
  static int16_t s_minute = 0;

  static uint8_t einstell_schritt = 0;
  static bool uhr_frisch = true;
  static MenuPageDrawFlag page_drawn;
  static uint8_t last_step = 255;

  extern uint32_t menu_global_debounce;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  if (uhr_frisch)
  {
    s_tag = day();
    s_monat = month();
    s_jahr = year();
    s_stunde = hour();
    s_minute = minute();

    // Wenn TimeLib nach einer unplausiblen RTC-Lesung noch auf 1970 steht
    // oder ein alter RTC-Fehler 2074 geliefert hat, nicht mit diesem Wert
    // ins Einstellmenue gehen.
    if (s_jahr < RTC_MENU_YEAR_MIN || s_jahr > RTC_MENU_YEAR_MAX)
    {
      s_jahr = RTC_MENU_YEAR_DEFAULT;
    }

    einstell_schritt = 0;
    uhr_frisch = false;
    page_drawn = false;
    last_step = 255;
    flag.menu_lcd_upd = false;
  }

  ReadButtons(false);

  uint8_t key = menuReadKey(250);

  if (key != MK_NONE)
  {
    // UP
    if (key == MK_UP)
    {
      if (einstell_schritt == 0) { s_tag++;    if (s_tag > 31) s_tag = 1; }
      if (einstell_schritt == 1) { s_monat++;  if (s_monat > 12) s_monat = 1; }
      if (einstell_schritt == 2) { s_jahr++;   if (s_jahr > RTC_MENU_YEAR_MAX) s_jahr = RTC_MENU_YEAR_MIN; }
      if (einstell_schritt == 3) { s_stunde++; if (s_stunde > 23) s_stunde = 0; }
      if (einstell_schritt == 4) { s_minute++; if (s_minute > 59) s_minute = 0; }

      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }

    // DOWN
    else if (key == MK_DOWN)
    {
      if (einstell_schritt == 0) { s_tag--;    if (s_tag < 1) s_tag = 31; }
      if (einstell_schritt == 1) { s_monat--;  if (s_monat < 1) s_monat = 12; }
      if (einstell_schritt == 2) { s_jahr--;   if (s_jahr < RTC_MENU_YEAR_MIN) s_jahr = RTC_MENU_YEAR_MAX; }
      if (einstell_schritt == 3) { s_stunde--; if (s_stunde < 0) s_stunde = 23; }
      if (einstell_schritt == 4) { s_minute--; if (s_minute < 0) s_minute = 59; }

      flag.menu_lcd_upd = false;
      menu_global_debounce = millis();
    }

    // ENTER
    else if (key == MK_ENTER)
    {
      if (einstell_schritt < 4)
      {
        einstell_schritt++;
        page_drawn = false;
        flag.menu_lcd_upd = false;
        menu_global_debounce = millis();
      }
      else
      {
        menuDirectClearAllRows();

        setTime(s_stunde, s_minute, 0, s_tag, s_monat, s_jahr);
        schreibeEchtzeitUhr();

        tft.fillScreen(BLACK);
        delayMicroseconds(200);

        eraseDisplay();

        menu_level = MENU_MAIN;
        einstell_schritt = 0;
        uhr_frisch = true;
        page_drawn = false;
        last_step = 255;

        flag.config_mode = false;
        flag.mode_change = true;
        flag.menu_lcd_upd = true;

        FirstBut = -1;
        LastBut = -1;

        frisch_geoeffnet = true;
        flag.short_push = false;

        TouchZ = false;
        TouchX = 0;
        TouchY = 0;

        menu_global_debounce = millis();
      }
    }
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn || last_step != einstell_schritt)
    {
      flag.menu_lcd_upd = true;

      menuDirectClearAllRows();

      VirtLCDMenu->clear();
      VirtLCDMessage->clear();

      VirtLCDMenu->setCursor(1, 1);
      VirtLCDMenu->print(T(TXT_MENU_RTC_TITLE));

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print(T(TXT_RTC_ADJUST));

      if (einstell_schritt == 0) VirtLCDMenu->print(T(TXT_RTC_STEP_DAY));
      if (einstell_schritt == 1) VirtLCDMenu->print(T(TXT_RTC_STEP_MONTH));
      if (einstell_schritt == 2) VirtLCDMenu->print(T(TXT_RTC_STEP_YEAR));
      if (einstell_schritt == 3) VirtLCDMenu->print(T(TXT_RTC_STEP_HOUR));
      if (einstell_schritt == 4) VirtLCDMenu->print(T(TXT_RTC_STEP_MIN_SAVE));

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "Date:" : "Datum:");
      VirtLCDMenu->setCursor(21, 4);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "Time:" : "Zeit:");

      VirtLCDMenu->setCursor(1, 7);
      VirtLCDMenu->print(T(TXT_RTC_NEXT));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_step = einstell_schritt;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    snprintf(lcd_buf, 255, "%02d.%02d.%04d", s_tag, s_monat, s_jahr);
    menuDirectWriteFixedRow(0, 8, 4, lcd_buf, 10, WHITE);

    snprintf(lcd_buf, 255, "%02d:%02d", s_stunde, s_minute);
    menuDirectWriteFixedRow(1, 27, 4, lcd_buf, 5, WHITE);

    ReadButtons(true);
  }
}

// =========================================================================
// PID KP - numerisch, 1er-Schritte
// =========================================================================
void FLASHMEM pid_kp_menu(void)
{
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;
  static uint16_t last_kp = 65535;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    R.pid_kp = constrain(R.pid_kp, 0, 1000);
    frisch = false;
    page_drawn = false;
    last_kp = 65535;
    flag.menu_lcd_upd = false;
  }

  if (menuValueHandleUInt16(R.pid_kp,
                            0,
                            1000,
                            1,
                            nullptr,
                            180))
  {
    frisch = true;
    page_drawn = false;
    last_kp = 65535;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_PID_KP_TITLE, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print("Kp:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print(T(TXT_PID_UPDOWN_1));

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_kp = 65535;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_kp != R.pid_kp)
    {
      snprintf(lcd_buf, 255, "%4u", R.pid_kp);
      menuDirectWriteFixedRow(0, 5, 4, lcd_buf, 4, WHITE);
      last_kp = R.pid_kp;
    }

    ReadButtons(true);
  }
}

// =========================================================================
// PID KI - numerisch, 0.1er-Schritte
// =========================================================================
void FLASHMEM pid_ki_menu(void)
{
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;
  static int16_t last_ki10 = -32768;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    if (R.pid_ki < 0.0f) R.pid_ki = 0.0f;
    if (R.pid_ki > 50.0f) R.pid_ki = 50.0f;
    frisch = false;
    page_drawn = false;
    last_ki10 = -32768;
    flag.menu_lcd_upd = false;
  }

  if (menuValueHandleFloat(R.pid_ki,
                           0.0f,
                           50.0f,
                           0.1f,
                           nullptr,
                           180))
  {
    frisch = true;
    page_drawn = false;
    last_ki10 = -32768;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    int16_t ki10 = (int16_t)(R.pid_ki * 10.0f + 0.5f);

    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_PID_KI_TITLE, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print("Ki:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "UP/DOWN: 0.1 / 1 / 5" : "UP/DOWN: 0,1 / 1 / 5");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_ki10 = -32768;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_ki10 != ki10)
    {
      snprintf(lcd_buf, 255, "%4.1f", (double)R.pid_ki);
      menuDirectWriteFixedRow(0, 5, 4, lcd_buf, 4, WHITE);
      last_ki10 = ki10;
    }

    ReadButtons(true);
  }
}

// =========================================================================
// PID KD - numerisch, 0.1er-Schritte
// =========================================================================
void FLASHMEM pid_kd_menu(void)
{
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;
  static int16_t last_kd10 = -32768;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    if (R.pid_kd < 0.0f) R.pid_kd = 0.0f;
    if (R.pid_kd > 50.0f) R.pid_kd = 50.0f;
    frisch = false;
    page_drawn = false;
    last_kd10 = -32768;
    flag.menu_lcd_upd = false;
  }

  if (menuValueHandleFloat(R.pid_kd,
                           0.0f,
                           50.0f,
                           0.1f,
                           nullptr,
                           180))
  {
    frisch = true;
    page_drawn = false;
    last_kd10 = -32768;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    int16_t kd10 = (int16_t)(R.pid_kd * 10.0f + 0.5f);

    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_PID_KD_TITLE, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print("Kd:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "UP/DOWN: 0.1 / 1 / 5" : "UP/DOWN: 0,1 / 1 / 5");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_kd10 = -32768;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_kd10 != kd10)
    {
      snprintf(lcd_buf, 255, "%4.1f", (double)R.pid_kd);
      menuDirectWriteFixedRow(0, 5, 4, lcd_buf, 4, WHITE);
      last_kd10 = kd10;
    }

    ReadButtons(true);
  }
}

// =========================================================================
// REGELTAKT
// =========================================================================
static const uint16_t regler_takt_values[] = { 62, 100, 120, 142, 180, 250 };
static const char *regler_takt_items[] = {
  "62 ms (16 Hz)",
  "100 ms (10 Hz)",
  "120 ms (8 Hz)",
  "142 ms (7 Hz)",
  "180 ms (5.5 Hz)",
  "250 ms (4 Hz)"
};
static const uint8_t regler_takt_size = sizeof(regler_takt_values) / sizeof(regler_takt_values[0]);

static int8_t FLASHMEM reglerTaktSelectionFromValue(uint16_t value)
{
  int8_t best_index = 0;
  uint16_t best_diff = 0xFFFF;

  for (uint8_t i = 0; i < regler_takt_size; i++)
  {
    uint16_t candidate = regler_takt_values[i];
    uint16_t diff = (candidate > value) ? (candidate - value) : (value - candidate);

    if (diff < best_diff)
    {
      best_diff = diff;
      best_index = (int8_t)i;
    }
  }

  return best_index;
}

void FLASHMEM regler_takt_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    current_selection = reglerTaktSelectionFromValue(R.regler_intervall_ms);
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, regler_takt_size, 250))
  {
    R.regler_intervall_ms = regler_takt_values[current_selection];
    frisch = true;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    menuValueBeginDraw(TXT_MENU_CONTROL_PERIOD_TITLE, false);

    lcd_scroll_Menu(regler_takt_items, regler_takt_size, current_selection, 4, 1, 5);

    menuValueEndDraw();
  }
}




// =========================================================================
// AUTO-CAL INTERVALL - Auswahl 10 min bis 3 h
// =========================================================================
static const uint32_t autocal_interval_values_ms[] = {
  10UL  * 60UL * 1000UL,
  15UL  * 60UL * 1000UL,
  30UL  * 60UL * 1000UL,
  60UL  * 60UL * 1000UL,
  120UL * 60UL * 1000UL,
  180UL * 60UL * 1000UL
};

static const char *autocal_interval_items_de[] = {
  "10 min",
  "15 min",
  "30 min",
  "60 min",
  "120 min",
  "180 min"
};

static const char *autocal_interval_items_en[] = {
  "10 min",
  "15 min",
  "30 min",
  "60 min",
  "120 min",
  "180 min"
};

static const uint8_t autocal_interval_size = sizeof(autocal_interval_values_ms) / sizeof(autocal_interval_values_ms[0]);

uint32_t optikAutoCalIntervalMs()
{
  if (R.optik_autocal_interval_index >= autocal_interval_size)
  {
    return autocal_interval_values_ms[OPTIK_AUTOCAL_INTERVAL_DEFAULT_INDEX];
  }
  return autocal_interval_values_ms[R.optik_autocal_interval_index];
}

void FLASHMEM autocal_interval_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    if (R.optik_autocal_interval_index >= autocal_interval_size)
    {
      R.optik_autocal_interval_index = OPTIK_AUTOCAL_INTERVAL_DEFAULT_INDEX;
    }

    current_selection = (int8_t)R.optik_autocal_interval_index;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, autocal_interval_size, 250))
  {
    R.optik_autocal_interval_index = (uint8_t)current_selection;
    frisch = true;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    menuValueBeginDraw(TXT_AUTOCAL_INTERVAL_TITLE, false);

    const char **items = (ui_language == LANG_EN) ? autocal_interval_items_en : autocal_interval_items_de;
    lcd_scroll_Menu(items, autocal_interval_size, current_selection, 4, 1, 5);

    menuValueEndDraw();
  }
}



// =========================================================================
// LED-AUTOADAPTION - Aus / Grundkurve / selbstlernend
// Lernen aus erfolgreichen Auto-Cals laeuft bei vorhandener SD in allen Modi.
// Ohne SD bleiben AUS und EIN-GRUNDKURVE verfuegbar; nur SELBSTLERNEND ist gesperrt.
// =========================================================================
extern bool ledAdaptationSdAvailable(void);
extern uint8_t ledAdaptationModeGet(void);
extern bool ledAdaptationSetMode(uint8_t mode);
extern bool ledAdaptationResetCurrentHead(char* message, size_t messageSize);
extern uint32_t ledAdaptationAcceptedCount(void);
extern uint16_t ledAdaptationValidBinCount(void);

void FLASHMEM led_autoadaptation_mode_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;
  static bool sdAvailable = false;

  if (VirtLCDMenu == nullptr) return;
  if (frisch) sdAvailable = ledAdaptationSdAvailable();

  const char* itemsDeAll[] = { "Aus", "Ein - Grundkurve", "Selbstlernend" };
  const char* itemsEnAll[] = { "Off", "On - base curve", "Self-learning" };
  const char* itemsNoSdDe[] = { "Aus", "Ein - Grundkurve" };
  const char* itemsNoSdEn[] = { "Off", "On - base curve" };
  const char** items = nullptr;
  uint8_t itemCount = 0U;

  if (sdAvailable)
  {
    items = (ui_language == LANG_EN) ? itemsEnAll : itemsDeAll;
    itemCount = 3U;
  }
  else
  {
    items = (ui_language == LANG_EN) ? itemsNoSdEn : itemsNoSdDe;
    itemCount = 2U;
  }

  if (frisch)
  {
    current_selection = (int8_t)ledAdaptationModeGet();
    if (current_selection < 0 || current_selection >= (int8_t)itemCount) current_selection = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, itemCount, 250))
  {
    const uint8_t selectedMode = (uint8_t)current_selection;
    (void)ledAdaptationSetMode(selectedMode);
    frisch = true;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
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
    VirtLCDMenu->print(ui_language == LANG_EN ? "LED AUTO-ADAPTATION" : "LED-AUTOADAPTION");
    VirtLCDMenu->setCursor(1, 2);
    if (sdAvailable)
    {
      snprintf(lcd_buf, 255, ui_language == LANG_EN ?
               "Learning always active: n=%lu, bins=%u" :
               "Lernen immer aktiv: n=%lu, Klassen=%u",
               (unsigned long)ledAdaptationAcceptedCount(),
               (unsigned)ledAdaptationValidBinCount());
      VirtLCDMenu->print(lcd_buf);
    }
    else
    {
      VirtLCDMenu->print(ui_language == LANG_EN ?
                         "No SD: base curve available" :
                         "Keine SD: Grundkurve verfuegbar");
    }
    lcd_scroll_Menu(items, itemCount, current_selection, 4, 1, 5);
    VirtLCDMenu->transfer();
    ReadButtons(true);
  }
}


// =========================================================================
// LED-LERNDATEN ZURUECKSETZEN - ausschliesslich aktueller Kopf
// =========================================================================
void FLASHMEM led_learning_reset_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;
  static bool sdAvailable = false;

  if (VirtLCDMenu == nullptr) return;
  if (frisch) sdAvailable = ledAdaptationSdAvailable();

  const char* itemsDe[] = { "Nein / zurück", "JA - aktuellen Kopf löschen" };
  const char* itemsEn[] = { "No / back", "YES - delete current head" };
  const char* noSdDe[] = { "Zurück" };
  const char* noSdEn[] = { "Back" };
  const char** items = sdAvailable ?
                       ((ui_language == LANG_EN) ? itemsEn : itemsDe) :
                       ((ui_language == LANG_EN) ? noSdEn : noSdDe);
  const uint8_t itemCount = sdAvailable ? 2U : 1U;

  if (frisch)
  {
    current_selection = 0; // sichere Vorgabe: Nein
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, itemCount, 250))
  {
    if (sdAvailable && current_selection == 1)
    {
      char message[128] = {0};
      const bool ok = ledAdaptationResetCurrentHead(message, sizeof(message));
      if (VirtLCDMessage)
      {
        VirtLCDMessage->clear();
        VirtLCDMessage->setCursor(1, 1);
        VirtLCDMessage->print(message[0] ? message : (ok ? "OK" : "Fehler"));
      }
    }
    frisch = true;
    menu_level = MENU_CONTROL_PARAMETERS;
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
    VirtLCDMenu->print(ui_language == LANG_EN ?
                       "RESET LED LEARNING DATA" :
                       "LED-LERNDATEN LÖSCHEN");
    VirtLCDMenu->setCursor(1, 2);
    if (sdAvailable)
    {
      snprintf(lcd_buf, 255, "%s K%05lu",
               headTypeTextGet(), (unsigned long)(R.head_serial % 100000UL));
      VirtLCDMenu->print(lcd_buf);
      VirtLCDMenu->setCursor(1, 3);
      VirtLCDMenu->print(ui_language == LANG_EN ?
                         "Only this selected head" :
                         "Nur dieser gewählte Kopf");
    }
    else
    {
      VirtLCDMenu->print(ui_language == LANG_EN ?
                         "No SD card" : "Keine SD-Karte");
    }
    lcd_scroll_Menu(items, itemCount, current_selection, 5, 1, 4);
    VirtLCDMenu->transfer();
    ReadButtons(true);
  }
}


// =========================================================================
// ADC/PT100-MESSFILTER - Normal / Auto / Praezision
// =========================================================================
static const char *adc_filter_mode_items_de[] = {
  "Normal",
  "Auto",
  "Präzision"
};

static const char *adc_filter_mode_items_en[] = {
  "Normal",
  "Auto",
  "Precision"
};

static const uint8_t adc_filter_mode_size = 3;

void FLASHMEM adc_filter_mode_menu(void)
{
  static int8_t current_selection = ADC_MEAS_FILTER_DEFAULT;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    current_selection = (int8_t)adcFilterModeGet();
    if (current_selection < 0 || current_selection >= adc_filter_mode_size)
    {
      current_selection = ADC_MEAS_FILTER_DEFAULT;
    }
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, adc_filter_mode_size, 250))
  {
    adcFilterModeSet((uint8_t)current_selection);
    frisch = true;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    menuValueBeginDraw(TXT_ADC_FILTER_MODE_TITLE, false);

    const char **items = (ui_language == LANG_EN) ? adc_filter_mode_items_en : adc_filter_mode_items_de;
    lcd_scroll_Menu(items, adc_filter_mode_size, current_selection, 4, 1, 5);

    menuValueEndDraw();
  }
}


// =========================================================================
// ADC1 / ADS1263-SFOCAL - Start / Auto-Cal synchron / 10/30/60 min
// =========================================================================
static const char *adc1_sfocal_mode_items_de[] = {
  "Nur beim Start",
  "Synchron mit Auto-Cal",
  "Alle 10 min",
  "Alle 30 min",
  "Alle 60 min"
};

static const char *adc1_sfocal_mode_items_en[] = {
  "Start only",
  "Sync auto-cal",
  "Every 10 min",
  "Every 30 min",
  "Every 60 min"
};

static const uint8_t adc1_sfocal_mode_size = 5;

void FLASHMEM adc1_sfocal_mode_menu(void)
{
  static int8_t current_selection = ADC1_SFOCAL_DEFAULT;
  static bool frisch = true;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    current_selection = (int8_t)adc1SfocalModeGet();
    if (current_selection < 0 || current_selection >= adc1_sfocal_mode_size)
    {
      current_selection = ADC1_SFOCAL_DEFAULT;
    }
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, adc1_sfocal_mode_size, 250))
  {
    adc1SfocalModeSet((uint8_t)current_selection);
    frisch = true;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    menuValueBeginDraw(TXT_ADC1_SFOCAL_MODE_TITLE, false);

    const char **items = (ui_language == LANG_EN) ? adc1_sfocal_mode_items_en : adc1_sfocal_mode_items_de;
    lcd_scroll_Menu(items, adc1_sfocal_mode_size, current_selection, 4, 1, 5);

    menuValueEndDraw();
  }
}


// =========================================================================
// OPTIK-ZIELWERT - numerisch, 0.1 %-Schritte
// =========================================================================
void FLASHMEM optik_sollwert_menu(void)
{
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;
  static uint16_t last_optik = 65535;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    if (R.optik_sollwert < 800 || R.optik_sollwert > 990)
    {
      R.optik_sollwert = 985;
    }

    frisch = false;
    page_drawn = false;
    last_optik = 65535;
    flag.menu_lcd_upd = false;
  }

  if (menuValueHandleUInt16(R.optik_sollwert,
                            800,
                            990,
                            1,
                            nullptr,
                            180))
  {
    frisch = true;
    page_drawn = false;
    last_optik = 65535;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_OPTIK_TARGET_TITLE, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "Target:" : "Optik:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "UP/DOWN: 0.1 / 1 / 5" : "UP/DOWN: 0,1 / 1 / 5");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_optik = 65535;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_optik != R.optik_sollwert)
    {
      snprintf(lcd_buf,
               255,
               "%3u.%u %%",
               (unsigned)(R.optik_sollwert / 10),
               (unsigned)(R.optik_sollwert % 10));
      menuDirectWriteFixedRow(0, 9, 4, lcd_buf, 7, WHITE);
      last_optik = R.optik_sollwert;
    }

    ReadButtons(true);
  }
}

// =========================================================================
// FREIHEIZTEMPERATUR - numerisch, 1 °C-Schritte
// =========================================================================
void FLASHMEM freiheiz_temp_menu(void)
{
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;
  static uint8_t last_temp = 255;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    if (R.freiheiz_ziel_temp[0] < 40 || R.freiheiz_ziel_temp[0] > 76)
    {
      R.freiheiz_ziel_temp[0] = 76;
    }

    frisch = false;
    page_drawn = false;
    last_temp = 255;
    flag.menu_lcd_upd = false;
  }

  if (menuValueHandleUInt8(R.freiheiz_ziel_temp[0],
                           40,
                           76,
                           1,
                           nullptr,
                           180))
  {
    frisch = true;
    page_drawn = false;
    last_temp = 255;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_FREIHEIZ_TEMP_TITLE, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "Heat-clean:" : "Freiheizen:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print(T(TXT_PID_UPDOWN_1));

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_temp = 255;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_temp != R.freiheiz_ziel_temp[0])
    {
      snprintf(lcd_buf, 255, "%2u \xB0""C", R.freiheiz_ziel_temp[0]);
      menuDirectWriteFixedRow(0, 14, 4, lcd_buf, 4, WHITE);
      last_temp = R.freiheiz_ziel_temp[0];
    }

    ReadButtons(true);
  }
}


void FLASHMEM fan_speed_menu(void)
{
  static MenuPageDrawFlag page_drawn;
  static uint8_t last_percent = 255;
  static uint16_t last_pwm = 65535;

  if (VirtLCDMenu == nullptr) return;

  if (R.fan_percent < 50 || R.fan_percent > 100)
  {
    R.fan_percent = 60;
    fanApplyNormalSpeed();
    flag.menu_lcd_upd = false;
  }

  if (menuValueHandleUInt8(R.fan_percent,
                           50,
                           100,
                           1,
                           fanApplyNormalSpeed,
                           180))
  {
    fanApplyNormalSpeed();
    if (page_drawn)
    {
      menuDirectClearFanRows();
    }
    page_drawn = false;
    last_percent = 255;
    last_pwm = 65535;
    menuValueSaveAndReturn(MENU_MAIN, true);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    uint16_t pwm_now = fanPercentToPwm(R.fan_percent);

    // Erster Aufbau bleibt normal. Danach werden beim schnellen Verstellen
    // nur noch die zwei Zahlenzeilen als feste Felder aktualisiert.
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_FAN_SPEED_TITLE, true);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "Fan:" : "Lüfter:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print("PWM:");

      VirtLCDMenu->setCursor(1, 9);
      VirtLCDMenu->print(T(TXT_FAN_UPDOWN));

      VirtLCDMenu->setCursor(1, 10);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_percent = 255;
      last_pwm = 65535;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_percent != R.fan_percent)
    {
      snprintf(lcd_buf, 255, "%3u %%", R.fan_percent);
      menuDirectWriteFixedRow(0, 10, 4, lcd_buf, 5, WHITE);
      last_percent = R.fan_percent;
    }

    if (last_pwm != pwm_now)
    {
      snprintf(lcd_buf, 255, "%4u / 4095", pwm_now);
      menuDirectWriteFixedRow(1, 6, 6, lcd_buf, 11, WHITE);
      last_pwm = pwm_now;
    }

    ReadButtons(true);
  }
}



// =========================================================================
// ALARM - Einstellseiten
// =========================================================================
extern uint8_t alarmGetMode(void);
extern void alarmSetMode(uint8_t mode);
extern bool alarmBuzzerEnabled(void);
extern void alarmSetBuzzerEnabled(bool enabled);
extern int16_t alarmGetTauLow10(void);
extern int16_t alarmGetTauHigh10(void);
extern uint16_t alarmGetRhLow10(void);
extern uint16_t alarmGetRhHigh10(void);
extern void alarmSetTauLow10(int16_t value10);
extern void alarmSetTauHigh10(int16_t value10);
extern void alarmSetRhLow10(uint16_t value10);
extern void alarmSetRhHigh10(uint16_t value10);
extern void alarmConfigSave(void);
extern void alarmResetAcknowledge(void);

static int16_t FLASHMEM alarmFloatTo10(float value)
{
  return (int16_t)(value * 10.0f + ((value >= 0.0f) ? 0.5f : -0.5f));
}

static uint16_t FLASHMEM alarmFloatToU10(float value)
{
  if (value < 0.0f) value = 0.0f;
  return (uint16_t)(value * 10.0f + 0.5f);
}

static uint8_t alarmRawTouchKey(void)
{
  if (!TouchZ) return MK_NONE;

  if (TouchX > 20 && TouchX < 130 && TouchY > 25 && TouchY < 145)
  {
    return MK_UP;
  }

  if (TouchX > 20 && TouchX < 130 && TouchY > 187 && TouchY < 307)
  {
    return MK_DOWN;
  }

  if (TouchX > 20 && TouchX < 130 && TouchY > 350 && TouchY < 470)
  {
    return MK_ENTER;
  }

  return MK_NONE;
}

static bool FLASHMEM alarmValueHandleFloatFast(float& value,
                                               float min_value,
                                               float max_value,
                                               float base_step,
                                               void (*on_change)(void),
                                               uint16_t debounce_ms)
{
  static uint8_t hold_key = MK_NONE;
  static uint32_t hold_start_ms = 0;

  ReadButtons(false);

  const uint32_t now = millis();
  const uint8_t raw_key = alarmRawTouchKey();

  if (raw_key != hold_key)
  {
    hold_key = raw_key;
    hold_start_ms = now;
  }

  uint8_t key = menuReadKey(debounce_ms);

  if (key == MK_UP || key == MK_DOWN)
  {
    float effective_step = base_step;

    if (hold_key == key)
    {
      const uint32_t held_ms = now - hold_start_ms;

      // Alarmgrenzen haben grosse Bereiche. Kurz bleibt 0,1 fein,
      // langes Halten beschleunigt auf 1,0 und danach 5,0 pro Schritt.
      if (held_ms > 6400UL)
      {
        effective_step = 5.0f;
      }
      else if (held_ms > 3200UL)
      {
        effective_step = 1.0f;
      }
    }

    if (key == MK_UP)
    {
      value += effective_step;
      if (value > max_value) value = max_value;
    }
    else
    {
      value -= effective_step;
      if (value < min_value) value = min_value;
    }

    if (on_change != nullptr) on_change();
    flag.menu_lcd_upd = false;
  }
  else if (key == MK_ENTER)
  {
    hold_key = MK_NONE;
    flag.short_push = false;
    return true;
  }

  return false;
}

static void FLASHMEM alarmReturnToMenu(void)
{
  extern uint32_t menu_global_debounce;

  menuDirectClearAllRows();
  menu_level = MENU_ALARM;
  flag.short_push = false;
  flag.menu_lcd_upd = false;
  menuValueConsumeTouch();
  menu_global_debounce = millis();
}

void FLASHMEM alarm_mode_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  const char* mode_items[3] = {
    T(TXT_ALARM_MODE_OFF),
    T(TXT_ALARM_MODE_LIMIT),
    T(TXT_ALARM_MODE_RANGE)
  };

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    current_selection = (int8_t)alarmGetMode();
    if (current_selection < 0 || current_selection > 2) current_selection = 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, 3, 250))
  {
    alarmSetMode((uint8_t)current_selection);
    alarmConfigSave();
    alarmResetAcknowledge();
    frisch = true;
    alarmReturnToMenu();
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    menuValueBeginDraw(TXT_ALARM_MODE_TITLE, false);
    lcd_scroll_Menu(mode_items, 3, current_selection, 4, 1, 5);
    menuValueEndDraw();
  }
}

void FLASHMEM alarm_buzzer_menu(void)
{
  static int8_t current_selection = 0;
  static bool frisch = true;

  const char* buzzer_items[2] = {
    T(TXT_ALARM_BUZZER_OFF),
    T(TXT_ALARM_BUZZER_ON)
  };

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    current_selection = alarmBuzzerEnabled() ? 1 : 0;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (menuListHandleInput(current_selection, 2, 250))
  {
    alarmSetBuzzerEnabled(current_selection != 0);
    alarmConfigSave();
    alarmResetAcknowledge();
    frisch = true;
    alarmReturnToMenu();
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    menuValueBeginDraw(TXT_ALARM_BUZZER_TITLE, false);
    lcd_scroll_Menu(buzzer_items, 2, current_selection, 4, 1, 5);
    menuValueEndDraw();
  }
}

static bool FLASHMEM alarmFloatValuePage(TextId title,
                                const char* label,
                                float& edit_value,
                                float min_value,
                                float max_value,
                                float step,
                                uint8_t value_col,
                                void (*saveScaled)(float),
                                bool temperature_unit)
{
  static MenuPageDrawFlag page_drawn;
  static int16_t last_value10 = -32768;

  if (alarmValueHandleFloatFast(edit_value,
                                min_value,
                                max_value,
                                step,
                                nullptr,
                                100))
  {
    if (saveScaled != nullptr) saveScaled(edit_value);
    alarmConfigSave();
    alarmResetAcknowledge();
    page_drawn = false;
    last_value10 = -32768;
    alarmReturnToMenu();
    return true;
  }

  if (!flag.menu_lcd_upd)
  {
    int16_t value10 = alarmFloatTo10(edit_value);

    if (!page_drawn)
    {
      menuValueBeginDraw(title, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print(label);

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "UP/DOWN: 0.1 / 1 / 5" : "UP/DOWN: 0,1 / 1 / 5");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->setCursor(1, 10);
      VirtLCDMenu->print(T(TXT_ALARM_TOUCH_ACK));

      VirtLCDMenu->setCursor(1, 11);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "acknowledge buzzer" : "Buzzer quittieren");

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_value10 = -32768;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_value10 != value10)
    {
      if (temperature_unit)
      {
        snprintf(lcd_buf, 255, "%6.1f \xB0""C", (double)edit_value);
        menuDirectWriteFixedRow(0, value_col, 4, lcd_buf, 8, WHITE);
      }
      else
      {
        snprintf(lcd_buf, 255, "%5.1f %%", (double)edit_value);
        menuDirectWriteFixedRow(0, value_col, 4, lcd_buf, 7, WHITE);
      }
      last_value10 = value10;
    }

    ReadButtons(true);
  }

  return false;
}

static void FLASHMEM alarmSaveTauLow(float value)
{
  alarmSetTauLow10(alarmFloatTo10(value));
}

static void FLASHMEM alarmSaveTauHigh(float value)
{
  alarmSetTauHigh10(alarmFloatTo10(value));
}

static void FLASHMEM alarmSaveRhLow(float value)
{
  alarmSetRhLow10(alarmFloatToU10(value));
}

static void FLASHMEM alarmSaveRhHigh(float value)
{
  alarmSetRhHigh10(alarmFloatToU10(value));
}

void FLASHMEM alarm_tau_low_menu(void)
{
  static bool frisch = true;
  static float edit_value = 0.0f;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    edit_value = (float)alarmGetTauLow10() / 10.0f;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (alarmFloatValuePage(TXT_ALARM_TAU_LOW,
                          (ui_language == LANG_EN) ? "Dew min:" : "Tau min:",
                          edit_value,
                          -80.0f,
                          80.0f,
                          0.1f,
                          10,
                          alarmSaveTauLow,
                          true))
  {
    frisch = true;
  }
}

void FLASHMEM alarm_tau_high_menu(void)
{
  static bool frisch = true;
  static float edit_value = 0.0f;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    edit_value = (float)alarmGetTauHigh10() / 10.0f;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (alarmFloatValuePage(TXT_ALARM_TAU_HIGH,
                          (ui_language == LANG_EN) ? "Dew max:" : "Tau max:",
                          edit_value,
                          -80.0f,
                          80.0f,
                          0.1f,
                          10,
                          alarmSaveTauHigh,
                          true))
  {
    frisch = true;
  }
}

void FLASHMEM alarm_rh_low_menu(void)
{
  static bool frisch = true;
  static float edit_value = 0.0f;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    edit_value = (float)alarmGetRhLow10() / 10.0f;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (alarmFloatValuePage(TXT_ALARM_RH_LOW,
                          (ui_language == LANG_EN) ? "RH% min:" : "rF% min:",
                          edit_value,
                          0.0f,
                          100.0f,
                          0.1f,
                          9,
                          alarmSaveRhLow,
                          false))
  {
    frisch = true;
  }
}

void FLASHMEM alarm_rh_high_menu(void)
{
  static bool frisch = true;
  static float edit_value = 0.0f;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    edit_value = (float)alarmGetRhHigh10() / 10.0f;
    frisch = false;
    flag.menu_lcd_upd = false;
  }

  if (alarmFloatValuePage(TXT_ALARM_RH_HIGH,
                          (ui_language == LANG_EN) ? "RH% max:" : "rF% max:",
                          edit_value,
                          0.0f,
                          100.0f,
                          0.1f,
                          9,
                          alarmSaveRhHigh,
                          false))
  {
    frisch = true;
  }
}

// =========================================================================
// PELTIER MAXSTROM - kopfbezogen, 0.010-A-Schritte
// =========================================================================
void FLASHMEM peltier_current_limit_menu(void)
{
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;
  static uint16_t edit_ma = PELTIER_CURRENT_LIMIT_DEFAULT_MA;
  static uint16_t last_ma = 65535;

  if (VirtLCDMenu == nullptr) return;

  if (frisch)
  {
    edit_ma = peltierCurrentLimitGetMa();
    frisch = false;
    page_drawn = false;
    last_ma = 65535;
    flag.menu_lcd_upd = false;
  }

  if (menuValueHandleUInt16(edit_ma,
                            PELTIER_CURRENT_LIMIT_MIN_MA,
                            PELTIER_CURRENT_LIMIT_MAX_MA,
                            10,
                            nullptr,
                            180))
  {
    peltierCurrentLimitSetMa(edit_ma);
    frisch = true;
    page_drawn = false;
    last_ma = 65535;
    menuValueSaveAndReturn(MENU_CONTROL_PARAMETERS, false);
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    if (!page_drawn)
    {
      menuValueBeginDraw(TXT_MENU_CONTROL_TITLE, false);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print("Peltier max:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "UP/DOWN: 0.010 A" : "UP/DOWN: 0,010 A");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_ENTER_SAVE));

      VirtLCDMenu->transfer();
      ReadButtons(true);

      menuDirectResetValueCache();
      page_drawn = true;
      last_ma = 65535;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    if (last_ma != edit_ma)
    {
      snprintf(lcd_buf, 255, "%1u.%03u A", (unsigned)(edit_ma / 1000U), (unsigned)(edit_ma % 1000U));
      menuDirectWriteFixedRow(0, 14, 4, lcd_buf, 7, WHITE);
      last_ma = edit_ma;
    }

    ReadButtons(true);
  }
}
