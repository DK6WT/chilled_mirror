/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPmenu_CalibrationPages.ino
 * Zweck: Kalibrierseiten, aus dem historischen Menuesystem ausgelagert.
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
// TPmenu_CalibrationPages.ino
// Kalibrierwert-Seiten - ausgelagert aus TPmenu.ino
// =========================================================================

static void FLASHMEM formatTempMilliC(int32_t milliC, char* out, size_t outSize)
{
  char sign = '+';

  if (milliC < 0)
  {
    sign = '-';
    milliC = -milliC;
  }

  int32_t ganz = milliC / 1000L;
  int32_t frac = milliC % 1000L;

  snprintf(out, outSize, "%c%02ld.%03ld \xB0""C", sign, (long)ganz, (long)frac);
}


// =========================================================================
// Pt100 2-PUNKT-KALIBRIERUNG MANUELL
// =========================================================================
void FLASHMEM pt100_2punkt_menu(void)
{
  uint8_t sensor = (menu_level == MENU_PT100_2P_MIRROR) ? 0 : 1; // 0 Spiegel, 1 Umgebung

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  // Werte in m°C:
  // Soll1, Ist1, Soll2, Ist2
  static int32_t werte[4] = {20000L, 20000L, 50000L, 50000L};

  // aktueller Wert 0..3 und Stelle 0..4
  // Stelle 0 = 10.000 C, 1 = 1.000 C, 2 = 0.100 C, 3 = 0.010 C, 4 = 0.001 C
  static uint8_t wertIndex = 0;
  static uint8_t stelle = 0;
  static bool frisch = true;
  static bool meldungAbstand = false;
  static uint8_t letzterSensor = 255;
  static MenuPageDrawFlag page_drawn;

  static const int32_t TEMP_STEPS[5] = {
    10000L,
    1000L,
    100L,
    10L,
    1L
  };

  static const TextId TEMP_DIGIT_TEXTS[5] = {
    TXT_PT100_DIGIT_10,
    TXT_PT100_DIGIT_1,
    TXT_PT100_DIGIT_0_1,
    TXT_PT100_DIGIT_0_01,
    TXT_PT100_DIGIT_0_001_NEXT
  };

  if (letzterSensor != sensor)
  {
    letzterSensor = sensor;
    frisch = true;
    wertIndex = 0;
    stelle = 0;
    meldungAbstand = false;
    page_drawn = false;
  }

  const int32_t TEMP_MIN_MC = -30000L;
  const int32_t TEMP_MAX_MC =  90000L;

  if (frisch)
  {
    bool aktiv = false;

    pt100Cal2GetScaled(sensor, &werte[0], &werte[1], &werte[2], &werte[3], &aktiv);

    // Falls noch nie gespeichert: sinnvolle Startwerte
    if (werte[0] < TEMP_MIN_MC || werte[0] > TEMP_MAX_MC ||
        werte[1] < TEMP_MIN_MC || werte[1] > TEMP_MAX_MC ||
        werte[2] < TEMP_MIN_MC || werte[2] > TEMP_MAX_MC ||
        werte[3] < TEMP_MIN_MC || werte[3] > TEMP_MAX_MC)
    {
      werte[0] = 20000L;
      werte[1] = 20000L;
      werte[2] = 50000L;
      werte[3] = 50000L;
    }

    wertIndex = 0;
    stelle = 0;
    frisch = false;
    meldungAbstand = false;
    page_drawn = false;
    flag.menu_lcd_upd = false;
  }

  uint8_t action = menuDigitValueHandle(werte[wertIndex],
                                        stelle,
                                        5,
                                        TEMP_STEPS,
                                        TEMP_MIN_MC,
                                        TEMP_MAX_MC,
                                        250);

  if (action != MDVA_NONE)
  {
    if (meldungAbstand) page_drawn = false;
    meldungAbstand = false;

    if (action == MDVA_NEXT_DIGIT)
    {
      page_drawn = false;
    }

    if (action == MDVA_DONE)
    {
      if (wertIndex < 3)
      {
        wertIndex++;
        page_drawn = false;
      }
      else
      {
        bool ok = pt100Cal2SetScaled(sensor, werte[0], werte[1], werte[2], werte[3]);

        if (ok)
        {
          frisch = true;
          wertIndex = 0;
          stelle = 0;
          meldungAbstand = false;
          page_drawn = false;

          menu_level = MENU_PT100_2P_SELECT;          // zurück zur 2P Sensorwahl
          flag.menu_lcd_upd = false;
          flag.short_push = false;

          menuDigitResetTouchDebounce();
          return;
        }

        // Abstand zu klein oder Werte unplausibel
        meldungAbstand = true;
        page_drawn = false;
      }
    }
  }

  if (!flag.menu_lcd_upd)
  {
    char b0[18], b1[18], b2[18], b3[18];
    formatTempMilliC(werte[0], b0, sizeof(b0));
    formatTempMilliC(werte[1], b1, sizeof(b1));
    formatTempMilliC(werte[2], b2, sizeof(b2));
    formatTempMilliC(werte[3], b3, sizeof(b3));

    if (!page_drawn)
    {
      if (sensor == 0)
      {
        menuDigitBeginDraw(TXT_TITLE_MIRROR_PT100_2POINT);
      }
      else
      {
        menuDigitBeginDraw(TXT_TITLE_AMBIENT_PT100_2POINT);
      }

      VirtLCDMenu->setCursor(3, 3);
      VirtLCDMenu->print(T(TXT_PT100_SET1));
      VirtLCDMenu->print(":");
      VirtLCDMenu->setCursor(3, 4);
      VirtLCDMenu->print(T(TXT_PT100_ACT1));
      VirtLCDMenu->print(":");
      VirtLCDMenu->setCursor(3, 5);
      VirtLCDMenu->print(T(TXT_PT100_SET2));
      VirtLCDMenu->print(":");
      VirtLCDMenu->setCursor(3, 6);
      VirtLCDMenu->print(T(TXT_PT100_ACT2));
      VirtLCDMenu->print(":");

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_LABEL_DIGIT));
      menuPrintTextIdForIndex(stelle, TEMP_DIGIT_TEXTS, 5);

      VirtLCDMenu->setCursor(1, 10);
      if (meldungAbstand)
      {
        VirtLCDMenu->print(T(TXT_ERR_PT100_DISTANCE));
      }
      else
      {
        VirtLCDMenu->print(T(TXT_UP_DOWN_ENTER_NEXT));
      }

      menuDigitEndDraw();

      menuDirectResetValueCache();
      page_drawn = true;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    snprintf(lcd_buf, 255, "%c", (wertIndex == 0) ? '>' : ' ');
    menuDirectWriteFixedRow(0, 1, 3, lcd_buf, 1, WHITE);
    menuDirectWriteFixedRow(4, 11, 3, b0, 10, WHITE);

    snprintf(lcd_buf, 255, "%c", (wertIndex == 1) ? '>' : ' ');
    menuDirectWriteFixedRow(1, 1, 4, lcd_buf, 1, WHITE);
    menuDirectWriteFixedRow(5, 11, 4, b1, 10, WHITE);

    snprintf(lcd_buf, 255, "%c", (wertIndex == 2) ? '>' : ' ');
    menuDirectWriteFixedRow(2, 1, 5, lcd_buf, 1, WHITE);
    menuDirectWriteFixedRow(6, 11, 5, b2, 10, WHITE);

    snprintf(lcd_buf, 255, "%c", (wertIndex == 3) ? '>' : ' ');
    menuDirectWriteFixedRow(3, 1, 6, lcd_buf, 1, WHITE);
    menuDirectWriteFixedRow(7, 11, 6, b3, 10, WHITE);

    ReadButtons(true);
  }
}


// =========================================================================
// SENSOR AUSWAHL
void FLASHMEM fühler_eichung_menu(void)
{
  uint8_t scale_set = (menu_level == MENU_SENSOR_R0_MIRROR) ? 0 : 1; // 0 = Spiegel, 1 = Umgebung

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  // R0 intern als Ohm * 10000
  // Beispiel: 100.1234 Ohm = 1001234
  static int32_t r0_scaled = 1000000L;

  // 0 = Vorkomma, 1 = 0.1000, 2 = 0.0100, 3 = 0.0010, 4 = 0.0001
  static uint8_t eich_schritt = 0;
  static bool eich_frisch = true;
  static uint8_t letzterScaleSet = 255;
  static MenuPageDrawFlag page_drawn;

  static const int32_t OHM_STEPS[5] = {
    10000L,
    1000L,
    100L,
    10L,
    1L
  };

  static const TextId OHM_DIGIT_TEXTS[5] = {
    TXT_DIGIT_INTEGER,
    TXT_DIGIT_0_1000,
    TXT_DIGIT_0_0100,
    TXT_DIGIT_0_0010,
    TXT_DIGIT_0_0001_SAVE
  };

  if (letzterScaleSet != scale_set)
  {
    letzterScaleSet = scale_set;
    eich_frisch = true;
    eich_schritt = 0;
    page_drawn = false;
  }

  const int32_t R0_MIN_SCALED = 800000L;   // 80.0000 Ohm
  const int32_t R0_MAX_SCALED = 1200000L;  // 120.0000 Ohm

  if (eich_frisch)
  {
    r0_scaled = pt100R0GetScaled(scale_set);

    if (r0_scaled < R0_MIN_SCALED) r0_scaled = R0_MIN_SCALED;
    if (r0_scaled > R0_MAX_SCALED) r0_scaled = R0_MAX_SCALED;

    eich_schritt = 0;
    eich_frisch = false;
    page_drawn = false;
    flag.menu_lcd_upd = false;
  }

  uint8_t action = menuDigitValueHandle(r0_scaled,
                                        eich_schritt,
                                        5,
                                        OHM_STEPS,
                                        R0_MIN_SCALED,
                                        R0_MAX_SCALED,
                                        250);

  if (action == MDVA_NEXT_DIGIT)
  {
    page_drawn = false;
  }

  if (action == MDVA_DONE)
  {
    // R0 separat speichern. var_t wird hier bewusst nicht geschrieben,
    // damit Werkseinstellungen die Messketten-Kalibrierung nicht loeschen.
    pt100R0SetScaled(scale_set, r0_scaled);

    // Engine zurücksetzen für nächstes Öffnen
    eich_frisch = true;
    eich_schritt = 0;
    page_drawn = false;

    // Zurück ins Sensor-Auswahlmenü
    menu_level = MENU_SENSOR_R0_SELECT;

    // Wichtig: false, damit Sensor-Auswahlmenü neu gezeichnet wird
    flag.menu_lcd_upd = false;
    flag.short_push = false;

    // Touch sperren gegen gehaltenen Finger
    menuDigitResetTouchDebounce();
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    int32_t vorkomma = r0_scaled / 10000L;
    int32_t nachkomma = r0_scaled % 10000L;

    snprintf(lcd_buf, 255, "%3ld.%04ld \xB1", (long)vorkomma, (long)nachkomma);

    if (!page_drawn)
    {
      menuDigitBeginDraw(TXT_MENU_SENSOR_R0_TITLE);

      VirtLCDMenu->setCursor(1, 3);

      if (scale_set == 0)
      {
        VirtLCDMenu->print(T(TXT_SENSOR_MIRROR_PT100));
      }
      else
      {
        VirtLCDMenu->print(T(TXT_SENSOR_AMBIENT_PT100));
      }

      VirtLCDMenu->setCursor(1, 5);
      VirtLCDMenu->print("R0:");

      VirtLCDMenu->setCursor(1, 7);
      VirtLCDMenu->print(T(TXT_LABEL_DIGIT));
      menuPrintTextIdForIndex(eich_schritt, OHM_DIGIT_TEXTS, 5);

      VirtLCDMenu->setCursor(1, 9);
      VirtLCDMenu->print(T(TXT_UP_DOWN_ENTER_NEXT));

      menuDigitEndDraw();

      menuDirectResetValueCache();
      page_drawn = true;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    menuDirectWriteFixedRow(0, 5, 5, lcd_buf, 13, WHITE);

    ReadButtons(true);
  }
}



// =========================================================================
// REFERENZWIDERSTAND SENSORWAHL
void FLASHMEM referenz_eichung_menu(void)
{
  bool istRef100 = (menu_level == MENU_REF_100R);

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  // Ohm * 100000
  static int32_t ref_scaled = 10000000L;
  static uint8_t stelle = 0;
  static bool frisch = true;
  static bool meldungFehler = false;
  static uint8_t letzterTyp = 255;
  static MenuPageDrawFlag page_drawn;

  static const int32_t OHM_STEPS[6] = {
    100000L,
    10000L,
    1000L,
    100L,
    10L,
    1L
  };

  uint8_t aktuellerTyp = istRef100 ? 0 : 1;

  if (letzterTyp != aktuellerTyp)
  {
    letzterTyp = aktuellerTyp;
    frisch = true;
    stelle = 0;
    meldungFehler = false;
    page_drawn = false;
  }

  const int32_t REF100_MIN = 6000000L;    // Ref Low: 60.00000 Ohm
  const int32_t REF100_MAX = 11000000L;   // Ref Low: 110.00000 Ohm
  const int32_t REF120_MIN = 11500000L;   // Ref High: 115.00000 Ohm
  const int32_t REF120_MAX = 15200000L;   // Ref High: 152.00000 Ohm

  if (frisch)
  {
    int32_t ref100 = 10000000L;
    int32_t ref120 = 12000000L;

    refCalGetScaled(&ref100, &ref120);

    ref_scaled = istRef100 ? ref100 : ref120;

    if (istRef100)
    {
      if (ref_scaled < REF100_MIN || ref_scaled > REF100_MAX)
      {
        ref_scaled = 10000000L;
      }
    }
    else
    {
      if (ref_scaled < REF120_MIN || ref_scaled > REF120_MAX)
      {
        ref_scaled = 12000000L;
      }
    }

    stelle = 0;
    frisch = false;
    meldungFehler = false;
    page_drawn = false;
    flag.menu_lcd_upd = false;
  }

  int32_t minVal = istRef100 ? REF100_MIN : REF120_MIN;
  int32_t maxVal = istRef100 ? REF100_MAX : REF120_MAX;

  uint8_t action = menuDigitValueHandle(ref_scaled,
                                        stelle,
                                        6,
                                        OHM_STEPS,
                                        minVal,
                                        maxVal,
                                        250);

  if (action != MDVA_NONE)
  {
    if (meldungFehler) page_drawn = false;
    meldungFehler = false;

    if (action == MDVA_NEXT_DIGIT)
    {
      page_drawn = false;
    }

    if (action == MDVA_DONE)
    {
      bool ok = false;

      if (istRef100)
      {
        ok = refCalSet100Scaled(ref_scaled);
      }
      else
      {
        ok = refCalSet120Scaled(ref_scaled);
      }

      if (ok)
      {
        frisch = true;
        stelle = 0;
        meldungFehler = false;
        page_drawn = false;

        menu_level = MENU_REFERENCES;          // zurück zur Referenzwahl
        flag.menu_lcd_upd = false;
        flag.short_push = false;

        menuDigitResetTouchDebounce();
        return;
      }

      meldungFehler = true;
      page_drawn = false;
    }
  }

  if (!flag.menu_lcd_upd)
  {
    int32_t vorkomma = ref_scaled / 100000L;
    int32_t nachkomma = ref_scaled % 100000L;

    snprintf(lcd_buf, 255, "%3ld.%05ld \xB1", (long)vorkomma, (long)nachkomma);

    if (!page_drawn)
    {
      if (istRef100)
      {
        menuDigitBeginDraw(TXT_REF100_TITLE);
      }
      else
      {
        menuDigitBeginDraw(TXT_REF120_TITLE);
      }

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "Value:" : "Wert:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print(T(TXT_LABEL_DIGIT));

      // Bei Ref ist die erste Stelle 1 Ohm, danach folgen 5 Nachkommastellen.
      switch (stelle)
      {
        case 0: VirtLCDMenu->print("[ 1.00000 ]"); break;
        case 1: VirtLCDMenu->print("[ 0.10000 ]"); break;
        case 2: VirtLCDMenu->print("[ 0.01000 ]"); break;
        case 3: VirtLCDMenu->print("[ 0.00100 ]"); break;
        case 4: VirtLCDMenu->print("[ 0.00010 ]"); break;
        default:
          VirtLCDMenu->print((ui_language == LANG_EN) ? "[ 0.00001 ] -> Save" : "[ 0.00001 ] -> Speichern");
          break;
      }

      VirtLCDMenu->setCursor(1, 8);

      if (meldungFehler)
      {
        VirtLCDMenu->print(T(TXT_ERR_REF_ORDER));
      }
      else
      {
        VirtLCDMenu->print(T(TXT_UP_DOWN_ENTER_NEXT));
      }

      menuDigitEndDraw();

      menuDirectResetValueCache();
      page_drawn = true;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    menuDirectWriteFixedRow(0, 8, 4, lcd_buf, 14, WHITE);

    ReadButtons(true);
  }
}


// =========================================================================
// WERKSEINSTELLUNG

// =========================================================================
// TAUPUNKT-/FROSTPUNKT-OFFSET 1-PUNKT
// =========================================================================
void FLASHMEM taupunkt_offset_menu(void)
{
  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  // Offset intern als mC: -2.000 C .. +2.000 C
  static int32_t offset_mC = 0L;
  static uint8_t stelle = 0;
  static bool frisch = true;
  static MenuPageDrawFlag page_drawn;

  // Stelle 0 = 1.000 C, 1 = 0.100 C, 2 = 0.010 C, 3 = 0.001 C
  static const int32_t OFFSET_STEPS[4] = {
    1000L,
    100L,
    10L,
    1L
  };

  static const TextId OFFSET_DIGIT_TEXTS[4] = {
    TXT_PT100_DIGIT_1,
    TXT_PT100_DIGIT_0_1,
    TXT_PT100_DIGIT_0_01,
    TXT_DIGIT_0_001_SAVE
  };

  const int32_t OFFSET_MIN_MC = -2000L;
  const int32_t OFFSET_MAX_MC =  2000L;

  if (frisch)
  {
    offset_mC = taupunktOffsetGetScaled();

    if (offset_mC < OFFSET_MIN_MC || offset_mC > OFFSET_MAX_MC)
    {
      offset_mC = 0L;
    }

    stelle = 0;
    frisch = false;
    page_drawn = false;
    flag.menu_lcd_upd = false;
  }

  uint8_t action = menuDigitValueHandle(offset_mC,
                                        stelle,
                                        4,
                                        OFFSET_STEPS,
                                        OFFSET_MIN_MC,
                                        OFFSET_MAX_MC,
                                        250);

  if (action == MDVA_NEXT_DIGIT)
  {
    page_drawn = false;
  }

  if (action == MDVA_DONE)
  {
    taupunktOffsetSetScaled(offset_mC);

    frisch = true;
    stelle = 0;
    page_drawn = false;

    menu_level = MENU_CALIBRATION;
    flag.menu_lcd_upd = false;
    flag.short_push = false;

    menuDigitResetTouchDebounce();
    return;
  }

  if (!flag.menu_lcd_upd)
  {
    char buf[18];
    formatTempMilliC(offset_mC, buf, sizeof(buf));

    if (!page_drawn)
    {
      menuDigitBeginDraw(TXT_TAUPOINT_OFFSET_TITLE);

      VirtLCDMenu->setCursor(1, 4);
      VirtLCDMenu->print("Offset:");

      VirtLCDMenu->setCursor(1, 6);
      VirtLCDMenu->print(T(TXT_LABEL_DIGIT));
      menuPrintTextIdForIndex(stelle, OFFSET_DIGIT_TEXTS, 4);

      VirtLCDMenu->setCursor(1, 8);
      VirtLCDMenu->print(T(TXT_UP_DOWN_ENTER_NEXT));

      VirtLCDMenu->setCursor(1, 10);
      VirtLCDMenu->print((ui_language == LANG_EN) ? "Range: -2.000 .. +2.000 \xB0""C" : "Bereich: -2.000 .. +2.000 \xB0""C");

      menuDigitEndDraw();

      menuDirectResetValueCache();
      page_drawn = true;
    }
    else
    {
      flag.menu_lcd_upd = true;
    }

    menuDirectWriteFixedRow(0, 9, 4, buf, 10, WHITE);

    ReadButtons(true);
  }
}
