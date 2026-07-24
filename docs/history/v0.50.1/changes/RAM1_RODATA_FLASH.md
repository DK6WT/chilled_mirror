# V0.50.1_10 – RAM1-Rodata-Test

Dieser Teststand verschiebt ausschließlich konstante, nicht zeitkritische Daten
der Geräteidentität aus RAM1 in den speicherabgebildeten QSPI-Flash des Teensy
4.1.

Verschoben wurden:

- Identity-Status-, Fehler-, Feld- und Formattexte
- HTML/CSS/JS-Vorlagen der Identity-Webseite
- öffentlicher Test-Root und SPKI-Präfix
- SHA-256-Rundentabelle
- micro-ecc-P-256-Kurvenparameter

Die Funktionen der lokalen micro-ecc-Bibliothek bleiben wie in V0.50.1_9 in
`.flashmem`. Die beiden großen temporären Identity-Puffer bleiben in RAM2.
Messung, Regelung und Safety wurden nicht verändert.

Nach dem Kompilieren sind insbesondere `RAM1: variables`, `RAM1: code`,
`padding` und `free for local variables` mit V0.50.1_9 zu vergleichen.
