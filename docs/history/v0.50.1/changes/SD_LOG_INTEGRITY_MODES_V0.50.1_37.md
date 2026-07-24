# TP-3000 – SD-Logintegritätsmodi ab Build 0.50.1_37

## Auswahl

| Wert | Menütext | Abgeschlossene Dateien |
|---:|---|---|
| 0 | Aus | bestehende Tages-`.CSV` |
| 1 | SHA-256 | nummerierte `.CSV` + `.CSV.sha256` |
| 2 | CSV zertifiziert | nummerierte `.CSV` + `.TPSIG` |
| 3 | TPLOG zertifiziert | ausschließlich `.TPLOG` |

Die Werte 0 bis 2 behalten ihre bisherige Bedeutung. Wert 3 ist neu. Das EEPROM-Feld
bleibt ein Byte groß; Layout und CRC-Position der Interface-Konfiguration ändern sich nicht.

## Zertifizierte Modi

Beide zertifizierten Varianten benötigen dieselbe vollständige Nachweiskette und verwenden
unverändert `TP3000-LOG-SIGNATURE-2`. Im CSV-Modus bleiben CSV und TPSIG getrennt.
Im TPLOG-Modus werden beide zunächst vollständig erzeugt und geprüft, in
`TP3000-LOG-CONTAINER-1` eingebettet und erst nach erfolgreicher Containerprüfung entfernt.
Die fertige TPLOG ist damit die einzige sichtbare Archivdatei.

## Laufender Abschnitt und Recovery

Während des Loggens wird in allen Integritätsmodi eine CSV-Arbeitsdatei geschrieben. Im
TPLOG-Modus ist sie eine interne Zwischenstufe. Nach Stromausfall wird sie bis zur letzten
vollständigen Zeile wiederhergestellt und anschließend als TPLOG mit gesetztem Recovery-Flag
versiegelt.

## Snapshot

Ein Web-Snapshot versiegelt den aktiven Abschnitt. Im TPLOG-Modus wird anschließend der
Name der fertigen TPLOG an den Downloadpfad übergeben; die nicht mehr vorhandene CSV wird
nicht geöffnet.

## Sequenzen

Eine vorhandene TPLOG reserviert ihren `_NNN`-Dateistamm auch nach einem Moduswechsel.
Dadurch kann beispielsweise eine spätere CSV-zertifizierte Datei nicht denselben Namen wie
eine bereits vorhandene TPLOG erhalten.
