# V0.50.1_11 Root-/Parser-Update

- Eingebettete Root-Key-ID: `6CC674A19758D153`
- SHA-256 des Root-SPKI:
  `6CC674A19758D153EC7D2E47FD8CB9E4BFB14BFD3B22F78AF037E14BFE20FA55`
- Kurve: NIST P-256 / prime256v1
- Der eingebettete Raw-Public-Key `X||Y` entspricht der beigefügten Datei
  `test_identity/TP3000_TEST_ROOT_PUBLIC.pem`.
- Das bereits ausgestellte Zertifikat `TP3000_00001_F472B57D.tpcert` wurde
  außerhalb des Pakets gegen diesen Root geprüft: Manifest-SHA-256 und
  ECDSA-P256-Signatur sind gültig.
- Der JSON-Parser akzeptiert nun vierstellige `\\uXXXX`-Escapes für ASCII,
  insbesondere das vom .NET-KeyTool erzeugte `\\u002B` in Base64-Texten.
- Kein privater Root-Schlüssel ist Bestandteil des Firmwarepakets.
