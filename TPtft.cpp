/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPtft.cpp
 * Zweck: RA8875-Grafik- und TextBox-Hilfsklassen.
 *
 * Abgeleitet aus: PSWRtft.cpp
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2016 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#include "TPtft.h"
#include "TP_T.h"

// Macht die Hardware-Objektinstanz deines Displays verfügbar
extern RA8875 tft; 

// =============================================================================
//  Zeichengeometrie ohne sichtbare Testausgabe
// =============================================================================
static uint32_t tpFetchBitsUnsigned(const uint8_t *p, uint32_t index, uint32_t required)
{
  uint32_t val = 0;

  while (required)
  {
    uint8_t b = p[index >> 3];
    uint32_t avail = 8 - (index & 7);

    if (avail <= required)
    {
      val <<= avail;
      val |= b & ((1 << avail) - 1);
      index += avail;
      required -= avail;
    }
    else
    {
      b >>= avail - required;
      val <<= required;
      val |= b & ((1 << required) - 1);
      break;
    }
  }

  return val;
}

static int16_t tpFontCharAdvance(const ILI9341_t3_font_t &f, uint16_t c, int16_t fallback)
{
  uint32_t bitoffset = 0;

  if (c >= f.index1_first && c <= f.index1_last)
  {
    bitoffset = (uint32_t)(c - f.index1_first) * f.bits_index;
  }
  else if (c >= f.index2_first && c <= f.index2_last)
  {
    bitoffset = (uint32_t)(c - f.index2_first + f.index1_last - f.index1_first + 1) * f.bits_index;
  }
  else
  {
    return fallback;
  }

  const uint8_t *data = f.data + tpFetchBitsUnsigned(f.index, bitoffset, f.bits_index);

  // ILI9341_t3 Font-Encoding 0 = Bitmap-Glyph.
  if (tpFetchBitsUnsigned(data, 0, 3) != 0)
  {
    return fallback;
  }

  bitoffset = 3 + f.bits_width;      // Encoding + Breite
  bitoffset += f.bits_height;        // Hoehe
  bitoffset += f.bits_xoffset;       // X-Offset
  bitoffset += f.bits_yoffset;       // Y-Offset

  int16_t delta = (int16_t)tpFetchBitsUnsigned(data, bitoffset, f.bits_delta);
  return (delta > 0) ? delta : fallback;
}


// =============================================================================
//  Direkte UTF-8-Ausgabe fuer die erweiterten TP-3000-Droid-Sans-Mono-Fonts
// =============================================================================
static bool tpUtf8ToFontByte(const uint8_t* p, uint8_t* out, uint8_t* used)
{
  if (p == nullptr || out == nullptr || used == nullptr || p[0] == 0)
  {
    return false;
  }

  *used = 0;

  if      (p[0] == 0xC2 && p[1] == 0xB0) { *out = 0xB0; *used = 2; } // °
  else if (p[0] == 0xCE && p[1] == 0xA9) { *out = 0xB1; *used = 2; } // Ω
  else if (p[0] == 0xE2 && p[1] != 0 && p[2] != 0 && p[1] == 0x84 && p[2] == 0xA6) { *out = 0xB1; *used = 3; } // Ω
  else if (p[0] == 0xC3 && p[1] == 0xA4) { *out = 0xB2; *used = 2; } // ä
  else if (p[0] == 0xC3 && p[1] == 0xB6) { *out = 0xB3; *used = 2; } // ö
  else if (p[0] == 0xC3 && p[1] == 0xBC) { *out = 0xB4; *used = 2; } // ü
  else if (p[0] == 0xC3 && p[1] == 0x84) { *out = 0xB6; *used = 2; } // Ä
  else if (p[0] == 0xC3 && p[1] == 0x96) { *out = 0xB7; *used = 2; } // Ö
  else if (p[0] == 0xC3 && p[1] == 0x9C) { *out = 0xB8; *used = 2; } // Ü
  else if (p[0] == 0xC3 && p[1] == 0x9F) { *out = 0xB9; *used = 2; } // ß
  else if (p[0] == 0xC2 && p[1] == 0xB5) { *out = 0xBA; *used = 2; } // µ
  else if (p[0] == 0xCE && p[1] == 0xBC) { *out = 0xBA; *used = 2; } // μ
  else if (p[0] == 0xC2 && p[1] == 0xB1) { *out = 0xBB; *used = 2; } // ±
  else if (p[0] == 0xCE && p[1] == 0x94) { *out = 0xBC; *used = 2; } // Δ
  else if (p[0] == 0xE2 && p[1] != 0 && p[2] != 0 && p[1] == 0x89 && p[2] == 0xA4) { *out = 0xBD; *used = 3; } // ≤
  else if (p[0] == 0xE2 && p[1] != 0 && p[2] != 0 && p[1] == 0x89 && p[2] == 0xA5) { *out = 0xBE; *used = 3; } // ≥

  return (*used != 0);
}

void tpTftPrintUtf8(const char* text)
{
  if (text == nullptr) return;

  const uint8_t* p = (const uint8_t*)text;
  while (*p)
  {
    uint8_t out = 0;
    uint8_t used = 0;

    if (tpUtf8ToFontByte(p, &out, &used))
    {
      tft.write(out);
      p += used;
    }
    else
    {
      // ASCII und bereits lokal codierte Fontbytes 0xB0..0xBE unveraendert.
      tft.write(*p++);
    }
  }
}

// =============================================================================
// 1. LEERE HÜLLEN FÜR ALTE HF-METER (Verhindert Linker-Fehler zu 100%)
// =============================================================================
void PowerMeter::init(int16_t xcoord, int16_t ycoord, int16_t xlen, int16_t ylen, int16_t graphcolour, int16_t lowcolour, int16_t highcolour) {}
void PowerMeter::init(int16_t xcoord, int16_t ycoord, int16_t xlen, int16_t ylen) {}
void PowerMeter::scale(double s, char *range) {}
void PowerMeter::erase(void) {}
void PowerMeter::drawframe(int16_t colour) {}
void PowerMeter::printunits(int16_t colour, char *range) {}
void PowerMeter::drawscale(double scale, int16_t colour, char *range) {}
void PowerMeter::graph(double lowlevel, double highlevel, double maxlevel) {}
void PowerMeter::erasegraph(void) {}

void VSWRmeter::init(int16_t xcoord, int16_t ycoord, int16_t xlen, int16_t ylen, int16_t graphcolour, int16_t lowcolour, int16_t midcolour, int16_t highcolour) {}
void VSWRmeter::init(int16_t xcoord, int16_t ycoord, int16_t xlen, int16_t ylen) {}
void VSWRmeter::scale(void) {}
void VSWRmeter::erase(void) {}
void VSWRmeter::drawscale(int16_t colour) {}
void VSWRmeter::graph(double mid, double high, double swr) {}

void ModulationScope::init(int16_t xcoord, int16_t ycoord, int16_t xlen, int16_t ylen, int16_t bordercolour, int16_t tracecolour, int16_t peakcolour) {}
void ModulationScope::init(int16_t xcoord, int16_t ycoord, int16_t xlen, int16_t ylen) {}
void ModulationScope::erase(void) {}
void ModulationScope::rate(int16_t rate) {}
void ModulationScope::adddata(double level, double fullscale) {} // <-- HIER SAß DER FEHLER! JETZT RUHIGGESTELLT.
void ModulationScope::draw(void) {}
void ModulationScope::update(void) {}

// =============================================================================
// 2. DEINE VOLL FUNKTIONSFÄHIGE, UNBLOCKIERTE TEXTBOX-PUFFER-ENGINE
// =============================================================================
void TextBox::init(int16_t _col, int16_t _row, int16_t _xoffs, int16_t _yoffs, 
                   const ILI9341_t3_font_t &f, int16_t _fontColour, int16_t _blnkColour)
{
  xoffs = _xoffs; 
  yoffs = _yoffs;
  Col = _col;
  Row = _row;
  font = &f;
  fontColour = _fontColour;
  blnkColour = _blnkColour;
  
  tft.setFont(*font); 

  // Zeichenbreite direkt aus den Fontdaten ermitteln.
  // Dadurch wird beim Booten kein sichtbares schwarzes Test-"a" mehr ins Splash-Bild geschrieben.
  fontXsize = tpFontCharAdvance(*font, 'a', fontXsize);
  fontYsize = (fontXsize * 21) / 10; // wie bisher: ca. Faktor 2.1 fuer symmetrischen Zeilenabstand
}

void TextBox::transfer(void)
{ 
  uint16_t line, column, character = 0;
  tft.setFont(*font);
  
  for (line = 0; line < Row; line++)
  {
    for (column = 0; column < Col; column++)
    {
      // Ändert nur Zeichen auf dem TFT, die sich im virtuellen Puffer real verändert haben
      if (virt_lcd[character] != text_lcd[character]) 
      {
        tft.setTextColor(blnkColour);
        tft.setCursor(xoffs + fontXsize * column, yoffs + fontYsize * line);
        tft.print(text_lcd[character]); // Altes Zeichen unblockiert löschen (überschreiben)
        
        tft.setTextColor(fontColour);
        tft.setCursor(xoffs + fontXsize * column, yoffs + fontYsize * line);
        text_lcd[character] = virt_lcd[character];
        tft.print(text_lcd[character]); // Neues Zeichen sauber schreiben
      }
      character++;
      if (character >= TEXTBOXSIZE) character = TEXTBOXSIZE - 1; 
    }
  }
}

void TextBox::write(char ch)
{
  uint16_t virt_pos;
 
  virt_pos = virt_x + Col * virt_y; 
  if ((virt_pos >= TEXTBOXSIZE) || (virt_pos >= (Col * Row))) virt_pos = 0; 
  
  virt_lcd[virt_pos++] = ch; 
  virt_x = virt_pos;
  
  for (virt_y = 0; virt_y < Row && virt_x >= Col; virt_y++) 
  {
    virt_x -= Col;
  }
}

void TextBox::print(const char *ch_in)
{
  if (ch_in == nullptr) return;

  // Projekt-Sonderzeichen fuer die erweiterten DroidSansMono-Fonts:
  // 0xB0 = Gradzeichen, 0xB1 = Ohm/Omega,
  // 0xB2..0xB4 = ae/oe/ue, 0xB5 = kleines Prozentzeichen,
  // 0xB6..0xB9 = AE/OE/UE/ss,
  // 0xBA = Mikro, 0xBB = Plus/Minus, 0xBC = Delta,
  // 0xBD = kleiner/gleich, 0xBE = groesser/gleich.
  // Direkte lokale Bytes (z.B. "\xB0") und echte UTF-8-Zeichen werden
  // dadurch beide unterstuetzt.
  const uint8_t* p = (const uint8_t*)ch_in;

  while (*p)
  {
    uint8_t out = 0;
    uint8_t used = 0;

    if (tpUtf8ToFontByte(p, &out, &used))
    {
      write((char)out);
      p += used;
    }
    else
    {
      write((char)*p++);
    }
  }
}

void TextBox::setCursor(int16_t x, int16_t y)
{
  virt_x = x;
  virt_y = y;
}

void TextBox::clear(void)
{
  uint16_t lcdsize = Row * Col;
  if (lcdsize > TEXTBOXSIZE) lcdsize = TEXTBOXSIZE;
  
  for (uint16_t i = 0; i < lcdsize; i++) virt_lcd[i] = ' '; 
  virt_x = 0;
  virt_y = 0;
}

void TextBox::invalidate(void)
{
  // Wenn der echte TFT extern geloescht wurde, stimmt der
  // interne Sichtcache text_lcd nicht mehr. Dann wuerde gleicher
  // Menue-Text beim erneuten Betreten nicht neu gezeichnet.
  uint16_t lcdsize = Row * Col;
  if (lcdsize > TEXTBOXSIZE) lcdsize = TEXTBOXSIZE;

  for (uint16_t i = 0; i < lcdsize; i++)
  {
    text_lcd[i] = '\0';
  }
}
