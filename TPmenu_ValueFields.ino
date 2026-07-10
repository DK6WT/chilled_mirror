/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPmenu_ValueFields.ino
 * Zweck: Wertefeld-Helfer des ausgelagerten Menuesystems.
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
// TPmenu_ValueFields.ino
// ausgelagert aus TPmenu.ino - keine Funktionsaenderung
// =========================================================================

// KLEINES VALUE-MENUE-FRAMEWORK
// =========================================================================
// Diese Hilfen vereinheitlichen nur die Bedienlogik der einfachen Wert-Menues.
// Die Anzeigezeilen werden weiterhin im jeweiligen Menue gezeichnet, damit die
// bestehende Optik und Positionierung unveraendert bleiben.

static void menuDirectClearAllRows(void);

static void menuValueConsumeTouch(void)
{
  TouchZ = false;
  TouchX = 0;
  TouchY = 0;
}

static void menuValueSaveAndReturn(uint16_t return_level, bool return_to_main)
{
  extern uint32_t menu_global_debounce;
  extern bool controlParamsDeferValueSave(uint16_t return_level, bool return_to_main);

  menuDirectClearAllRows();

  if (!controlParamsDeferValueSave(return_level, return_to_main))
  {
    tpMainConfigSave();
  }

  menu_level = return_level;

  if (return_to_main)
  {
    frisch_geoeffnet = true;
  }

  flag.short_push = false;
  flag.menu_lcd_upd = false;

  menuValueConsumeTouch();
  menu_global_debounce = millis();
}

static void menuValueBeginDraw(TextId title, bool clear_message)
{
  menuDirectClearAllRows();

  flag.menu_lcd_upd = true;

  FirstBut = 0;
  LastBut = 3;

  VirtLCDMenu->clear();

  if (clear_message && VirtLCDMessage != nullptr)
  {
    VirtLCDMessage->clear();
  }

  VirtLCDMenu->setCursor(1, 1);
  VirtLCDMenu->print(T(title));
}

static void menuValueEndDraw(void)
{
  VirtLCDMenu->transfer();
  ReadButtons(true);
}

// Feste Direkt-Wertfelder fuer schnell wechselnde Menue-Zahlen.
// Gleiche Grundidee wie Haupt-/Diagnose-Anzeige:
// fester Textbereich, schwarzer Texthintergrund, Cache und nur bei Aenderung
// neu schreiben. Diese Felder liegen direkt auf dem TFT und muessen beim
// Verlassen/Neuzeichnen direkt wieder geloescht werden.
#define MENU_DIRECT_SLOT_COUNT 16

// Cache fuer direkt gezeichnete Menuewerte in RAM2 auslagern.
static DMAMEM char     menuDirectValueCache[MENU_DIRECT_SLOT_COUNT][64];
static DMAMEM bool     menuDirectValueActive[MENU_DIRECT_SLOT_COUNT];
static DMAMEM uint8_t  menuDirectValueCol[MENU_DIRECT_SLOT_COUNT];
static DMAMEM uint8_t  menuDirectValueRow[MENU_DIRECT_SLOT_COUNT];
static DMAMEM uint8_t  menuDirectValueWidth[MENU_DIRECT_SLOT_COUNT];

static void menuDirectResetValueCache(void)
{
  for (uint8_t i = 0; i < MENU_DIRECT_SLOT_COUNT; i++)
  {
    menuDirectValueCache[i][0] = '\0';
    menuDirectValueActive[i] = false;
    menuDirectValueCol[i] = 0;
    menuDirectValueRow[i] = 0;
    menuDirectValueWidth[i] = 0;
  }
}

static void menuDirectMakeFixedField(char* out, size_t outSize, const char* text, uint8_t chars)
{
  if (outSize == 0) return;

  uint8_t n = 0;
  while (text != nullptr && text[n] != '\0' && n < chars && n < (outSize - 1))
  {
    out[n] = text[n];
    n++;
  }

  while (n < chars && n < (outSize - 1))
  {
    out[n++] = ' ';
  }

  out[n] = '\0';
}

static void menuDirectTextGeometry(int16_t* fontX, int16_t* fontY)
{
  static bool ready = false;
  static int16_t cachedFontX = 14;
  static int16_t cachedFontY = 29;

  if (!ready)
  {
    // DroidSansMono_20 ist ein Monospace-Font.
    // Breite/Hoehe fest setzen, damit kein sichtbares Test-"a" auf den TFT geschrieben wird.
    cachedFontX = 17;
    cachedFontY = 35;
    ready = true;
  }

  if (fontX != nullptr) *fontX = cachedFontX;
  if (fontY != nullptr) *fontY = cachedFontY;
}

static void menuDirectWriteFixedRow(uint8_t slot,
                                    uint8_t col,
                                    uint8_t row,
                                    const char* text,
                                    uint8_t width,
                                    uint16_t color)
{
  char fixed[64];
  int16_t fontX = 14;
  int16_t fontY = 29;

  if (slot >= MENU_DIRECT_SLOT_COUNT) return;
  if (width >= sizeof(fixed)) width = sizeof(fixed) - 1;

  menuDirectMakeFixedField(fixed, sizeof(fixed), text, width);

  if (menuDirectValueActive[slot] &&
      menuDirectValueCol[slot] == col &&
      menuDirectValueRow[slot] == row &&
      menuDirectValueWidth[slot] == width &&
      strncmp(menuDirectValueCache[slot], fixed, sizeof(menuDirectValueCache[slot])) == 0)
  {
    return;
  }

  menuDirectTextGeometry(&fontX, &fontY);

  // Falls derselbe Slot auf eine andere Position wechselt, altes Feld entfernen.
  if (menuDirectValueActive[slot] &&
      (menuDirectValueCol[slot] != col ||
       menuDirectValueRow[slot] != row ||
       menuDirectValueWidth[slot] != width))
  {
    const int16_t oldX = 155 + fontX * menuDirectValueCol[slot];
    const int16_t oldY = 40  + fontY * menuDirectValueRow[slot];

    tft.setFont(DroidSansMono_20);
    tft.setTextColor(BLACK, BLACK);
    tft.setCursor(oldX, oldY);
    tft.print(menuDirectValueCache[slot]);

    menuDirectValueCache[slot][0] = '\0';
    menuDirectValueActive[slot] = false;
  }

  const int16_t x = 155 + fontX * col;
  const int16_t y = 40  + fontY * row;

  tft.setFont(DroidSansMono_20);
  tft.setTextColor(color, BLACK);

  // Zeichenweise schreiben: nur die Zeichen des Wertefeldes, die sich wirklich
  // geaendert haben, werden neu auf das TFT geschrieben.
  for (uint8_t i = 0; i < width; i++)
  {
    char oldChar = (menuDirectValueActive[slot] && i < strlen(menuDirectValueCache[slot]))
                   ? menuDirectValueCache[slot][i]
                   : '\0';
    char newChar = fixed[i];

    if (!menuDirectValueActive[slot] || oldChar != newChar)
    {
      tft.setCursor(x + fontX * i, y);
      tft.print(newChar);
    }
  }

  size_t copyLen = strlen(fixed);
  if (copyLen >= sizeof(menuDirectValueCache[slot]))
  {
    copyLen = sizeof(menuDirectValueCache[slot]) - 1;
  }
  memcpy(menuDirectValueCache[slot], fixed, copyLen);
  menuDirectValueCache[slot][copyLen] = '\0';

  menuDirectValueCol[slot] = col;
  menuDirectValueRow[slot] = row;
  menuDirectValueWidth[slot] = width;
  menuDirectValueActive[slot] = true;
}

static void menuDirectClearFixedRow(uint8_t slot,
                                    uint8_t col,
                                    uint8_t row,
                                    uint8_t width)
{
  int16_t fontX = 14;
  int16_t fontY = 29;

  if (slot >= MENU_DIRECT_SLOT_COUNT) return;

  menuDirectTextGeometry(&fontX, &fontY);

  const int16_t x = 155 + fontX * col;
  const int16_t y = 40  + fontY * row;

  tft.setFont(DroidSansMono_20);
  tft.setTextColor(BLACK, BLACK);

  for (uint8_t i = 0; i < width; i++)
  {
    tft.setCursor(x + fontX * i, y);
    tft.print(' ');
  }

  menuDirectValueCache[slot][0] = '\0';
  menuDirectValueActive[slot] = false;
}

static void menuDirectClearAllRows(void)
{
  for (uint8_t i = 0; i < MENU_DIRECT_SLOT_COUNT; i++)
  {
    if (menuDirectValueActive[i])
    {
      menuDirectClearFixedRow(i,
                              menuDirectValueCol[i],
                              menuDirectValueRow[i],
                              menuDirectValueWidth[i]);
    }
  }
}

static void menuDirectClearFanRows(void)
{
  // Kompatibilitaets-Wrapper fuer den getesteten Fan-Speed-Code.
  menuDirectClearFixedRow(0, 10, 4, 5);
  menuDirectClearFixedRow(1, 6, 6, 11);
}

static bool menuValueHandleUInt8(uint8_t& value,
                                 uint8_t min_value,
                                 uint8_t max_value,
                                 uint8_t step,
                                 void (*on_change)(void),
                                 uint16_t debounce_ms)
{
  ReadButtons(false);

  uint8_t key = menuReadKey(debounce_ms);

  if (key == MK_UP)
  {
    uint16_t next_value = (uint16_t)value + step;
    if (next_value > max_value) next_value = max_value;

    if (next_value != value)
    {
      value = (uint8_t)next_value;
      if (on_change != nullptr) on_change();
    }

    flag.menu_lcd_upd = false;
  }
  else if (key == MK_DOWN)
  {
    int16_t next_value = (int16_t)value - step;
    if (next_value < min_value) next_value = min_value;

    if (next_value != value)
    {
      value = (uint8_t)next_value;
      if (on_change != nullptr) on_change();
    }

    flag.menu_lcd_upd = false;
  }
  else if (key == MK_ENTER)
  {
    flag.short_push = false;
    return true;
  }

  return false;
}

static bool menuValueHandleUInt16(uint16_t& value,
                                  uint16_t min_value,
                                  uint16_t max_value,
                                  uint16_t step,
                                  void (*on_change)(void),
                                  uint16_t debounce_ms)
{
  ReadButtons(false);

  uint8_t key = menuReadKey(debounce_ms);

  if (key == MK_UP)
  {
    uint32_t next_value = (uint32_t)value + step;
    if (next_value > max_value) next_value = max_value;

    if (next_value != value)
    {
      value = (uint16_t)next_value;
      if (on_change != nullptr) on_change();
    }

    flag.menu_lcd_upd = false;
  }
  else if (key == MK_DOWN)
  {
    int32_t next_value = (int32_t)value - step;
    if (next_value < min_value) next_value = min_value;

    if (next_value != value)
    {
      value = (uint16_t)next_value;
      if (on_change != nullptr) on_change();
    }

    flag.menu_lcd_upd = false;
  }
  else if (key == MK_ENTER)
  {
    flag.short_push = false;
    return true;
  }

  return false;
}

static bool menuValueHandleFloat(float& value,
                                 float min_value,
                                 float max_value,
                                 float step,
                                 void (*on_change)(void),
                                 uint16_t debounce_ms)
{
  ReadButtons(false);

  uint8_t key = menuReadKey(debounce_ms);

  if (key == MK_UP)
  {
    value += step;
    if (value > max_value) value = max_value;
    if (on_change != nullptr) on_change();
    flag.menu_lcd_upd = false;
  }
  else if (key == MK_DOWN)
  {
    value -= step;
    if (value < min_value) value = min_value;
    if (on_change != nullptr) on_change();
    flag.menu_lcd_upd = false;
  }
  else if (key == MK_ENTER)
  {
    flag.short_push = false;
    return true;
  }

  return false;
}


// =========================================================================
// KLEINES ZIFFERNSTELLEN-FRAMEWORK FUER KALIBRIERWERTE
// =========================================================================
// Diese Hilfen werden bei Pt100-2P, Sensor-R0 und Ref100/Ref120 benutzt.
// Sie veraendern nur die Bedienlogik. Die Anzeige bleibt in den Menues,
// damit Zeilen, Texte und Optik unveraendert bleiben.
#ifndef MDVA_NONE
#define MDVA_NONE       0
#define MDVA_CHANGED    1
#define MDVA_NEXT_DIGIT 2
#define MDVA_DONE       3
#endif

static uint8_t menuDigitValueHandle(int32_t& value,
                                    uint8_t& digit_index,
                                    uint8_t digit_count,
                                    const int32_t* digit_steps,
                                    int32_t min_value,
                                    int32_t max_value,
                                    uint16_t debounce_ms)
{
  if (digit_count == 0 || digit_steps == nullptr) return MDVA_NONE;
  if (digit_index >= digit_count) digit_index = 0;

  ReadButtons(false);

  uint8_t key = menuReadKey(debounce_ms);

  if (key == MK_UP)
  {
    value += digit_steps[digit_index];

    if (value > max_value)
    {
      value = min_value;
    }

    flag.menu_lcd_upd = false;
    return MDVA_CHANGED;
  }

  if (key == MK_DOWN)
  {
    value -= digit_steps[digit_index];

    if (value < min_value)
    {
      value = max_value;
    }

    flag.menu_lcd_upd = false;
    return MDVA_CHANGED;
  }

  if (key == MK_ENTER)
  {
    flag.short_push = false;

    if (digit_index < (digit_count - 1))
    {
      digit_index++;
      flag.menu_lcd_upd = false;
      return MDVA_NEXT_DIGIT;
    }

    digit_index = 0;
    flag.menu_lcd_upd = false;
    return MDVA_DONE;
  }

  return MDVA_NONE;
}

static void menuDigitResetTouchDebounce(void)
{
  extern uint32_t menu_global_debounce;

  menuDirectClearAllRows();
  menuValueConsumeTouch();
  menu_global_debounce = millis();
}

static void menuDigitBeginDraw(TextId title)
{
  menuDirectClearAllRows();

  flag.menu_lcd_upd = true;

  VirtLCDMenu->clear();

  if (VirtLCDMessage != nullptr)
  {
    VirtLCDMessage->clear();
  }

  VirtLCDMenu->setCursor(1, 1);
  VirtLCDMenu->print(T(title));
}

static void menuDigitEndDraw(void)
{
  VirtLCDMenu->transfer();
  ReadButtons(true);
}

static void menuPrintTextIdForIndex(uint8_t index,
                                    const TextId* text_ids,
                                    uint8_t count)
{
  if (text_ids == nullptr || index >= count) return;
  VirtLCDMenu->print(T(text_ids[index]));
}


