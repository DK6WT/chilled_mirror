# TP-3000 Setup-Beschriftung V0.50.1_65

## Ziel

Die Auswahl des Hauptscreen-Layouts soll direkt benennen, wie viele große Messwerte angezeigt werden. Der bisherige Begriff `Standard` war dafür nicht eindeutig.

## TFT-Setup

Unter `Setup > Anzeige > Hauptscreen Layout` werden angezeigt:

- Deutsch: `2 Werte`, `3 Werte`
- Englisch: `2 values`, `3 values`

Die Änderung betrifft ausschließlich die sichtbaren Sprachtexte in `TPlanguage.h`. Die internen Konstanten `MAIN_SCREEN_LAYOUT_STANDARD = 0` und `MAIN_SCREEN_LAYOUT_3VALUES = 1` bleiben aus Kompatibilitätsgründen unverändert.

## Web-Setup

Die Auswahl `screenLayout` verwendet dieselben sichtbaren Texte:

- Wert 0: `2 Werte` / `2 values`
- Wert 1: `3 Werte` / `3 values`

## Unverändert

EEPROM-Wert, Web-Parameter, Layoutumschaltung, Rahmen, Positionen, Schriftgrößen und alle Mess- und Sicherheitsfunktionen bleiben unverändert.
