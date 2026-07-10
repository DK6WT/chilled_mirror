/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPtouchscreen.ino
 * Zweck: Touchscreen-, Button- und Setup-Bedienlogik.
 *
 * Abgeleitet aus: PSWRtouchscreen.ino und buttons.ino
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2016 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 *   Zusaetzlich enthaltene Button-Grundstruktur: buttons.ino,
 *   Copyright (C) 2021 J.G. Holstein
 *
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

void menuInvalidatePageDrawCache(void);
//******************************************************************************
//**
//** STABILES TOUCH + BUTTON SYSTEM FÜR RA8875 + GSL1680
//**
//** FIXES:
//** --------------------------------------------------------------------------
//** ✓ KEINE String-Fragmentierung mehr
//** ✓ KEINE Speicherkorruption
//** ✓ KEINE Array-Overflows
//** ✓ RA8875 textMode()/graphicsMode() sauber
//** ✓ Icons wieder mittig
//** ✓ stabil gegen lila Grafikfehler
//** ✓ sichere Fontbehandlung
//** ✓ stabile Button-Redraws
//**
//******************************************************************************

#include <SPI.h>
#include <RA8875.h>

extern RA8875 tft;

#ifndef YELLOW
#define YELLOW 0xFFE0
#endif

#ifndef GREEN
#define GREEN 0x07E0
#endif

extern uint16_t TouchX;
extern uint16_t TouchY;
extern bool TouchZ;

extern bool refresh;
extern int FirstBut;
extern int LastBut;
extern uint16_t menu_level;

extern flags flag;
extern bool touch_scroll_aktiv;
extern bool frisch_geoeffnet;
extern TextBox* VirtLCDMenu;
extern TextBox* VirtLCDMessage;

extern bool sensorFanIsEnabled(void);
extern bool sensorFanTachoMissing(void);
extern const char* sensorFanStatusText(void);
extern void sensorFanToggleEnabled(void);

static void clearFanButtonArea(void);
static void resetFanButtonCache(void);

// Nach dem EXIT wird im Hauptscreen der alte FAN-Punkt fuer ein paar
// Frames gezielt geloescht. Das faengt RA8875-1-Pixel-Geister vom
// gefuellten Kreis ab, falls fillScreen/fillRect allein nicht sauber reicht.
bool fanButtonMainGhostClearActive(void);
void fanButtonMainGhostClearDoneOne(void);


// ============================================================================
// BUTTON STRUCT
// ============================================================================

struct NewBut
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;

    int16_t xoff;
    int16_t font;

    const char* offtext;
    uint16_t offtxtcol;
    uint16_t offbutcol;

    const char* ontext;
    uint16_t ontxtcol;
    uint16_t onbutcol;

    int16_t onoff;
};


// ============================================================================
// ICONS
// ============================================================================

static const char icon_ok[]    = {0x10, 0};
static const char icon_up[]    = {0x2A, 0};
static const char icon_down[]  = {0x2B, 0};


// ============================================================================
// BUTTON TABLE
// ============================================================================

static const uint8_t BUTTON_COUNT = 5;

NewBut But_Param[BUTTON_COUNT] =
{
    // SETUP
    {20, 350, 110, 120, 20, 0xf00, icon_ok,    WHITE, BLUE, icon_ok,    BLACK, 0xFFE0, 0},
    {20, 25,  110, 120, 20, 0x100, icon_up,    WHITE, BLUE, icon_up,    BLACK, 0xFFE0, 0},
    {20, 187, 110, 120, 20, 0x100, icon_down,  WHITE, BLUE, icon_down,  BLACK, 0xFFE0, 0},
    {675,350, 110, 120, 20, 0xfff, "EXIT",     WHITE, BLUE, "EXIT",     BLACK, 0xFFE0, 0},
    {675,187, 110, 120, 20, 0xfff, "FAN ON",   WHITE, BLUE, "FAN ON",   BLACK, 0xFFE0, 0}
};


// ============================================================================
// BUTTON STATE
// ============================================================================

static uint8_t Button[BUTTON_COUNT]    = {0};
static uint8_t ButAction[BUTTON_COUNT] = {0};
static uint8_t fanButtonMainGhostClearCounter = 0;

// Verhindert mehrfaches Neuzeichnen/Mehrfachausloesen des EXIT-Buttons,
// solange der Finger noch auf dem Touch liegt.
static bool exit_button_latched = false;

// Beim Wechsel vom Hauptscreen in das Setup liegt der SETUP-Button genau
// ueber der spaeteren ENTER-Flaeche. Diese Sperre verhindert, dass derselbe
// noch aufliegende Finger im frisch geoeffneten Menue sofort ENTER ausloest.
static bool menu_input_blocked_until_release = false;

void menuBlockInputUntilTouchRelease(void)
{
    menu_input_blocked_until_release = true;
    flag.short_push = false;
    touch_scroll_aktiv = false;

    // Der Bildschirm wird beim Eintritt ins Setup vollstaendig geloescht.
    // Deshalb duerfen die Button-Zeichenzustaende nicht weiter behaupten,
    // UP/DOWN/ENTER/EXIT seien bereits sichtbar. Solange der ausloesende
    // Finger noch aufliegt, bleibt die Bedienung gesperrt; direkt nach dem
    // echten Loslassen zeichnet ReadButtons() alle Setup-Tasten neu.
    memset(Button, 0, sizeof(Button));
    exit_button_latched = false;
    resetFanButtonCache();
}

// EXIT wird nur angefordert. Der eigentliche Bildschirmwechsel passiert
// zentral nach dem Ende der aktuellen Menue-Seite. So kann keine Seite
// nach einem fillScreen(BLACK) noch Text/Werte in den Hauptscreen schreiben.
bool menu_exit_requested = false;

bool menuExitRequested(void)
{
  return menu_exit_requested;
}

void menuExitClearRequest(void)
{
  menu_exit_requested = false;
}

void menuButtonsResetAfterExit(void)
{
  // Beim Verlassen des Menues den FAN-Button-Bereich nochmal schwarz
  // ueberzeichnen. Das verhindert 1-Pixel-Reste vom runden Statuspunkt
  // im Hauptscreen.
  clearFanButtonArea();

  memset(Button, 0, sizeof(Button));
  memset(ButAction, 0, sizeof(ButAction));
  exit_button_latched = false;
  resetFanButtonCache();

  // Kein Nachputzen im Hauptscreen mehr. Das hatte bei der RA8875-TextBox
  // ein schwarzes Loch erzeugt. Die FAN-LED wird jetzt ohne fillCircle(),
  // sondern als Software-Kreis aus fillRect()-Zeilen gezeichnet.
  fanButtonMainGhostClearCounter = 0;
}


// ============================================================================
// FAN-GHOST-CLEAR FUER HAUPTSCREEN
// ============================================================================

bool fanButtonMainGhostClearActive(void)
{
  return (fanButtonMainGhostClearCounter > 0);
}

void fanButtonMainGhostClearDoneOne(void)
{
  if (fanButtonMainGhostClearCounter > 0)
  {
    fanButtonMainGhostClearCounter--;
  }
}


// ============================================================================
// FAN-TOGGLE-BUTTON
// ============================================================================

static bool fanButtonDrawn = false;
static bool fanButtonLastEnabled = false;
static bool fanButtonLastMissing = false;
static bool fanButtonLastPressed = false;

static void clearFanButtonArea(void)
{
  // Dieser Clear ist nur im Setup zulaessig. Ein spaeter Aufruf nach dem
  // Bildschirmwechsel darf niemals in den bereits aufgebauten Hauptscreen
  // schneiden.
  if (!flag.config_mode) return;

  const int16_t x = But_Param[4].x;
  const int16_t y = But_Param[4].y;
  const int16_t w = But_Param[4].w;
  const int16_t h = But_Param[4].h;

  // Nur die reale Buttonflaeche mit kleiner Randreserve loeschen. Der alte
  // sehr grosse Sicherheitsbereich ueberdeckte den Messwertrahmen.
  tft.drawPixel(0, 0, BLACK);
  delayMicroseconds(10);
  tft.fillRect(x - 3, y - 3, w + 6, h + 6, BLACK);
  delayMicroseconds(40);
}

static void resetFanButtonCache(void)
{
  fanButtonDrawn = false;
  fanButtonLastEnabled = false;
  fanButtonLastMissing = false;
  fanButtonLastPressed = false;
}

// Kleiner Software-Kreis fuer die Status-LED im FAN-Button.
// Wichtig: NICHT tft.fillCircle() benutzen. Beim RA8875 bleiben dabei
// manchmal 1-Pixel-Reste stehen. Die horizontalen fillRect()-Zeilen
// sind bei diesem Display deutlich robuster.
static void fanButtonFillDotSoft(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
  for (int16_t dy = -r; dy <= r; dy++)
  {
    int16_t dx = 0;
    while ((int32_t)(dx + 1) * (dx + 1) + (int32_t)dy * dy <= (int32_t)r * r)
    {
      dx++;
    }
    tft.fillRect(cx - dx, cy + dy, (dx * 2) + 1, 1, color);
  }
}

static void fanButtonDrawDotSoft(int16_t cx, int16_t cy, uint16_t color)
{
  // Dickerer schwarzer Rand fuer besseren Kontrast auf dem blauen Button.
  // Schwarzer Aussenkreis jetzt etwas groesser und 1 Pixel tiefer gesetzt.
  // Dadurch sollte der Ring optisch gleichmaessiger um den farbigen
  // Innenkreis wirken.
  fanButtonFillDotSoft(cx - 1, cy,     14, BLACK);
  fanButtonFillDotSoft(cx,     cy,      9, color);
}

static uint16_t fanButtonDotColor(void)
{
  if (!sensorFanIsEnabled()) return WHITE;
  return sensorFanTachoMissing() ? YELLOW : GREEN;
}

static void drawFanToggleButton(bool pressed, bool forceRedraw)
{
  const int16_t x = But_Param[4].x;
  const int16_t y = But_Param[4].y;
  const int16_t w = But_Param[4].w;
  const int16_t h = But_Param[4].h;

  bool enabled = sensorFanIsEnabled();
  bool missing = sensorFanTachoMissing();

  if (!forceRedraw && fanButtonDrawn &&
      fanButtonLastEnabled == enabled &&
      fanButtonLastMissing == missing &&
      fanButtonLastPressed == pressed)
  {
    return;
  }

  uint16_t bg = pressed ? WHITE : BLUE;
  uint16_t fg = pressed ? BLACK : WHITE;
  uint16_t dot = pressed ? BLACK : fanButtonDotColor();
  tft.drawPixel(0, 0, BLACK);
  delayMicroseconds(10);

  tft.fillRoundRect(x, y, w, h, 10, bg);
  delayMicroseconds(10);
  tft.drawRoundRect(x, y, w, h, 10, WHITE);
  delayMicroseconds(10);

  // Statuspunkt 8 Pixel weiter rechts als vorher; mit dickerem schwarzem Rand.
  fanButtonDrawDotSoft(x + 28, y + (h / 2), dot);
  delayMicroseconds(10);

  // Zweizeilig, damit der Text sauber in den 110x120-Button passt:
  //   FAN
  //   ON / OFF
  // Die beiden Zeilen sitzen optisch um die Buttonmitte.
  tft.setFont(DroidSansMono_16);
  tft.setTextColor(fg, bg);

  const char* stateTxt = enabled ? "ON" : "OFF";
  const int16_t line1Y = y + (h / 2) - 23;
  const int16_t line2Y = y + (h / 2) + 3;

  tft.setCursor(x + 56, line1Y);
  delayMicroseconds(10);
  tft.print("FAN");
  delayMicroseconds(10);

  tft.setCursor(enabled ? (x + 62) : (x + 56), line2Y);
  delayMicroseconds(10);
  tft.print(stateTxt);
  delayMicroseconds(30);

  fanButtonDrawn = true;
  fanButtonLastEnabled = enabled;
  fanButtonLastMissing = missing;
  fanButtonLastPressed = pressed;
}


// ============================================================================
// SAFE BUTTON DRAW
// ============================================================================

void TXTtestButton(
 int16_t x,
 int16_t y,
 int16_t w,
 int16_t h,
 int16_t xoff,
 int16_t font,
 const char* text,
 int16_t onoff,
 uint16_t txtcol,
 uint16_t butcol)
{
 int16_t yoff = 0;

 // ------------------------------------------------------------------------
 // HARDWARE-RESET: Zwingt den RA8875 VOR dem Zeichnen in den Grafikmodus
 // ------------------------------------------------------------------------
 tft.drawPixel(0, 0, BLACK); 
 delayMicroseconds(10);

 // ------------------------------------------------------------------------
 // BUTTON FLÄCHE
 // ------------------------------------------------------------------------
 tft.fillRoundRect(x, y, w, h, 10, butcol);
 delayMicroseconds(10);
 tft.drawRoundRect(x, y, w, h, 10, WHITE);
 delayMicroseconds(10);

 // Der untere gelbe Grad-Kreis des Hauptscreens liegt geometrisch im EXIT-Button.
 // Falls der RA8875 beim Wechsel noch einen Alt-Pixel-Rest zeigt, wird genau
 // diese kleine Stelle mit der aktuellen Buttonfarbe ueberdeckt.
 if (font == 0xfff)
 {
   tft.fillRect(x + 72, y + 34, 22, 20, butcol);
   delayMicroseconds(10);
 }

 // ------------------------------------------------------------------------
 // FONT BERECHNUNG
 // ------------------------------------------------------------------------
 if (font == 0xf00 || font == 0x100)
 {
   tft.setFont(AwesomeF080_40);
   xoff = (w / 2) - 23;
   yoff = (h / 2) - 29;
 }
 else if (font == 0xfff)
 {
   tft.setFont(DroidSansMono_20);
   int tw = strlen(text) * 16;
   xoff = (w - tw) / 2;
   yoff = (h / 2) - 9;
 }
 else
 {
   tft.setFont(DroidSansMono_24);
   int tw = strlen(text) * 13;
   xoff = (w - tw) / 2;
   yoff = (h / 2) - 14;
 }
 if (xoff < 0) xoff = 0;
 if (yoff < 0) yoff = 0;

 // ------------------------------------------------------------------------
 // TEXT DRUCKEN
 // ------------------------------------------------------------------------
 tft.setCursor(x + xoff, y + yoff);
 tft.setTextColor(txtcol, butcol); 
 delayMicroseconds(10);
 tft.print(text);
 delayMicroseconds(30);
}


// ============================================================================
// READ BUTTONS
// ============================================================================

void ReadButtons(bool redraw)
{
    // Setup-Tasten duerfen niemals nach dem Umschalten auf Haupt-/Diagnose-
    // screen noch zeichnen oder loeschen. Einige Menuepfade koennen im selben
    // Durchlauf den Config-Modus beenden; ein spaeter ReadButtons()-Aufruf
    // wuerde sonst die grossen UP/DOWN/ENTER/EXIT-Flaechen in den Hauptscreen
    // schreiben.
    if (!flag.config_mode)
    {
        return;
    }

    // Nach dem SETUP-Druck keinerlei Menueaktion zulassen, solange der
    // ausloesende Finger noch auf dem Display liegt. Die Pruefung muss vor
    // dem refresh-Block erfolgen, weil dieser TouchZ einmal kuenstlich auf
    // false setzt und sonst eine echte Freigabe vortaeuschen wuerde.
    if (menu_input_blocked_until_release)
    {
        if (TouchZ)
        {
            flag.short_push = false;
            touch_scroll_aktiv = false;
            return;
        }

        menu_input_blocked_until_release = false;
        flag.short_push = false;
    }

    if (refresh)
    {
        TouchZ = false;
        refresh = false;
    }

    // EXIT wurde bereits verarbeitet; solange der Finger noch auf dem Display
    // liegt, keine weiteren Button-Redraws/Aktionen auslösen. Das verhindert
    // das Blau/Weiß-Flimmern beim Verlassen des Menüs.
    if (exit_button_latched && TouchZ)
    {
        return;
    }
    if (!TouchZ)
    {
        exit_button_latched = false;
    }

    // ------------------------------------------------------------------------
    // BOUNDS CHECK
    // ------------------------------------------------------------------------

    if (FirstBut < 0)
        FirstBut = 0;

    if (LastBut >= BUTTON_COUNT)
        LastBut = BUTTON_COUNT - 1;

    // Der zusaetzliche FAN-Button darf nur im Hauptmenue sichtbar und bedienbar sein.
    // In allen Untermenues, Info-/Lizenzseiten und Statusseiten wird er sofort
    // sauber geloescht, damit keine Reste im rechten Buttonbereich stehen bleiben.
    if (LastBut < 4)
    {
      if (fanButtonDrawn || Button[4] != 0)
      {
        clearFanButtonArea();
        resetFanButtonCache();
        Button[4] = 0;
      }
    }

     // ------------------------------------------------------------------------
  // DRAW IDLE
  // ------------------------------------------------------------------------
  for (int i = FirstBut; i <= LastBut; i++)
  {
    if ((Button[i] == 0 || redraw) && TouchZ == false)
    {
      if (i == 4)
      {
        drawFanToggleButton(false, (Button[i] == 0 || redraw));
        Button[i] = 1;
      }
      else
      {
        // Bestimme die Hintergrundfarbe dynamisch anhand des aktuellen Toggle-Zustands
        uint16_t bg_col = (ButAction[i] == 1) ? But_Param[i].onbutcol : But_Param[i].offbutcol;
        uint16_t tx_col = (ButAction[i] == 1) ? But_Param[i].ontxtcol : But_Param[i].offtxtcol;
        const char* txt = (ButAction[i] == 1) ? But_Param[i].ontext : But_Param[i].offtext;

        TXTtestButton(
          But_Param[i].x,
          But_Param[i].y,
          But_Param[i].w,
          But_Param[i].h,
          But_Param[i].xoff,
          But_Param[i].font,
          txt,
          But_Param[i].onoff,
          tx_col,
          bg_col
        );
        Button[i] = 1;
      }
    }
    else if (i == 4 && Button[i] == 1 && TouchZ == false)
    {
      // Tacho-Wechsel gruen/gelb auch ohne kompletten Button-Redraw nachfuehren.
      drawFanToggleButton(false, false);
    }


          // --------------------------------------------------------------------
  // TOUCH CHECK
  // --------------------------------------------------------------------
  bool inside =
  TouchX > But_Param[i].x &&
  TouchX < (But_Param[i].x + But_Param[i].w) &&
  TouchY > But_Param[i].y &&
  TouchY < (But_Param[i].y + But_Param[i].h);

  // WICHTIG: Button[i] darf NUR auf 0 gehen, wenn TouchZ aktiv ist
  if (Button[i] == 1 && inside && TouchZ)
  {
    if (i == 4)
    {
      drawFanToggleButton(true, true);
    }
    else
    {
      TXTtestButton(
        But_Param[i].x,
        But_Param[i].y,
        But_Param[i].w,
        But_Param[i].h,
        But_Param[i].xoff,
        But_Param[i].font,
        But_Param[i].ontext,
        But_Param[i].onoff,
        BLACK,
        WHITE
      );
    }
    
    // Setze auf 2 (Gedrückt-Marker), um es vom Release-Zustand (0) zu trennen
    Button[i] = 2; 

    switch (i)
    {
      case 0:
        if (ButAction[i] == 0) { flag.short_push = true; flag.menu_lcd_upd = false; }
        break;
      case 1:
      case 2:
        if (ButAction[i] == 0) { touch_scroll_aktiv = true; flag.menu_lcd_upd = false; }
        break;
      case 3:
        if (ButAction[i] == 0) {
          exit_button_latched = true;
          menu_exit_requested = true;
          flag.menu_lcd_upd = false;
          return;
        }
        break;
      case 4:
        sensorFanToggleEnabled();
        break;
    }

    if (But_Param[i].onoff == 1)
    {
      ButAction[i] = !ButAction[i];
    }
  }
 }

 // ------------------------------------------------------------------------
 // RELEASE (Nur ausführen, wenn der Finger das Display VERLASSEN hat!)
 // ------------------------------------------------------------------------
 if (TouchZ == false)
 {
   for (int i = FirstBut; i <= LastBut; i++)
   {
     // Nur zurücksetzen, wenn er vorher im Gedrückt-Zustand (2) war
     if (Button[i] == 2)
     {
       uint16_t bg_col = (ButAction[i] == 1) ? But_Param[i].onbutcol : But_Param[i].offbutcol;
       uint16_t tx_col = (ButAction[i] == 1) ? But_Param[i].ontxtcol : But_Param[i].offtxtcol;
       const char* txt = (ButAction[i] == 1) ? But_Param[i].ontext : But_Param[i].offtext;

       if (i == 4)
       {
         drawFanToggleButton(false, true);
       }
       else
       {
         TXTtestButton(
           But_Param[i].x,
           But_Param[i].y,
           But_Param[i].w,
           But_Param[i].h,
           But_Param[i].xoff,
           But_Param[i].font,
           txt,
           But_Param[i].onoff,
           tx_col,
           bg_col
         );
       }
       
       // Wieder sauber in den Idle-Zustand versetzen
       Button[i] = 1;
     }
   }
 }
}



// ============================================================================
// TOUCHSCREEN MANAGEMENT
// ============================================================================

void manage_Touchscreen(void)
{
    // Die alten Touch-Flags werden nicht mehr ausgewertet.
    // Die echte Button-/Menübedienung läuft direkt über ReadButtons().
}
