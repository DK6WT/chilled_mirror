# TP-3000 V0.50.1_27 – Uploads und Systemkalibrierschein

## Direkte Web-Uploads

Die Verwaltungsseite akzeptiert vollständige signierte Pakete:

- `.tpdcal` → `POST /calibration-device-upload`
- `.tphcal` → `POST /calibration-head-upload`
- `.tpscal` → `POST /calibration-system-upload`

Jedes Paket wird vor der Aktivierung kryptografisch geprüft. Geräte- und Kopfjustierung werden zusätzlich mit den aktuell wirksamen Justierungswerten verglichen. Private Schlüssel werden niemals übertragen.

## Kalibrierschein

Die aktive `.tpscal` ist das Hauptdokument. Der Ausdruck zeigt `SCAL-...`, Systembindung, Systemkalibriermesswerte, Messunsicherheiten und Fachangaben. Geräte- und Kopfjustierung bleiben als gebundene Grundlagen sichtbar.

## Offline-QR

`TP3Q3` enthält Gerätejustierung, Kopfjustierung und Systemkalibrierung. Der Dokumenthash bindet die drei Manifest-SHA-256-Werte. Falls ein einzelner QR-Code nicht reicht, wird automatisch eine passende Aufteilung in zwei gemeinsam gebundene Teile versucht.

## Druck

Die Druckansicht erzwingt unabhängig von der Browser-Viewport-Erkennung zwei Spalten. Bei A4, Rand `Keine` und Skalierung `Standard` bleibt der Inhalt mittig.
