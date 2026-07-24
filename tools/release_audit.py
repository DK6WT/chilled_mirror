#!/usr/bin/env python3
"""Static release-tree audit for the public TP-3000 V0.50.1 source package."""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path

EXPECTED_VERSION = "0.50.1"
EXPECTED_BUILD = "0.50.1_87"
EXPECTED_DATE_ISO = "2026-07-24"

ALLOWED_ROOT_MARKDOWN = {
    "README.md",
    "BUILDING.md",
    "CHANGELOG.md",
    "THIRD_PARTY_NOTICES.md",
}

REQUIRED_FILES = {
    ".gitattributes",
    ".gitignore",
    "README.md",
    "BUILDING.md",
    "CHANGELOG.md",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "TP-3000.ino",
    "TPsignedData.h",
    "TPethernet.ino",
    "TPqrCode.cpp",
    "TPqrCode.h",
    "external/GSL1680/README.md",
    "docs/README.md",
    "docs/release/RELEASE_NOTES_V0.50.1.md",
    "docs/release/SOURCE_PACKAGE_V0.50.1.md",
    "docs/release/VALIDATION_V0.50.1.md",
    "docs/release/LICENSE_AUDIT_V0.50.1.md",
    "LICENSES/GPL-3.0.txt",
    "LICENSES/MIT.txt",
    "LICENSES/BSD-2-Clause.txt",
    "LICENSES/BSD-3-Clause.txt",
    "LICENSES/Apache-2.0.txt",
    "LICENSES/CC-BY-SA-4.0.txt",
    "LICENSES/CC-BY-3.0-NOTICE.txt",
    "LICENSES/OFL-1.1.txt",
    "LICENSES/Zlib.txt",
    "LICENSES/PAKO-NOTICE.txt",
    "LICENSES/QRCode-for-JavaScript-NOTICE.txt",
    "LICENSES/Nayuki-QR-NOTICE.txt",
    "LICENSES/MICRO-ECC-NOTICE.txt",
    "LICENSES/BOSCH-BMP5-NOTICE.txt",
    "SD_CARD_TEMPLATE/LEDADAPT/TP3000_SYSTEM_GRUNDKURVE.CSV",
    "MANIFEST.sha256",
}

LOCAL_LIBRARY_LICENSES = {
    "libraries/ProtoCentral_ADS1262_32-bit_precision_ADC_Library/LICENSE",
    "libraries/ProtoCentral_ADS1262_32-bit_precision_ADC_Library/LICENSE.md",
    "libraries/RTC_RV3129_Arduino_Library/LICENSE.md",
    "libraries/SparkFun_BMP581_Arduino_Library/LICENSE.md",
    "libraries/SparkFun_BMP581_Arduino_Library/src/bmp5_api/LICENSE",
    "libraries/TP3000_micro_ecc/LICENSE.txt",
    "libraries/WDT_T4/LICENSE",
}

FORBIDDEN_SUFFIXES = {
    ".zip", ".7z", ".rar", ".tar", ".gz",
    ".hex", ".bin", ".elf", ".map", ".o", ".obj", ".a",
    ".so", ".dll", ".exe",
}

TEXT_SUFFIXES = {
    ".ino", ".cpp", ".c", ".h", ".hpp", ".md", ".txt", ".json",
    ".py", ".properties", ".yml", ".yaml", ".html", ".js", ".css",
    ".gitignore", ".gitattributes",
}

PRIVATE_KEY_MARKERS = (
    b"-----BEGIN " + b"PRIVATE KEY-----",
    b"-----BEGIN EC " + b"PRIVATE KEY-----",
    b"-----BEGIN RSA " + b"PRIVATE KEY-----",
    b"-----BEGIN OPENSSH " + b"PRIVATE KEY-----",
)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def audit(root: Path) -> list[str]:
    errors: list[str] = []

    if not root.is_dir():
        return [f"Kein Verzeichnis: {root}"]

    for rel in sorted(REQUIRED_FILES | LOCAL_LIBRARY_LICENSES):
        if not (root / rel).is_file():
            errors.append(f"Pflichtdatei fehlt: {rel}")

    metadata = root / "TPsignedData.h"
    if metadata.is_file():
        text = metadata.read_text(encoding="utf-8", errors="replace")
        expected = {
            "TP_FIRMWARE_VERSION_STRING": EXPECTED_VERSION,
            "TP_FIRMWARE_BUILD_ID_STRING": EXPECTED_BUILD,
            "TP_FIRMWARE_BUILD_DATE_ISO": EXPECTED_DATE_ISO,
        }
        for macro, value in expected.items():
            if not re.search(rf'#define\s+{re.escape(macro)}\s+"{re.escape(value)}"', text):
                errors.append(f"Release-Metadatum stimmt nicht: {macro} != {value}")

    web = root / "TPethernet.ino"
    if web.is_file():
        text = web.read_text(encoding="utf-8", errors="replace")
        if "Version 0.50.1 | Build 0.50.1_87" not in text:
            errors.append("Web-Lizenzseite enthält nicht Build 0.50.1_87")
        if "Version 0.50.1 | Build 0.50.1_47" in text:
            errors.append("Veraltete Web-Lizenzanzeige Build 0.50.1_47 gefunden")
        if "ethPakoDeflateLibrary" not in text or "pako=t()" not in text:
            errors.append("Eingebetteter pako-Block nicht gefunden")
        if "Copyright (c) 2009 Kazuhiko Arase" not in text:
            errors.append("QRCode-for-JavaScript-Copyright fehlt im Quellblock")

    qr = root / "TPqrCode.cpp"
    if qr.is_file() and "Project Nayuki" not in qr.read_text(encoding="utf-8", errors="replace"):
        errors.append("Project-Nayuki-Hinweis fehlt in TPqrCode.cpp")

    vendor_firmware = root / "external/GSL1680/gslX680_311_5_F.h"
    if vendor_firmware.exists():
        errors.append("Nicht weiterverteilbare GSL1680-Panel-Firmware ist enthalten")

    root_markdown = {p.name for p in root.glob("*.md")}
    extra_md = sorted(root_markdown - ALLOWED_ROOT_MARKDOWN)
    if extra_md:
        errors.append("Ungeordnete Markdown-Dateien auf Hauptebene: " + ", ".join(extra_md))

    all_files = [p for p in root.rglob("*") if p.is_file()]
    for path in all_files:
        rel = path.relative_to(root).as_posix()
        lower_name = path.name.lower()
        suffix = path.suffix.lower()

        if suffix in FORBIDDEN_SUFFIXES:
            errors.append(f"Unerlaubtes Archiv/Binary: {rel}")

        if lower_name.endswith("_private.pem") or "private_key" in lower_name:
            errors.append(f"Verdächtiger privater Schlüsselname: {rel}")

        # CSV is allowed only as a documented SD-card template.
        if suffix == ".csv" and not rel.startswith("SD_CARD_TEMPLATE/"):
            errors.append(f"Mess-/CSV-Datei außerhalb der SD-Vorlage: {rel}")

        try:
            data = path.read_bytes()
        except OSError as exc:
            errors.append(f"Datei nicht lesbar: {rel}: {exc}")
            continue

        if any(marker in data for marker in PRIVATE_KEY_MARKERS):
            errors.append(f"Privater Schlüsselinhalt gefunden: {rel}")

        is_text = suffix in TEXT_SUFFIXES or path.name in {"LICENSE", ".gitignore", ".gitattributes"}
        if is_text and b"\x00" in data:
            errors.append(f"NUL-Byte in Textdatei: {rel}")

    manifest = root / "MANIFEST.sha256"
    if manifest.is_file():
        listed: dict[str, str] = {}
        for line_no, line in enumerate(manifest.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if not line.strip():
                continue
            match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
            if not match:
                errors.append(f"Ungültige Manifestzeile {line_no}")
                continue
            digest, rel = match.groups()
            listed[rel] = digest
            target = root / rel
            if not target.is_file():
                errors.append(f"Manifestziel fehlt: {rel}")
            elif sha256(target) != digest:
                errors.append(f"Manifest-Hash falsch: {rel}")

        expected_paths = {
            p.relative_to(root).as_posix()
            for p in all_files
            if p.name != "MANIFEST.sha256"
        }
        missing = sorted(expected_paths - set(listed))
        extra = sorted(set(listed) - expected_paths)
        if missing:
            errors.append(f"Manifest unvollständig: {len(missing)} Datei(en) fehlen")
        if extra:
            errors.append(f"Manifest enthält {len(extra)} unbekannte Datei(en)")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default=".", help="Repository-Wurzel")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    errors = audit(root)
    if errors:
        print(f"TP-3000 Release-Audit: FEHLER ({len(errors)})")
        for error in errors:
            print(f"- {error}")
        return 1
    files = sum(1 for p in root.rglob("*") if p.is_file())
    print(f"TP-3000 Release-Audit: OK ({files} Dateien, Build {EXPECTED_BUILD})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
