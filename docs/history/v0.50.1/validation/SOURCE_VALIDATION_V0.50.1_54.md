# Source Validation – V0.50.1_54

- Basis frisch aus `Taupunktspiegel_StandV1_2026-07-18_V0.50.1_29.zip` entpackt.
- Interne Version: `0.50.1`.
- Build-ID: `0.50.1_54`.
- `/firmware-certificate-upload` ist im Request-Routing vorhanden.
- Derselbe Pfad ist jetzt zusätzlich in der Body-Whitelist des nicht blockierenden HTTP-Requestlesers enthalten.
- Der Handler erhält dadurch den vollständig gelesenen, per `Content-Length` begrenzten JSON-Body.
- Browser-JavaScript sendet weiterhin den reinen Dateiinhalt als `application/json`; kein Multipart-Parser erforderlich.
- Geräte-, Kopf-, System- und externer PDF-Uploadpfad bleiben unverändert.
- Statische Prüfungen, ZIP-Integrität und Vergleich nach frischem Entpacken bestanden.
- Vollständiger Teensyduino-Build und Hardwaretest stehen noch aus.
