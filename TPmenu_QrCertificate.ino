/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPmenu_QrCertificate.ino
 * Zweck: Aktiven Kalibrierschein als zweiteiligen TP3C1-QR-Code am TFT anzeigen.
 *
 * Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "TPsignedCalibration.h"
#include "TPqrCode.h"

namespace
{
static constexpr size_t TP_TFT_QR_PART_CAPACITY = 3420U;
static constexpr int16_t TP_TFT_QR_AREA_LEFT = 140;
static constexpr int16_t TP_TFT_QR_AREA_RIGHT = 665;
static constexpr int16_t TP_TFT_QR_AREA_TOP = 0;
static constexpr int16_t TP_TFT_QR_AREA_BOTTOM = 480;
static constexpr uint8_t TP_TFT_QR_QUIET_MODULES = 4U;

static char tpTftQrPart1[TP_TFT_QR_PART_CAPACITY];
static char tpTftQrPart2[TP_TFT_QR_PART_CAPACITY];
static char tpTftQrDocumentId[20];
static char tpTftQrError[176];
static TpQrCode::Matrix tpTftQrMatrix;

static bool tpTftQrPrepared = false;
static bool tpTftQrReady = false;
static uint8_t tpTftQrPartIndex = 0U;
static uint16_t tpTftQrEnvelopeSize = 0U;
static uint16_t tpTftQrTransportSize = 0U;
static bool tpTftQrCompressed = false;

static size_t FLASHMEM tpTftQrUtf8CharBytes(const char* text, size_t length, size_t offset)
{
  if (text == nullptr || offset >= length) return 0U;
  const uint8_t first = static_cast<uint8_t>(text[offset]);
  size_t count = 1U;
  if ((first & 0xE0U) == 0xC0U) count = 2U;
  else if ((first & 0xF0U) == 0xE0U) count = 3U;
  else if ((first & 0xF8U) == 0xF0U) count = 4U;
  if (offset + count > length) return 1U;
  for (size_t i = 1U; i < count; ++i)
    if ((static_cast<uint8_t>(text[offset + i]) & 0xC0U) != 0x80U) return 1U;
  return count;
}

static size_t FLASHMEM tpTftQrCopyWrappedLine(const char* text,
                                               size_t length,
                                               size_t offset,
                                               char* output,
                                               size_t outputSize,
                                               size_t maxGlyphs)
{
  if (output == nullptr || outputSize == 0U) return length;
  output[0] = '\0';
  if (text == nullptr) return length;
  while (offset < length && text[offset] == ' ') ++offset;
  const size_t start = offset;
  size_t end = start;
  size_t lastSpace = (size_t)-1;
  size_t glyphs = 0U;
  while (end < length && glyphs < maxGlyphs)
  {
    if (text[end] == ' ') lastSpace = end;
    const size_t step = tpTftQrUtf8CharBytes(text, length, end);
    if (step == 0U) break;
    end += step;
    ++glyphs;
  }
  size_t next = end;
  if (end < length && lastSpace != (size_t)-1 && lastSpace > start)
  {
    end = lastSpace;
    next = lastSpace + 1U;
  }
  size_t count = end - start;
  if (count >= outputSize) count = outputSize - 1U;
  if (count > 0U) memcpy(output, text + start, count);
  output[count] = '\0';
  while (next < length && text[next] == ' ') ++next;
  return next;
}

static void FLASHMEM tpTftQrResetState()
{
  tpTftQrPrepared = false;
  tpTftQrReady = false;
  tpTftQrPartIndex = 0U;
  tpTftQrEnvelopeSize = 0U;
  tpTftQrTransportSize = 0U;
  tpTftQrCompressed = false;
  tpTftQrPart1[0] = '\0';
  tpTftQrPart2[0] = '\0';
  tpTftQrDocumentId[0] = '\0';
  tpTftQrError[0] = '\0';
  memset(&tpTftQrMatrix, 0, sizeof(tpTftQrMatrix));
}

static bool FLASHMEM tpTftQrEncodeSelectedPart()
{
  const char* part = tpTftQrPartIndex == 0U ? tpTftQrPart1 : tpTftQrPart2;
  tpTftQrError[0] = '\0';
  if (part == nullptr || part[0] == '\0')
  {
    snprintf(tpTftQrError, sizeof(tpTftQrError), "%s",
             ui_language == LANG_EN ? "QR part is empty" : "QR-Teil ist leer");
    return false;
  }

  if (!tpSignedCalibrationEncodeTp3c1Qr(
          part,
          tpTftQrMatrix,
          tpTftQrError,
          sizeof(tpTftQrError)))
  {
    if (tpTftQrError[0] == '\0')
      snprintf(tpTftQrError, sizeof(tpTftQrError), "%s",
               ui_language == LANG_EN ? "QR code could not be generated"
                                      : "QR-Code konnte nicht erzeugt werden");
    Serial.print("[TFT-QR] QR-Encoding fehlgeschlagen: ");
    Serial.println(tpTftQrError);
    return false;
  }
  return true;
}

static bool FLASHMEM tpTftQrPrepare()
{
  tpTftQrPrepared = true;
  tpTftQrReady = false;
  tpTftQrPartIndex = 0U;
  tpTftQrError[0] = '\0';

  if (!tpSignedCalibrationBuildActiveTp3c1Parts(
          tpTftQrPart1, sizeof(tpTftQrPart1),
          tpTftQrPart2, sizeof(tpTftQrPart2),
          tpTftQrDocumentId, sizeof(tpTftQrDocumentId),
          &tpTftQrEnvelopeSize,
          &tpTftQrTransportSize,
          &tpTftQrCompressed,
          tpTftQrError, sizeof(tpTftQrError)))
  {
    if (tpTftQrError[0] == '\0')
      snprintf(tpTftQrError, sizeof(tpTftQrError), "%s",
               ui_language == LANG_EN ? "Calibration QR is unavailable"
                                      : "Kalibrierschein-QR ist nicht verfügbar");
    Serial.print("[TFT-QR] Aufbau fehlgeschlagen: ");
    Serial.println(tpTftQrError);
    return false;
  }

  if (!tpTftQrEncodeSelectedPart()) return false;
  tpTftQrReady = true;
  return true;
}

static uint8_t FLASHMEM tpTftQrSelectScale(uint8_t matrixSize)
{
  const int16_t availableWidth = TP_TFT_QR_AREA_RIGHT - TP_TFT_QR_AREA_LEFT + 1;
  const int16_t availableHeight = TP_TFT_QR_AREA_BOTTOM - TP_TFT_QR_AREA_TOP;
  const uint16_t modules = static_cast<uint16_t>(matrixSize) +
                           static_cast<uint16_t>(TP_TFT_QR_QUIET_MODULES) * 2U;
  for (uint8_t scale = 4U; scale >= 2U; --scale)
  {
    if (modules * scale <= static_cast<uint16_t>(availableWidth) &&
        modules * scale <= static_cast<uint16_t>(availableHeight)) return scale;
  }
  return 0U;
}

static void FLASHMEM tpTftQrClearContentArea()
{
  tft.fillRect(TP_TFT_QR_AREA_LEFT, TP_TFT_QR_AREA_TOP,
               TP_TFT_QR_AREA_RIGHT - TP_TFT_QR_AREA_LEFT + 1,
               TP_TFT_QR_AREA_BOTTOM - TP_TFT_QR_AREA_TOP, BLACK);
  // Rechte Infospalte oberhalb des EXIT-Buttons loeschen. Die Buttons selbst
  // werden anschliessend zentral durch ReadButtons(true) neu gezeichnet.
  tft.fillRect(675, 0, 125, 345, BLACK);
}

static void FLASHMEM tpTftQrDrawSideInfo(uint8_t scale)
{
  // Die rechte Spalte ist nur 125 Pixel breit. Alle Texte erhalten rechts
  // bewusst mindestens 8 Pixel Abstand und werden bei Umlauten ueber den
  // UTF-8-Ausgabepfad gezeichnet. Dadurch laufen keine Bytes/Glyphen mehr
  // in den Displayrand oder in den EXIT-Button.
  static constexpr int16_t textX = 682;

  tft.setFont(DroidSansMono_14);
  tft.setTextColor(YELLOW, BLACK);
  tft.setCursor(textX, 28);
  tpTftPrintUtf8(ui_language == LANG_EN ? "CALIBRATION" : "KALIBRIER-");
  tft.setCursor(textX, 48);
  tpTftPrintUtf8(ui_language == LANG_EN ? "CERTIFICATE" : "SCHEIN QR");

  tft.setFont(DroidSansMono_11);
  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(textX, 88);
  tpTftPrintUtf8(ui_language == LANG_EN ? "Scan with" : "Mit App");
  tft.setCursor(textX, 104);
  tpTftPrintUtf8(ui_language == LANG_EN ? "TP-3000 app" : "scannen");

  if (tpTftQrDocumentId[0] != '\0')
  {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(textX, 140);
    tft.print("ID:");
    char shortId[9];
    memcpy(shortId, tpTftQrDocumentId, 8U);
    shortId[8] = '\0';
    tft.setCursor(textX, 156);
    tft.print(shortId);
  }

  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(textX, 198);
  tft.print("UP/DOWN");
  tft.setCursor(textX, 214);
  tpTftPrintUtf8(ui_language == LANG_EN ? "change code" : "Code wechseln");

  tft.setCursor(textX, 246);
  tft.print("ENTER");
  tft.setCursor(textX, 262);
  tpTftPrintUtf8(ui_language == LANG_EN ? "back to menu" : "zurück zum");
  if (ui_language != LANG_EN)
  {
    tft.setCursor(textX, 278);
    tpTftPrintUtf8("Menü");
  }

  char scaleText[20];
  snprintf(scaleText, sizeof(scaleText), "%ux%u px", (unsigned)scale, (unsigned)scale);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(textX, 302);
  tft.print(scaleText);

  // Teilanzeige direkt oberhalb des EXIT-Buttons.
  tft.setFont(DroidSansMono_14);
  tft.setTextColor(YELLOW, BLACK);
  tft.setCursor(textX, 326);
  tft.print(tpTftQrPartIndex == 0U ? "CODE 1/2" : "CODE 2/2");
}

static void FLASHMEM tpTftQrLeaveToMenu()
{
  // QR-Matrix und Seiteninformationen werden direkt auf den realen TFT
  // gezeichnet und liegen deshalb ausserhalb der VirtLCD-Puffer. Beim ENTER
  // muss der reale Bildschirm vollstaendig geloescht und jeder Button-/
  // Seiten-Cache invalidiert werden. Andernfalls bleiben QR-Zeichen hinter
  // dem neu gezeichneten Hauptmenue beziehungsweise FAN-Button sichtbar.
  menuButtonsResetAfterExit();
  tft.fillScreen(BLACK);
  delayMicroseconds(200);

  if (VirtLCDMenu != nullptr)
  {
    VirtLCDMenu->clear();
    VirtLCDMenu->invalidate();
  }
  if (VirtLCDMessage != nullptr)
  {
    VirtLCDMessage->clear();
    VirtLCDMessage->invalidate();
  }

  menuInvalidatePageDrawCache();
  tpTftQrResetState();
  menu_level = MENU_MAIN;
  frisch_geoeffnet = true;
  flag.menu_lcd_upd = false;
  flag.short_push = false;
  TouchZ = false;
  TouchX = 0;
  TouchY = 0;
}

static void FLASHMEM tpTftQrDrawMatrix()
{
  tpTftQrClearContentArea();
  const uint8_t scale = tpTftQrSelectScale(tpTftQrMatrix.size);
  if (scale == 0U)
  {
    snprintf(tpTftQrError, sizeof(tpTftQrError), "%s",
             ui_language == LANG_EN ? "QR code is too large for TFT"
                                    : "QR-Code ist für das TFT zu groß");
    tpTftQrReady = false;
    return;
  }

  const int16_t fullModules = static_cast<int16_t>(tpTftQrMatrix.size) +
                              static_cast<int16_t>(TP_TFT_QR_QUIET_MODULES) * 2;
  const int16_t qrPixels = fullModules * scale;
  const int16_t x = TP_TFT_QR_AREA_LEFT +
                    ((TP_TFT_QR_AREA_RIGHT - TP_TFT_QR_AREA_LEFT + 1) - qrPixels) / 2;
  const int16_t y = TP_TFT_QR_AREA_TOP +
                    ((TP_TFT_QR_AREA_BOTTOM - TP_TFT_QR_AREA_TOP) - qrPixels) / 2;

  tft.fillRect(x, y, qrPixels, qrPixels, WHITE);
  const int16_t matrixX = x + TP_TFT_QR_QUIET_MODULES * scale;
  const int16_t matrixY = y + TP_TFT_QR_QUIET_MODULES * scale;

  // Horizontale Laeufe reduzieren die Zahl der langsamen RA8875-Fuellaufrufe.
  for (uint8_t row = 0U; row < tpTftQrMatrix.size; ++row)
  {
    uint8_t column = 0U;
    while (column < tpTftQrMatrix.size)
    {
      while (column < tpTftQrMatrix.size &&
             !TpQrCode::getModule(tpTftQrMatrix, column, row)) ++column;
      const uint8_t runStart = column;
      while (column < tpTftQrMatrix.size &&
             TpQrCode::getModule(tpTftQrMatrix, column, row)) ++column;
      if (column > runStart)
      {
        tft.fillRect(matrixX + runStart * scale,
                     matrixY + row * scale,
                     (column - runStart) * scale,
                     scale,
                     BLACK);
      }
    }
  }

  tpTftQrDrawSideInfo(scale);
}

static void FLASHMEM tpTftQrDrawError()
{
  tpTftQrClearContentArea();
  tft.setFont(DroidSansMono_16);
  tft.setTextColor(RED, BLACK);
  tft.setCursor(170, 80);
  tpTftPrintUtf8(ui_language == LANG_EN ? "CALIBRATION QR UNAVAILABLE"
                                        : "KALIBRIERSCHEIN-QR NICHT VERFÜGBAR");

  tft.setFont(DroidSansMono_14);
  tft.setTextColor(WHITE, BLACK);
  const size_t length = strlen(tpTftQrError);
  size_t offset = 0U;
  int16_t y = 135;
  while (offset < length && y < 380)
  {
    char line[176];
    const size_t next = tpTftQrCopyWrappedLine(tpTftQrError, length, offset,
                                               line, sizeof(line), 42U);
    tft.setCursor(170, y);
    tpTftPrintUtf8(line);
    if (next <= offset) break;
    offset = next;
    y += 25;
  }

  tft.setTextColor(CYAN, BLACK);
  tft.setCursor(170, 420);
  tpTftPrintUtf8(ui_language == LANG_EN ? "ENTER: back to menu   EXIT: main screen"
                                        : "ENTER: zurück zum Menü   EXIT: Hauptscreen");
}
}

void FLASHMEM calibration_certificate_qr_menu(void)
{
  static bool fresh = true;

  if (VirtLCDMenu == nullptr || VirtLCDMessage == nullptr) return;

  FirstBut = 0;
  LastBut = 3;
  ReadButtons(false);

  if (!flag.config_mode)
  {
    fresh = true;
    tpTftQrResetState();
    TouchZ = false;
    return;
  }

  const uint8_t key = menuReadKey(220);
  if (key == MK_ENTER)
  {
    fresh = true;
    tpTftQrLeaveToMenu();
    return;
  }
  if (tpTftQrReady && (key == MK_UP || key == MK_DOWN))
  {
    tpTftQrPartIndex = tpTftQrPartIndex == 0U ? 1U : 0U;
    tpTftQrReady = tpTftQrEncodeSelectedPart();
    flag.menu_lcd_upd = false;
  }

  if (!tpTftQrPrepared)
  {
    tpTftQrPrepare();
    flag.menu_lcd_upd = false;
  }

  if (!flag.menu_lcd_upd || fresh)
  {
    fresh = false;
    flag.menu_lcd_upd = true;
    VirtLCDMenu->clear();
    VirtLCDMenu->transfer();
    VirtLCDMessage->clear();
    VirtLCDMessage->transfer();

    if (tpTftQrReady) tpTftQrDrawMatrix();
    else tpTftQrDrawError();
    ReadButtons(true);
  }
}
