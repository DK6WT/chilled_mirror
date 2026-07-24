# TP3C1 V0.3 – Signierertopologien

Firmware: 0.50.1 / Build 0.50.1_47

Behoben wurde die zu enge Prüfung auf ausschließlich `CALIBRATION_LAB / CALIBRATION_LAB / MANUFACTURER_ROOT`. Der KeyGen darf alle drei Kalibrierdokumente entweder mit Hersteller-Root oder einem autorisierten Kalibrierlabor signieren. Der kompakte QR-Transport bildet diese vorhandenen Signaturen nun korrekt ab.

Unterstützt werden alle Kombinationen aus Root und einem gemeinsamen Labor. Mehrere verschiedene Laborzertifikate innerhalb eines TP3C1-Dokuments bleiben ausgeschlossen. Bei Root/Root/Root wird kein Laborzertifikat in Feld 6 transportiert.

Die signierten Originaldokumente und ihre kanonischen Bytes bleiben unverändert.
