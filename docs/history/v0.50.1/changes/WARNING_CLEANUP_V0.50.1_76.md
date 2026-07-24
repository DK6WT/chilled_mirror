# TP-3000 V0.50.1_76 – Bereinigung sinnvoller Compilerwarnungen

## Umfang

Basis ist `V0.50.1_75`. Geändert wurde ausschließlich eigener TP-3000-Projektcode. Der komplette Ordner `libraries/` wurde nicht verändert.

## Umgesetzte Korrekturen

1. **Sichere Textkopien**
   Projektinterne Hilfsfunktionen in `TPexternalCalibration.cpp`, `TPfirmwareIntegrity.cpp`, `TPsignedCalibration.cpp` und `TPcertifiedLog.cpp` kopieren jetzt längenbegrenzt, überlappungssicher und immer nullterminiert. Damit entfällt insbesondere die Selbstkopie-Warnung im zertifizierten Logpfad.

2. **Keine still gekürzten SD-Pfade**
   Archivpfade externer Kalibrierscheine und Pfade zu Firmwarezertifikaten werden vor dem Öffnen beziehungsweise Umbenennen vollständig geprüft. Zu lange oder formal ungültige Namen werden abgelehnt beziehungsweise übersprungen, statt einen abgeschnittenen Pfad zu verwenden.

3. **Alte Schnittstellenkonfigurationen wieder migriert**
   Sind beide aktuellen A/B-Slots ungültig, werden die vorhandenen CRC-geschützten Formate V8, V7, V6 und V5 in dieser Reihenfolge geprüft und in das aktuelle Format übernommen. Nach erfolgreicher Migration wird unmittelbar auch der zweite redundante Slot geschrieben.

4. **Begrenzte TFT- und Webtexte**
   Statuszeilen auf `Info / Gültigkeit` werden bewusst auf die verfügbare TFT-Zeilenbreite begrenzt. Lange Erfolgstexte der Webimporte besitzen größere Puffer, begrenzte dynamische Felder und eine geprüfte Kurzmeldung als Rückfall.

5. **Unbenutzter eigener Code bereinigt oder gekennzeichnet**
   Ein unbenutztes lokales Objekt und ein doppelter Helfer wurden entfernt. Bewusst aufbewahrte Legacy-Validierungen und EEPROM-Adresshelfer sind als `[[maybe_unused]]` markiert, sodass ihr Erhalt dokumentiert bleibt.

## Bewusst nicht geändert

- Keine Datei unter `libraries/`.
- Keine Änderung an Messalgorithmus, ADC-Konfiguration, Regelung, Kalibrierwerten, Zertifikatsformaten oder LED-Autoadaption.
- Die BMP581-Warnung `fifo_len may be used uninitialized` bleibt eine Fremdbibliothekswarnung und wurde entsprechend der Vorgabe nicht angefasst.
- Die Arduino/Teensy-Architekturmeldung einer lokalen Library ist Metadaten-/Toolchain-bezogen und wurde ebenfalls nicht durch Bibliotheksänderungen unterdrückt.
