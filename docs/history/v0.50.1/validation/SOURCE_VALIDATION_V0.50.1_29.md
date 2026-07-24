# Source Validation – TP-3000 V0.50.1 / Build 0.50.1_29

- Ausgangsarchiv frisch entpackt: `Taupunktspiegel_StandV1_2026-07-13_V0.50.1_8.zip`.
- Build-ID in `TPsignedData.h` und Web-Info konsistent auf `0.50.1_29` gesetzt.
- Eingebettetes JavaScript mit Node.js syntaktisch geprüft.
- Testpayload: 1126 Zeichen Rohdaten, 1428 Zeichen URI-kodierter Fragmentteil, 1444 Zeichen vollständiger Deep Link.
- JavaScript-/URI-Rundlauf bestätigt: `decodeURIComponent(fragment) == ursprünglicher TP3Q3-Rohpayload`.
- Der erzeugte 105×105-Module-QR wurde extern aus PNG dekodiert; alle 1444 Zeichen stimmten exakt überein.
- Keine Leerzeichen oder Zeilenumbrüche zwischen `tp3000://verify#` und Fragmentdarstellung.
- `.tpqr`-Speicherpfad verwendet weiterhin den unveränderten Rohpayload.
- A4-Layouttest mit zwei 98-mm-Codes: beide Karten lagen vollständig innerhalb einer 297-mm-Seite; gemessene QR-Sektion ca. 235 mm hoch.
- Ein vollständiger Teensyduino-Build und der reale Samsung-/Android-Hardwaretest waren in dieser Umgebung nicht möglich.
