# Source Validation V0.50.1_57

## Prüfstand

Geprüft wurde der frisch aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_32.zip` entpackte und ausschließlich für Build `0.50.1_57` geänderte Quellstand.

## Statische Prüfungen

- `TP_FIRMWARE_BUILD_ID_STRING` ist exakt `0.50.1_57`.
- In allen vier Anfrageexporten wird `client.restartBudget()` nach den Erzeugungs-/SD-Schritten und vor dem ersten HTTP-Header aufgerufen.
- Die Fehlerantworten der Geräte-/Kopf-, System- und Firmwareanfragen starten das Schreibbudget ebenfalls neu.
- `Content-Length` entspricht weiterhin exakt `bodyLength`; der JSON-Body wird unverändert aus `ethJsonBuf` übertragen.
- Requestpfade und Dateiendungen bleiben unverändert.
- Keine Änderung an kanonischer Bytefolge, Hashbildung oder ECDSA-Signatur.

## Paketprüfungen

- ZIP-Integrität geprüft.
- Erneut frisch entpackt und bytegleich mit dem Arbeitsverzeichnis verglichen.
- Keine privaten Schlüssel oder Firmware-Binaries enthalten.

## Noch ausstehend

- vollständiger Teensyduino-Build,
- reale Memory-Usage-Ausgabe,
- Hardwaretest: `.tpscalreq` über Browser laden und vollständig im KeyGen öffnen,
- optionaler Test der übrigen drei Anfrageexporte.
