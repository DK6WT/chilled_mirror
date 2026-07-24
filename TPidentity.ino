/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPidentity.ino
 * Zweck: Test-Provisionierung fuer individuellen P-256-Geraeteschluessel,
 *        selbstsignierte .tpreq-Anfrage und Root-signiertes .tpcert.
 *
 * WICHTIG: Diese erste Firmwareintegration verwendet bewusst den oeffentlichen
 * Wegwerf-Test-Root aus dem TP3000KeyTool-Test. Vor einer Produktivfreigabe
 * muss der Produktiv-Root eingesetzt und der komplette Ablauf erneut geprueft
 * werden. Der private Root-Schluessel ist hier niemals enthalten.
 *
 * RAM-Optimierung V0.50.1_10:
 * Alle Funktionen dieser Datei werden aus QSPI-Flash ausgeführt. Die beiden
 * großen temporären Arbeitsbuffer liegen in RAM2. Die Routinen laufen nur bei
 * Start, Provisionierung, Zertifikatsimport oder Statusabfrage und sind nicht
 * Bestandteil der zeitkritischen ADC-/Regel-/Safety-Pfade.
 * Konstante Texte, SHA-Tabellen und P-256-Kurvendaten bleiben im QSPI-Flash.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <Entropy.h>
#include <TP3000_uECC.h>
#include "TPsha256.h"

#if !defined(ARDUINO_TEENSY41)
#error "TP-3000 Identity-Provisionierung benoetigt Teensy 4.1 (4284 Byte EEPROM)."
#endif

// -----------------------------------------------------------------------------
// Formate, EEPROM-Aufteilung und Test-Root
// -----------------------------------------------------------------------------
#if defined(__IMXRT1062__)
#define TP_IDENTITY_RODATA __attribute__((section(".progmem")))
#else
#define TP_IDENTITY_RODATA
#endif

static const char IDENTITY_REQUEST_FORMAT[] TP_IDENTITY_RODATA = "TP3000-CERTIFICATE-REQUEST-1";
static const char IDENTITY_CERT_FORMAT[] TP_IDENTITY_RODATA = "TP3000-DEVICE-CERTIFICATE-2";
static const char IDENTITY_DEVICE_TYPE[] TP_IDENTITY_RODATA = "TP-3000";
static const char IDENTITY_SIGNATURE_ALGORITHM[] TP_IDENTITY_RODATA = "ECDSA-P256-SHA256";
static const char IDENTITY_SIGNATURE_ENCODING[] TP_IDENTITY_RODATA = "IEEE-P1363";
static const char IDENTITY_TEST_ROOT_KEY_ID[] TP_IDENTITY_RODATA = "6CC674A19758D153";
static const char IDENTITY_SD_DIRECTORY[] TP_IDENTITY_RODATA = "/IDENTITY";

// Oeffentlicher P-256-Test-Root, raw X||Y, je 32 Byte Big-Endian.
// Fingerprint/Key-ID: 6CC674A19758D153
static const uint8_t identityTestRootPublicKey[64] TP_IDENTITY_RODATA =
{
  0x87, 0xAE, 0xD5, 0x76, 0x54, 0xCE, 0xD8, 0x96,
  0xB1, 0xBB, 0xD9, 0x4A, 0xD0, 0xBE, 0x01, 0x61,
  0x19, 0xFD, 0x56, 0x2C, 0x38, 0x5C, 0xC2, 0x47,
  0x30, 0xFE, 0xB2, 0x85, 0x13, 0xE3, 0xB6, 0x86,
  0xA5, 0x03, 0xDF, 0x52, 0xE7, 0x4A, 0xB8, 0x38,
  0x0C, 0xC8, 0xF5, 0xD1, 0x37, 0x93, 0x5A, 0x5C,
  0x80, 0xAD, 0x3C, 0x76, 0x27, 0x21, 0xEC, 0xB2,
  0xB2, 0xC1, 0x50, 0x40, 0xBF, 0xCB, 0x23, 0x24
};

// Die bisherigen EEPROM-Bloecke enden bei 3712. Teensy 4.1 stellt 4284 Byte
// emuliertes EEPROM bereit. Die verbleibenden 572 Byte werden exakt als
// A/B-Slots fuer Key und kompaktes Zertifikat genutzt.
#define IDENTITY_KEY_SLOT_A_ADDR   3712
#define IDENTITY_KEY_SLOT_B_ADDR   3760
#define IDENTITY_CERT_SLOT_A_ADDR  3808
#define IDENTITY_CERT_SLOT_B_ADDR  4046
#define IDENTITY_EEPROM_END        4284

#define IDENTITY_KEY_MAGIC         0x544B4559UL  // "TKEY"
#define IDENTITY_KEY_VERSION       1U
#define IDENTITY_CERT_MAGIC        0x54434552UL  // "TCER"
#define IDENTITY_CERT_VERSION      2U

#pragma pack(push, 1)
struct IdentityKeySlot
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  uint8_t privateKey[32];
  uint32_t crc32;
};

struct IdentityCertificateSlot
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  char deviceSerial[6];
  uint8_t deviceKeyId[8];
  uint8_t issuerKeyId[8];
  uint8_t certificateSerial[16];
  int64_t issuedUtc;
  uint8_t requestId[16];
  uint8_t requestManifestSha256[32];
  char requestFirmwareVersion[32];
  uint8_t manifestSha256[32];
  uint8_t rootSignature[64];
  uint32_t crc32;
};
#pragma pack(pop)

static_assert(sizeof(IdentityKeySlot) == 48U,
              "IdentityKeySlot muss exakt 48 Byte haben");
static_assert(sizeof(IdentityCertificateSlot) == 238U,
              "IdentityCertificateSlot muss exakt 238 Byte haben");
static_assert(IDENTITY_CERT_SLOT_B_ADDR + sizeof(IdentityCertificateSlot) == IDENTITY_EEPROM_END,
              "TP-3000 Identity-EEPROM-Aufteilung stimmt nicht");

static IdentityKeySlot identityKeySlot;
static IdentityCertificateSlot identityCertificateSlot;
static bool identityKeyValid = false;
static bool identityCertificateValidFlag = false;
static bool identityCertificateStoredButInvalid = false;
static uint8_t identityPublicKey[64] = {0};
static uint8_t identitySpki[91] = {0};
static char identityDeviceKeyIdText[17] = {0};
static char identityCertifiedSerialText[6] = {0};
static char identityCertificateSerialTextBuffer[33] = {0};
static char identityLastCertificateFile[64] = {0};
static bool identityEntropyInitialized = false;

// Große, nur bei Provisionierung/Zertifikatsprüfung benötigte Arbeitsbuffer
// liegen in RAM2. Damit bleibt RAM1 für Stack, Regelung und Safety frei.
static DMAMEM char identityCertificateJson[4096];
static DMAMEM uint8_t identityCanonicalBuffer[768];

// P-256 SubjectPublicKeyInfo-Praefix bis einschliesslich 0x04 des
// unkomprimierten EC-Punktes. Danach folgen X||Y mit 64 Byte.
static const uint8_t identitySpkiPrefix[27] TP_IDENTITY_RODATA =
{
  0x30, 0x59,
  0x30, 0x13,
  0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
  0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,
  0x03, 0x42, 0x00, 0x04
};

// Alle nur fuer Provisionierung/Webstatus benoetigten Texte bleiben im QSPI-Flash.
struct IdentityFlashTextTable
{
  char t000[sizeof("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")];
  char t001[sizeof("{\n  \"Format\": \"%s\",\n  \"DeviceType\": \"%s\",\n  \"RequestedDeviceSerial\": \"%s\",\n  \"DeviceKeyId\": \"%s\",\n  \"DevicePublicKeySpkiBase64\": \"%s\",\n  \"RequestId\": \"%s\",\n  \"FirmwareVersion\": \"%s\",\n  \"CreatedUtc\": \"%s\",\n  \"ManifestSha256\": \"%s\",\n  \"SignatureAlgorithm\": \"%s\",\n  \"SignatureEncoding\": \"%s\",\n  \"DeviceSelfSignatureBase64\": \"%s\"\n}\n")];
  char t002[sizeof("Pflichtfeld fehlt oder ist ungueltig: %s")];
  char t003[sizeof("Unbekannter Fehler")];
  char t004[sizeof("")];
  char t005[sizeof("Zertifiziert (TEST-ROOT)")];
  char t006[sizeof("Zertifikat gespeichert, aber ungueltig")];
  char t007[sizeof("Geraeteschluessel vorhanden, noch nicht zertifiziert")];
  char t008[sizeof("Noch kein Geraeteschluessel")];
  char t009[sizeof("Geraet ist bereits zertifiziert; Schluesselwechsel gesperrt")];
  char t010[sizeof("Geraeteschluessel ist bereits vorhanden")];
  char t011[sizeof("00000")];
  char t012[sizeof("Zuerst eine gueltige Geraete-SN ungleich 00000 einstellen")];
  char t013[sizeof("Bestaetigte SN stimmt nicht mit der eingestellten Geraete-SN ueberein")];
  char t014[sizeof("P-256-Geraeteschluessel konnte nicht erzeugt werden")];
  char t015[sizeof("Geraeteschluessel konnte nicht sicher im EEPROM gespeichert werden")];
  char t016[sizeof("OK")];
  char t017[sizeof("%04d-%02d-%02dT%02d:%02d:%02d+00:00")];
  char t018[sizeof("Noch kein Geraeteschluessel vorhanden")];
  char t019[sizeof("Geraet ist bereits zertifiziert")];
  char t020[sizeof("Ausgabepuffer fuer .tpreq ist zu klein")];
  char t021[sizeof("Geraete-SN ist ungueltig oder 00000")];
  char t022[sizeof("Zufaellige Anfrage-ID konnte nicht erzeugt werden")];
  char t023[sizeof("Kanonischer .tpreq-Datensatz ist zu gross")];
  char t024[sizeof("Selbstsignatur der Zertifikatsanfrage fehlgeschlagen")];
  char t025[sizeof(".tpreq-JSON passt nicht in den Ausgabepuffer")];
  char t026[sizeof("TP3000_%s_CERT_REQUEST.tpreq")];
  char t027[sizeof("Keine gueltige .tpreq zum Speichern")];
  char t028[sizeof("SD-Verzeichnis /IDENTITY konnte nicht erzeugt werden")];
  char t029[sizeof("%s/%s")];
  char t030[sizeof(".tpreq konnte auf SD nicht angelegt werden")];
  char t031[sizeof(".tpreq wurde nicht vollstaendig auf SD geschrieben")];
  char t032[sizeof("\"%s\"")];
  char t033[sizeof("Geraeteschluessel oder Zertifikatsdaten fehlen")];
  char t034[sizeof("Format")];
  char t035[sizeof("DeviceType")];
  char t036[sizeof("DeviceSerial")];
  char t037[sizeof("DeviceKeyId")];
  char t038[sizeof("DevicePublicKeySpkiBase64")];
  char t039[sizeof("IssuerKeyId")];
  char t040[sizeof("CertificateSerial")];
  char t041[sizeof("IssuedUtc")];
  char t042[sizeof("RequestId")];
  char t043[sizeof("RequestManifestSha256")];
  char t044[sizeof("RequestFirmwareVersion")];
  char t045[sizeof("ManifestSha256")];
  char t046[sizeof("SignatureAlgorithm")];
  char t047[sizeof("SignatureEncoding")];
  char t048[sizeof("RootSignatureBase64")];
  char t049[sizeof("Zertifikatsformat oder Signaturalgorithmus wird nicht unterstuetzt")];
  char t050[sizeof("Zertifikat enthaelt keine gueltige 5-stellige Geraete-SN")];
  char t051[sizeof("Firmwareversion aus der Anfrage ist ungueltig oder zu lang")];
  char t052[sizeof("Zertifikat gehoert nicht zum internen Geraeteschluessel")];
  char t053[sizeof("Zertifikat wurde nicht vom eingebauten TP-3000-Test-Root ausgestellt")];
  char t054[sizeof("Public Key im Zertifikat passt nicht zum TP-3000")];
  char t055[sizeof("Hex-Feld im Zertifikat hat eine ungueltige Laenge oder Zeichen")];
  char t056[sizeof("IssuedUtc im Zertifikat ist ungueltig")];
  char t057[sizeof("Root-Signatur im Zertifikat hat nicht 64 Byte")];
  char t058[sizeof("Kanonischer Zertifikatsdatensatz ist zu gross")];
  char t059[sizeof("Zertifikats-Manifest-SHA-256 stimmt nicht")];
  char t060[sizeof("Root-Signatur des Zertifikats ist ungueltig")];
  char t061[sizeof(".tpcert ist leer, zu gross oder nicht vollstaendig lesbar")];
  char t062[sizeof("Noch kein interner Geraeteschluessel vorhanden")];
  char t063[sizeof("SD-Verzeichnis /IDENTITY fehlt; .tpcert dort ablegen")];
  char t064[sizeof("Keine passende .tpcert gefunden")];
  char t065[sizeof(".tpcert")];
  char t066[sizeof("Gueltiges Zertifikat konnte nicht sicher im EEPROM gespeichert werden")];
  char t067[sizeof("Zertifikat ist nach EEPROM-Ruecklesung nicht mehr gueltig")];
  char t068[sizeof("Keine .tpcert-Datei im SD-Verzeichnis /IDENTITY gefunden")];
};

static const IdentityFlashTextTable identityFlashText TP_IDENTITY_RODATA =
{
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
  "{\n  \"Format\": \"%s\",\n  \"DeviceType\": \"%s\",\n  \"RequestedDeviceSerial\": \"%s\",\n  \"DeviceKeyId\": \"%s\",\n  \"DevicePublicKeySpkiBase64\": \"%s\",\n  \"RequestId\": \"%s\",\n  \"FirmwareVersion\": \"%s\",\n  \"CreatedUtc\": \"%s\",\n  \"ManifestSha256\": \"%s\",\n  \"SignatureAlgorithm\": \"%s\",\n  \"SignatureEncoding\": \"%s\",\n  \"DeviceSelfSignatureBase64\": \"%s\"\n}\n",
  "Pflichtfeld fehlt oder ist ungueltig: %s",
  "Unbekannter Fehler",
  "",
  "Zertifiziert (TEST-ROOT)",
  "Zertifikat gespeichert, aber ungueltig",
  "Geraeteschluessel vorhanden, noch nicht zertifiziert",
  "Noch kein Geraeteschluessel",
  "Geraet ist bereits zertifiziert; Schluesselwechsel gesperrt",
  "Geraeteschluessel ist bereits vorhanden",
  "00000",
  "Zuerst eine gueltige Geraete-SN ungleich 00000 einstellen",
  "Bestaetigte SN stimmt nicht mit der eingestellten Geraete-SN ueberein",
  "P-256-Geraeteschluessel konnte nicht erzeugt werden",
  "Geraeteschluessel konnte nicht sicher im EEPROM gespeichert werden",
  "OK",
  "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
  "Noch kein Geraeteschluessel vorhanden",
  "Geraet ist bereits zertifiziert",
  "Ausgabepuffer fuer .tpreq ist zu klein",
  "Geraete-SN ist ungueltig oder 00000",
  "Zufaellige Anfrage-ID konnte nicht erzeugt werden",
  "Kanonischer .tpreq-Datensatz ist zu gross",
  "Selbstsignatur der Zertifikatsanfrage fehlgeschlagen",
  ".tpreq-JSON passt nicht in den Ausgabepuffer",
  "TP3000_%s_CERT_REQUEST.tpreq",
  "Keine gueltige .tpreq zum Speichern",
  "SD-Verzeichnis /IDENTITY konnte nicht erzeugt werden",
  "%s/%s",
  ".tpreq konnte auf SD nicht angelegt werden",
  ".tpreq wurde nicht vollstaendig auf SD geschrieben",
  "\"%s\"",
  "Geraeteschluessel oder Zertifikatsdaten fehlen",
  "Format",
  "DeviceType",
  "DeviceSerial",
  "DeviceKeyId",
  "DevicePublicKeySpkiBase64",
  "IssuerKeyId",
  "CertificateSerial",
  "IssuedUtc",
  "RequestId",
  "RequestManifestSha256",
  "RequestFirmwareVersion",
  "ManifestSha256",
  "SignatureAlgorithm",
  "SignatureEncoding",
  "RootSignatureBase64",
  "Zertifikatsformat oder Signaturalgorithmus wird nicht unterstuetzt",
  "Zertifikat enthaelt keine gueltige 5-stellige Geraete-SN",
  "Firmwareversion aus der Anfrage ist ungueltig oder zu lang",
  "Zertifikat gehoert nicht zum internen Geraeteschluessel",
  "Zertifikat wurde nicht vom eingebauten TP-3000-Test-Root ausgestellt",
  "Public Key im Zertifikat passt nicht zum TP-3000",
  "Hex-Feld im Zertifikat hat eine ungueltige Laenge oder Zeichen",
  "IssuedUtc im Zertifikat ist ungueltig",
  "Root-Signatur im Zertifikat hat nicht 64 Byte",
  "Kanonischer Zertifikatsdatensatz ist zu gross",
  "Zertifikats-Manifest-SHA-256 stimmt nicht",
  "Root-Signatur des Zertifikats ist ungueltig",
  ".tpcert ist leer, zu gross oder nicht vollstaendig lesbar",
  "Noch kein interner Geraeteschluessel vorhanden",
  "SD-Verzeichnis /IDENTITY fehlt; .tpcert dort ablegen",
  "Keine passende .tpcert gefunden",
  ".tpcert",
  "Gueltiges Zertifikat konnte nicht sicher im EEPROM gespeichert werden",
  "Zertifikat ist nach EEPROM-Ruecklesung nicht mehr gueltig",
  "Keine .tpcert-Datei im SD-Verzeichnis /IDENTITY gefunden"
};

// -----------------------------------------------------------------------------
// Kleine Hilfsfunktionen
// -----------------------------------------------------------------------------
static uint32_t FLASHMEM identityCrc32(const void* data, size_t length)
{
  const uint8_t* p = (const uint8_t*)data;
  uint32_t crc = 0xFFFFFFFFUL;
  while (length-- > 0U)
  {
    crc ^= *p++;
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

static bool FLASHMEM identityBytesEqual(const uint8_t* a, const uint8_t* b, size_t length)
{
  if (a == nullptr || b == nullptr) return false;
  uint8_t difference = 0U;
  for (size_t i = 0U; i < length; i++) difference |= (uint8_t)(a[i] ^ b[i]);
  return difference == 0U;
}

static bool FLASHMEM identityAllZero(const uint8_t* data, size_t length)
{
  if (data == nullptr) return true;
  uint8_t combined = 0U;
  for (size_t i = 0U; i < length; i++) combined |= data[i];
  return combined == 0U;
}

static bool FLASHMEM identitySequenceNewer(uint32_t left, uint32_t right)
{
  return (int32_t)(left - right) > 0;
}

static void FLASHMEM identitySetError(char* errorText, size_t errorTextSize, const char* message)
{
  if (errorText == nullptr || errorTextSize == 0U) return;
  if (message == nullptr) message = identityFlashText.t003;
  strncpy(errorText, message, errorTextSize - 1U);
  errorText[errorTextSize - 1U] = '\0';
}

static char FLASHMEM identityHexDigit(uint8_t value, bool upper)
{
  value &= 0x0FU;
  if (value < 10U) return (char)('0' + value);
  return (char)((upper ? 'A' : 'a') + (value - 10U));
}

static void FLASHMEM identityBytesToHex(const uint8_t* data,
                               size_t length,
                               char* output,
                               size_t outputSize,
                               bool upper)
{
  if (output == nullptr || outputSize == 0U) return;
  if (data == nullptr || outputSize < length * 2U + 1U)
  {
    output[0] = '\0';
    return;
  }

  for (size_t i = 0U; i < length; i++)
  {
    output[i * 2U] = identityHexDigit((uint8_t)(data[i] >> 4U), upper);
    output[i * 2U + 1U] = identityHexDigit(data[i], upper);
  }
  output[length * 2U] = '\0';
}

static int8_t FLASHMEM identityHexValue(char c)
{
  if (c >= '0' && c <= '9') return (int8_t)(c - '0');
  if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
  if (c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
  return -1;
}

static bool FLASHMEM identityHexToBytes(const char* text, uint8_t* output, size_t outputLength)
{
  if (text == nullptr || output == nullptr) return false;
  for (size_t i = 0U; i < outputLength; i++)
  {
    const int8_t high = identityHexValue(text[i * 2U]);
    const int8_t low = identityHexValue(text[i * 2U + 1U]);
    if (high < 0 || low < 0) return false;
    output[i] = (uint8_t)(((uint8_t)high << 4U) | (uint8_t)low);
  }
  return text[outputLength * 2U] == '\0';
}

static size_t FLASHMEM identityBase64EncodedLength(size_t inputLength)
{
  return ((inputLength + 2U) / 3U) * 4U;
}

static bool FLASHMEM identityBase64Encode(const uint8_t* input,
                                 size_t inputLength,
                                 char* output,
                                 size_t outputSize)
{
  const size_t required = identityBase64EncodedLength(inputLength);
  if (input == nullptr || output == nullptr || outputSize < required + 1U) return false;

  size_t in = 0U;
  size_t out = 0U;
  while (in < inputLength)
  {
    const uint32_t a = input[in++];
    const bool haveB = in < inputLength;
    const uint32_t b = haveB ? input[in++] : 0U;
    const bool haveC = in < inputLength;
    const uint32_t c = haveC ? input[in++] : 0U;
    const uint32_t triple = (a << 16U) | (b << 8U) | c;

    output[out++] = identityFlashText.t000[(triple >> 18U) & 0x3FU];
    output[out++] = identityFlashText.t000[(triple >> 12U) & 0x3FU];
    output[out++] = haveB ? identityFlashText.t000[(triple >> 6U) & 0x3FU] : '=';
    output[out++] = haveC ? identityFlashText.t000[triple & 0x3FU] : '=';
  }
  output[out] = '\0';
  return true;
}

static int8_t FLASHMEM identityBase64Value(char c)
{
  if (c >= 'A' && c <= 'Z') return (int8_t)(c - 'A');
  if (c >= 'a' && c <= 'z') return (int8_t)(c - 'a' + 26);
  if (c >= '0' && c <= '9') return (int8_t)(c - '0' + 52);
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static bool FLASHMEM identityBase64Decode(const char* input,
                                 uint8_t* output,
                                 size_t outputSize,
                                 size_t* outputLength)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (input == nullptr || output == nullptr) return false;

  const size_t length = strlen(input);
  if (length == 0U || (length & 3U) != 0U) return false;

  size_t out = 0U;
  for (size_t i = 0U; i < length; i += 4U)
  {
    const int8_t a = identityBase64Value(input[i]);
    const int8_t b = identityBase64Value(input[i + 1U]);
    const bool padC = input[i + 2U] == '=';
    const bool padD = input[i + 3U] == '=';
    const int8_t c = padC ? 0 : identityBase64Value(input[i + 2U]);
    const int8_t d = padD ? 0 : identityBase64Value(input[i + 3U]);

    if (a < 0 || b < 0 || c < 0 || d < 0) return false;
    if (padC && !padD) return false;
    if ((padC || padD) && i + 4U != length) return false;

    const uint32_t triple = ((uint32_t)a << 18U) |
                            ((uint32_t)b << 12U) |
                            ((uint32_t)c << 6U) |
                            (uint32_t)d;

    if (out >= outputSize) return false;
    output[out++] = (uint8_t)(triple >> 16U);
    if (!padC)
    {
      if (out >= outputSize) return false;
      output[out++] = (uint8_t)(triple >> 8U);
    }
    if (!padD)
    {
      if (out >= outputSize) return false;
      output[out++] = (uint8_t)triple;
    }
  }

  if (outputLength != nullptr) *outputLength = out;
  return true;
}

static bool FLASHMEM identityValidSerial(const char* serial)
{
  if (serial == nullptr) return false;
  for (uint8_t i = 0U; i < 5U; i++)
  {
    if (serial[i] < '0' || serial[i] > '9') return false;
  }
  return serial[5] == '\0';
}

static bool FLASHMEM identityValidFirmwareVersion(const char* version)
{
  if (version == nullptr) return false;
  const size_t length = strlen(version);
  if (length == 0U || length > 31U) return false;
  for (size_t i = 0U; i < length; i++)
  {
    const uint8_t c = (uint8_t)version[i];
    if (c < 0x20U || c == 0x7FU) return false;
  }
  return true;
}

static bool FLASHMEM identityValidFileExtension(const char* name, const char* extension)
{
  if (name == nullptr || extension == nullptr) return false;
  const size_t nameLength = strlen(name);
  const size_t extensionLength = strlen(extension);
  if (nameLength < extensionLength) return false;
  const char* a = name + nameLength - extensionLength;
  for (size_t i = 0U; i < extensionLength; i++)
  {
    char ca = a[i];
    char cb = extension[i];
    if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
    if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
    if (ca != cb) return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
// Kanonischer Byte-Writer fuer die exakt mit KeyTool V0.3.0 abgestimmten
// .tpreq- und .tpcert-Datensaetze.
// -----------------------------------------------------------------------------
struct IdentityCanonicalWriter
{
  uint8_t* data;
  size_t capacity;
  size_t length;
  bool valid;

  IdentityCanonicalWriter(uint8_t* target, size_t targetSize);
  bool append(const void* source, size_t count);
  bool appendU32Le(uint32_t value);
  bool appendI64Le(int64_t value);
  bool writeBytes(const void* source, size_t count);
  bool writeUtf8(const char* text);
};

FLASHMEM IdentityCanonicalWriter::IdentityCanonicalWriter(uint8_t* target,
                                                          size_t targetSize)
  : data(target), capacity(targetSize), length(0U), valid(target != nullptr)
{
}

bool FLASHMEM IdentityCanonicalWriter::append(const void* source, size_t count)
{
  if (!valid || source == nullptr || count > capacity - length)
  {
    valid = false;
    return false;
  }
  memcpy(data + length, source, count);
  length += count;
  return true;
}

bool FLASHMEM IdentityCanonicalWriter::appendU32Le(uint32_t value)
{
  uint8_t bytes[4] =
  {
    (uint8_t)value,
    (uint8_t)(value >> 8U),
    (uint8_t)(value >> 16U),
    (uint8_t)(value >> 24U)
  };
  return append(bytes, sizeof(bytes));
}

bool FLASHMEM IdentityCanonicalWriter::appendI64Le(int64_t value)
{
  uint64_t raw = (uint64_t)value;
  uint8_t bytes[8];
  for (uint8_t i = 0U; i < 8U; i++) bytes[i] = (uint8_t)(raw >> (8U * i));
  return append(bytes, sizeof(bytes));
}

bool FLASHMEM IdentityCanonicalWriter::writeBytes(const void* source, size_t count)
{
  if (count > 0xFFFFFFFFUL) return false;
  return appendU32Le((uint32_t)count) && append(source, count);
}

bool FLASHMEM IdentityCanonicalWriter::writeUtf8(const char* text)
{
  if (text == nullptr) return false;
  return writeBytes(text, strlen(text));
}

static void FLASHMEM identityBuildSpki(const uint8_t publicKey[64], uint8_t spki[91])
{
  memcpy(spki, identitySpkiPrefix, sizeof(identitySpkiPrefix));
  memcpy(spki + sizeof(identitySpkiPrefix), publicKey, 64U);
}

static void FLASHMEM identityCalculateKeyId(const uint8_t spki[91],
                                   uint8_t keyIdBytes[8],
                                   char keyIdText[17])
{
  uint8_t digest[32];
  TpSha256::hash(spki, 91U, digest);
  memcpy(keyIdBytes, digest, 8U);
  identityBytesToHex(keyIdBytes, 8U, keyIdText, 17U, true);
  memset(digest, 0, sizeof(digest));
}

static bool FLASHMEM identityBuildRequestCanonical(const char* serial,
                                          const char* keyId,
                                          const uint8_t spki[91],
                                          const char* requestId,
                                          const char* firmwareVersion,
                                          int64_t createdUtc,
                                          size_t* outputLength)
{
  IdentityCanonicalWriter writer(identityCanonicalBuffer,
                                 sizeof(identityCanonicalBuffer));
  writer.writeUtf8(IDENTITY_REQUEST_FORMAT);
  writer.writeUtf8(IDENTITY_DEVICE_TYPE);
  writer.writeUtf8(serial);
  writer.writeUtf8(keyId);
  writer.writeBytes(spki, 91U);
  writer.writeUtf8(requestId);
  writer.writeUtf8(firmwareVersion);
  writer.appendI64Le(createdUtc);
  if (outputLength != nullptr) *outputLength = writer.length;
  return writer.valid;
}

static bool FLASHMEM identityBuildCertificateCanonical(const IdentityCertificateSlot& certificate,
                                              size_t* outputLength)
{
  char deviceKeyId[17];
  char issuerKeyId[17];
  char certificateSerial[33];
  char requestId[33];
  char requestManifest[65];

  identityBytesToHex(certificate.deviceKeyId, 8U,
                     deviceKeyId, sizeof(deviceKeyId), true);
  identityBytesToHex(certificate.issuerKeyId, 8U,
                     issuerKeyId, sizeof(issuerKeyId), true);
  identityBytesToHex(certificate.certificateSerial, 16U,
                     certificateSerial, sizeof(certificateSerial), true);
  identityBytesToHex(certificate.requestId, 16U,
                     requestId, sizeof(requestId), true);
  identityBytesToHex(certificate.requestManifestSha256, 32U,
                     requestManifest, sizeof(requestManifest), false);

  IdentityCanonicalWriter writer(identityCanonicalBuffer,
                                 sizeof(identityCanonicalBuffer));
  writer.writeUtf8(IDENTITY_CERT_FORMAT);
  writer.writeUtf8(IDENTITY_DEVICE_TYPE);
  writer.writeUtf8(certificate.deviceSerial);
  writer.writeUtf8(deviceKeyId);
  writer.writeBytes(identitySpki, sizeof(identitySpki));
  writer.writeUtf8(issuerKeyId);
  writer.writeUtf8(certificateSerial);
  writer.appendI64Le(certificate.issuedUtc);
  writer.writeUtf8(requestId);
  writer.writeUtf8(requestManifest);
  writer.writeUtf8(certificate.requestFirmwareVersion);
  if (outputLength != nullptr) *outputLength = writer.length;
  return writer.valid;
}

// -----------------------------------------------------------------------------
// Zufallsquelle und EEPROM-A/B-Slots
// -----------------------------------------------------------------------------
static int FLASHMEM identityMicroEccRng(uint8_t* destination, unsigned size)
{
  if (destination == nullptr) return 0;
  while (size > 0U)
  {
    const uint32_t randomValue = Entropy.random();
    for (uint8_t i = 0U; i < 4U && size > 0U; i++, size--)
    {
      *destination++ = (uint8_t)(randomValue >> (8U * i));
    }
  }
  return 1;
}

static void FLASHMEM identityEntropyBegin()
{
  if (identityEntropyInitialized) return;
  Entropy.Initialize();
  uECC_set_rng(&identityMicroEccRng);
  identityEntropyInitialized = true;
}

static bool FLASHMEM identityKeySlotValid(const IdentityKeySlot& slot)
{
  if (slot.magic != IDENTITY_KEY_MAGIC ||
      slot.version != IDENTITY_KEY_VERSION ||
      slot.size != sizeof(IdentityKeySlot)) return false;

  const uint32_t expected = identityCrc32(&slot, offsetof(IdentityKeySlot, crc32));
  if (expected != slot.crc32 || identityAllZero(slot.privateKey, sizeof(slot.privateKey))) return false;

  uint8_t publicKey[64];
  const bool valid = uECC_compute_public_key(slot.privateKey,
                                             publicKey,
                                             uECC_secp256r1()) == 1;
  memset(publicKey, 0, sizeof(publicKey));
  return valid;
}

static bool FLASHMEM identityCertificateSlotStorageValid(const IdentityCertificateSlot& slot)
{
  if (slot.magic != IDENTITY_CERT_MAGIC ||
      slot.version != IDENTITY_CERT_VERSION ||
      slot.size != sizeof(IdentityCertificateSlot)) return false;
  const uint32_t expected = identityCrc32(&slot, offsetof(IdentityCertificateSlot, crc32));
  return expected == slot.crc32;
}

static bool FLASHMEM identityLoadKeySlots()
{
  IdentityKeySlot a;
  IdentityKeySlot b;
  EEPROM.get(IDENTITY_KEY_SLOT_A_ADDR, a);
  EEPROM.get(IDENTITY_KEY_SLOT_B_ADDR, b);

  const bool validA = identityKeySlotValid(a);
  const bool validB = identityKeySlotValid(b);
  if (!validA && !validB) return false;

  identityKeySlot = (!validB || (validA && identitySequenceNewer(a.sequence, b.sequence))) ? a : b;
  return true;
}

static bool FLASHMEM identityLoadCertificateSlots()
{
  IdentityCertificateSlot a;
  IdentityCertificateSlot b;
  EEPROM.get(IDENTITY_CERT_SLOT_A_ADDR, a);
  EEPROM.get(IDENTITY_CERT_SLOT_B_ADDR, b);

  const bool validA = identityCertificateSlotStorageValid(a);
  const bool validB = identityCertificateSlotStorageValid(b);
  if (!validA && !validB) return false;

  identityCertificateSlot = (!validB || (validA && identitySequenceNewer(a.sequence, b.sequence))) ? a : b;
  return true;
}

static bool FLASHMEM identityWriteKeySlot(int address,
                                 const IdentityKeySlot& slot,
                                 IdentityKeySlot* verified)
{
  EEPROM.put(address, slot);
  IdentityKeySlot readBack;
  EEPROM.get(address, readBack);
  if (!identityKeySlotValid(readBack) || readBack.sequence != slot.sequence) return false;
  if (verified != nullptr) *verified = readBack;
  return true;
}

static bool FLASHMEM identitySaveKeySlot(const uint8_t privateKey[32])
{
  const bool hadValidKey = identityKeyValid;
  IdentityKeySlot slot;
  memset(&slot, 0, sizeof(slot));
  slot.magic = IDENTITY_KEY_MAGIC;
  slot.version = IDENTITY_KEY_VERSION;
  slot.size = sizeof(slot);
  slot.sequence = hadValidKey ? identityKeySlot.sequence + 1UL : 1UL;
  memcpy(slot.privateKey, privateKey, sizeof(slot.privateKey));
  slot.crc32 = identityCrc32(&slot, offsetof(IdentityKeySlot, crc32));

  const int firstAddress = (!hadValidKey ||
                            identityKeySlot.sequence % 2UL == 0UL)
                         ? IDENTITY_KEY_SLOT_A_ADDR
                         : IDENTITY_KEY_SLOT_B_ADDR;
  IdentityKeySlot verified;
  if (!identityWriteKeySlot(firstAddress, slot, &verified)) return false;

  // Beim allerersten Speichern beide A/B-Slots anlegen. Fällt die Versorgung
  // zwischen den beiden Writes aus, bleibt bereits der erste Slot gültig.
  if (!hadValidKey)
  {
    IdentityKeySlot mirror = slot;
    mirror.sequence = slot.sequence + 1UL;
    mirror.crc32 = identityCrc32(&mirror, offsetof(IdentityKeySlot, crc32));
    const int mirrorAddress = firstAddress == IDENTITY_KEY_SLOT_A_ADDR
                            ? IDENTITY_KEY_SLOT_B_ADDR
                            : IDENTITY_KEY_SLOT_A_ADDR;
    if (!identityWriteKeySlot(mirrorAddress, mirror, &verified)) return false;
  }

  identityKeySlot = verified;
  identityKeyValid = true;
  return true;
}

static bool FLASHMEM identityWriteCertificateSlot(int address,
                                         const IdentityCertificateSlot& slot,
                                         IdentityCertificateSlot* verified)
{
  EEPROM.put(address, slot);
  IdentityCertificateSlot readBack;
  EEPROM.get(address, readBack);
  if (!identityCertificateSlotStorageValid(readBack) ||
      readBack.sequence != slot.sequence) return false;
  if (verified != nullptr) *verified = readBack;
  return true;
}

static bool FLASHMEM identitySaveCertificateSlot(const IdentityCertificateSlot& source)
{
  const bool hadValidCertificate = identityCertificateValidFlag;
  IdentityCertificateSlot slot = source;
  slot.magic = IDENTITY_CERT_MAGIC;
  slot.version = IDENTITY_CERT_VERSION;
  slot.size = sizeof(slot);
  slot.sequence = hadValidCertificate
                ? identityCertificateSlot.sequence + 1UL
                : 1UL;
  slot.crc32 = identityCrc32(&slot, offsetof(IdentityCertificateSlot, crc32));

  const int firstAddress = (!hadValidCertificate ||
                            identityCertificateSlot.sequence % 2UL == 0UL)
                         ? IDENTITY_CERT_SLOT_A_ADDR
                         : IDENTITY_CERT_SLOT_B_ADDR;
  IdentityCertificateSlot verified;
  if (!identityWriteCertificateSlot(firstAddress, slot, &verified)) return false;

  if (!hadValidCertificate)
  {
    IdentityCertificateSlot mirror = slot;
    mirror.sequence = slot.sequence + 1UL;
    mirror.crc32 = identityCrc32(&mirror,
                                offsetof(IdentityCertificateSlot, crc32));
    const int mirrorAddress = firstAddress == IDENTITY_CERT_SLOT_A_ADDR
                            ? IDENTITY_CERT_SLOT_B_ADDR
                            : IDENTITY_CERT_SLOT_A_ADDR;
    if (!identityWriteCertificateSlot(mirrorAddress, mirror, &verified)) return false;
  }

  identityCertificateSlot = verified;
  return true;
}

// -----------------------------------------------------------------------------
// Zertifikatspruefung aus kompaktem EEPROM-Slot
// -----------------------------------------------------------------------------
static bool FLASHMEM identityVerifyStoredCertificate(IdentityCertificateSlot& certificate)
{
  if (!identityKeyValid || !identityValidSerial(certificate.deviceSerial)) return false;
  if (!identityValidFirmwareVersion(certificate.requestFirmwareVersion)) return false;

  uint8_t expectedKeyId[8];
  char expectedKeyIdText[17];
  identityCalculateKeyId(identitySpki, expectedKeyId, expectedKeyIdText);
  if (!identityBytesEqual(expectedKeyId, certificate.deviceKeyId, sizeof(expectedKeyId))) return false;

  uint8_t expectedIssuer[8];
  if (!identityHexToBytes(IDENTITY_TEST_ROOT_KEY_ID, expectedIssuer, sizeof(expectedIssuer)) ||
      !identityBytesEqual(expectedIssuer, certificate.issuerKeyId, sizeof(expectedIssuer))) return false;

  size_t canonicalLength = 0U;
  if (!identityBuildCertificateCanonical(certificate, &canonicalLength)) return false;

  uint8_t calculatedManifest[32];
  TpSha256::hash(identityCanonicalBuffer, canonicalLength, calculatedManifest);
  if (!identityBytesEqual(calculatedManifest,
                          certificate.manifestSha256,
                          sizeof(calculatedManifest))) return false;

  const bool signatureValid = uECC_verify(identityTestRootPublicKey,
                                           calculatedManifest,
                                           sizeof(calculatedManifest),
                                           certificate.rootSignature,
                                           uECC_secp256r1()) == 1;
  memset(calculatedManifest, 0, sizeof(calculatedManifest));
  return signatureValid;
}

// -----------------------------------------------------------------------------
// Oeffentliche Identity-Schnittstelle
// -----------------------------------------------------------------------------
void FLASHMEM deviceIdentityBegin(void)
{
  identityEntropyBegin();
  identityKeyValid = identityLoadKeySlots();
  identityCertificateValidFlag = false;
  identityCertificateStoredButInvalid = false;
  identityDeviceKeyIdText[0] = '\0';
  identityCertifiedSerialText[0] = '\0';

  if (identityKeyValid)
  {
    if (uECC_compute_public_key(identityKeySlot.privateKey,
                                identityPublicKey,
                                uECC_secp256r1()) != 1)
    {
      identityKeyValid = false;
      memset(identityPublicKey, 0, sizeof(identityPublicKey));
    }
    else
    {
      identityBuildSpki(identityPublicKey, identitySpki);
      uint8_t keyId[8];
      identityCalculateKeyId(identitySpki, keyId, identityDeviceKeyIdText);
      memset(keyId, 0, sizeof(keyId));
    }
  }

  if (identityKeyValid && identityLoadCertificateSlots())
  {
    identityCertificateStoredButInvalid = true;
    if (identityVerifyStoredCertificate(identityCertificateSlot))
    {
      identityCertificateValidFlag = true;
      identityCertificateStoredButInvalid = false;
      strncpy(identityCertifiedSerialText,
              identityCertificateSlot.deviceSerial,
              sizeof(identityCertifiedSerialText) - 1U);
      identityCertifiedSerialText[sizeof(identityCertifiedSerialText) - 1U] = '\0';

      // Die zertifizierte SN auch in den normalen Einstellungsblock spiegeln,
      // damit Backups und nicht kryptografische Altpfade denselben Wert sehen.
      if (strncmp(R.geraete_name, identityCertifiedSerialText, 6U) != 0)
      {
        strncpy(R.geraete_name, identityCertifiedSerialText,
                sizeof(R.geraete_name) - 1U);
        R.geraete_name[sizeof(R.geraete_name) - 1U] = '\0';
        tpMainConfigSave();
      }
    }
  }
}

bool FLASHMEM deviceIdentityHasKey(void)
{
  return identityKeyValid;
}

bool FLASHMEM deviceIdentityCertificateValid(void)
{
  return identityCertificateValidFlag;
}

bool FLASHMEM deviceIdentityCertificateStoredInvalid(void)
{
  return identityCertificateStoredButInvalid;
}

const char* FLASHMEM deviceIdentityCertifiedSerial(void)
{
  return identityCertificateValidFlag ? identityCertifiedSerialText : identityFlashText.t004;
}

const char* FLASHMEM deviceIdentityDeviceKeyId(void)
{
  return identityKeyValid ? identityDeviceKeyIdText : identityFlashText.t004;
}

const char* FLASHMEM deviceIdentityRootKeyId(void)
{
  return IDENTITY_TEST_ROOT_KEY_ID;
}

const char* FLASHMEM deviceIdentityLastCertificateFile(void)
{
  return identityLastCertificateFile;
}

const char* FLASHMEM deviceIdentityStatusText(void)
{
  if (identityCertificateValidFlag) return identityFlashText.t005;
  if (identityCertificateStoredButInvalid) return identityFlashText.t006;
  if (identityKeyValid) return identityFlashText.t007;
  return identityFlashText.t008;
}

bool FLASHMEM deviceIdentityGenerateKey(const char* confirmedSerial,
                                        char* errorText,
                                        size_t errorTextSize)
{
  if (identityCertificateValidFlag)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t009);
    return false;
  }
  if (identityKeyValid)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t010);
    return false;
  }

  const char* configuredSerial = deviceSerialGet();
  if (!identityValidSerial(configuredSerial) || strcmp(configuredSerial, identityFlashText.t011) == 0)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t012);
    return false;
  }
  if (confirmedSerial == nullptr || strcmp(configuredSerial, confirmedSerial) != 0)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t013);
    return false;
  }

  identityEntropyBegin();
  uint8_t newPublicKey[64];
  uint8_t newPrivateKey[32];
  memset(newPublicKey, 0, sizeof(newPublicKey));
  memset(newPrivateKey, 0, sizeof(newPrivateKey));

  if (uECC_make_key(newPublicKey,
                    newPrivateKey,
                    uECC_secp256r1()) != 1)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t014);
    memset(newPrivateKey, 0, sizeof(newPrivateKey));
    return false;
  }

  if (!identitySaveKeySlot(newPrivateKey))
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t015);
    memset(newPrivateKey, 0, sizeof(newPrivateKey));
    return false;
  }

  memcpy(identityPublicKey, newPublicKey, sizeof(identityPublicKey));
  identityBuildSpki(identityPublicKey, identitySpki);
  uint8_t keyId[8];
  identityCalculateKeyId(identitySpki, keyId, identityDeviceKeyIdText);
  memset(keyId, 0, sizeof(keyId));
  memset(newPrivateKey, 0, sizeof(newPrivateKey));
  memset(newPublicKey, 0, sizeof(newPublicKey));
  identitySetError(errorText, errorTextSize, identityFlashText.t016);
  return true;
}

static int64_t FLASHMEM identityCurrentUnixTime()
{
  const time_t value = now();
  if (value < (time_t)1577836800) return 0; // vor 2020 gilt als nicht gestellt
  return (int64_t)value;
}

static void FLASHMEM identityFormatIsoUtc(int64_t unixTime, char* output, size_t outputSize)
{
  if (output == nullptr || outputSize == 0U) return;
  const time_t t = unixTime > 0 ? (time_t)unixTime : (time_t)0;
  snprintf(output, outputSize,
           identityFlashText.t017,
           year(t), month(t), day(t), hour(t), minute(t), second(t));
}

bool FLASHMEM deviceIdentityBuildRequestJson(char* output,
                                             size_t outputSize,
                                             size_t* outputLength,
                                             char* downloadName,
                                             size_t downloadNameSize,
                                             char* errorText,
                                             size_t errorTextSize)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (!identityKeyValid)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t018);
    return false;
  }
  if (identityCertificateValidFlag)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t019);
    return false;
  }
  if (output == nullptr || outputSize < 1024U)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t020);
    return false;
  }

  const char* serial = deviceSerialGet();
  if (!identityValidSerial(serial) || strcmp(serial, identityFlashText.t011) == 0)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t021);
    return false;
  }

  uint8_t requestIdBytes[16];
  if (identityMicroEccRng(requestIdBytes, sizeof(requestIdBytes)) != 1)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t022);
    return false;
  }

  char requestId[33];
  identityBytesToHex(requestIdBytes, sizeof(requestIdBytes),
                     requestId, sizeof(requestId), true);
  const int64_t createdUtc = identityCurrentUnixTime();

  size_t canonicalLength = 0U;
  if (!identityBuildRequestCanonical(serial,
                                     identityDeviceKeyIdText,
                                     identitySpki,
                                     requestId,
                                     VERSION,
                                     createdUtc,
                                     &canonicalLength))
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t023);
    return false;
  }

  uint8_t manifest[32];
  uint8_t signature[64];
  TpSha256::hash(identityCanonicalBuffer, canonicalLength, manifest);
  if (uECC_sign(identityKeySlot.privateKey,
                manifest,
                sizeof(manifest),
                signature,
                uECC_secp256r1()) != 1)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t024);
    memset(signature, 0, sizeof(signature));
    return false;
  }

  char spkiBase64[125];
  char signatureBase64[89];
  char manifestHex[65];
  char createdText[40];
  identityBase64Encode(identitySpki, sizeof(identitySpki),
                       spkiBase64, sizeof(spkiBase64));
  identityBase64Encode(signature, sizeof(signature),
                       signatureBase64, sizeof(signatureBase64));
  identityBytesToHex(manifest, sizeof(manifest),
                     manifestHex, sizeof(manifestHex), false);
  identityFormatIsoUtc(createdUtc, createdText, sizeof(createdText));

  const int length = snprintf(output, outputSize,
    identityFlashText.t001,
    IDENTITY_REQUEST_FORMAT,
    IDENTITY_DEVICE_TYPE,
    serial,
    identityDeviceKeyIdText,
    spkiBase64,
    requestId,
    VERSION,
    createdText,
    manifestHex,
    IDENTITY_SIGNATURE_ALGORITHM,
    IDENTITY_SIGNATURE_ENCODING,
    signatureBase64);

  memset(signature, 0, sizeof(signature));
  memset(manifest, 0, sizeof(manifest));
  if (length <= 0 || (size_t)length >= outputSize)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t025);
    return false;
  }

  if (downloadName != nullptr && downloadNameSize > 0U)
  {
    snprintf(downloadName, downloadNameSize,
             identityFlashText.t026, serial);
  }
  if (outputLength != nullptr) *outputLength = (size_t)length;
  identitySetError(errorText, errorTextSize, identityFlashText.t016);
  return true;
}

bool FLASHMEM deviceIdentitySaveRequestToSd(const char* json,
                                            size_t jsonLength,
                                            const char* downloadName,
                                            char* storedPath,
                                            size_t storedPathSize,
                                            char* errorText,
                                            size_t errorTextSize)
{
  if (json == nullptr || jsonLength == 0U || downloadName == nullptr)
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t027);
    return false;
  }

  if (!SD.exists(IDENTITY_SD_DIRECTORY) && !SD.mkdir(IDENTITY_SD_DIRECTORY))
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t028);
    return false;
  }

  char path[96];
  snprintf(path, sizeof(path), identityFlashText.t029, IDENTITY_SD_DIRECTORY, downloadName);
  if (SD.exists(path)) SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  if (!file)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t030);
    return false;
  }

  const size_t written = file.write((const uint8_t*)json, jsonLength);
  file.flush();
  file.close();
  if (written != jsonLength)
  {
    SD.remove(path);
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t031);
    return false;
  }

  if (storedPath != nullptr && storedPathSize > 0U)
  {
    strncpy(storedPath, path, storedPathSize - 1U);
    storedPath[storedPathSize - 1U] = '\0';
  }
  identitySetError(errorText, errorTextSize, identityFlashText.t016);
  return true;
}

// -----------------------------------------------------------------------------
// Minimaler JSON- und ISO-Parser fuer die vom KeyTool erzeugte .tpcert-Datei
// -----------------------------------------------------------------------------
static int FLASHMEM identityJsonHexNibble(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static bool FLASHMEM identityJsonGetString(const char* json,
                                  const char* field,
                                  char* output,
                                  size_t outputSize)
{
  if (json == nullptr || field == nullptr || output == nullptr || outputSize == 0U) return false;

  char pattern[80];
  const int patternLength = snprintf(pattern, sizeof(pattern), identityFlashText.t032, field);
  if (patternLength <= 0 || (size_t)patternLength >= sizeof(pattern)) return false;

  const char* position = strstr(json, pattern);
  if (position == nullptr) return false;
  position += patternLength;
  while (*position == ' ' || *position == '\t' || *position == '\r' || *position == '\n') position++;
  if (*position++ != ':') return false;
  while (*position == ' ' || *position == '\t' || *position == '\r' || *position == '\n') position++;
  if (*position++ != '"') return false;

  size_t out = 0U;
  while (*position != '\0' && *position != '"')
  {
    char c = *position++;
    if (c == '\\')
    {
      const char escape = *position++;
      switch (escape)
      {
        case '"': c = '"'; break;
        case '\\': c = '\\'; break;
        case '/': c = '/'; break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'u':
        {
          int value = 0;
          for (uint8_t i = 0U; i < 4U; i++)
          {
            const int nibble = identityJsonHexNibble(*position++);
            if (nibble < 0) return false;
            value = (value << 4) | nibble;
          }
          // Die Zertifikatsfelder sind ASCII. System.Text.Json kann sichere
          // ASCII-Zeichen wie '+' als \u002B schreiben; diese Darstellung
          // wird hier in das identische Einzelbyte zurueckgewandelt.
          if (value > 0x7F) return false;
          c = (char)value;
          break;
        }
        default: return false;
      }
    }
    if (out + 1U >= outputSize) return false;
    output[out++] = c;
  }
  if (*position != '"') return false;
  output[out] = '\0';
  return true;
}

static bool FLASHMEM identityParseIsoUtc(const char* text, int64_t* unixTime)
{
  if (text == nullptr || unixTime == nullptr || strlen(text) < 19U) return false;

  // Festes Zertifikatsformat: YYYY-MM-DDTHH:MM:SS.
  // Bewusst ohne sscanf(): Der universelle newlib-Scanner zieht auf dem
  // Teensy grosse Locale-/Parser-Tabellen in RAM1, obwohl hier nur feste
  // Dezimalstellen benoetigt werden.
  if (text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
      text[13] != ':' || text[16] != ':') return false;

  const uint8_t digitPositions[] =
  {
    0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U,
    11U, 12U, 14U, 15U, 17U, 18U
  };
  for (uint8_t i = 0U; i < sizeof(digitPositions); i++)
  {
    const char c = text[digitPositions[i]];
    if (c < '0' || c > '9') return false;
  }

  const int y = (text[0] - '0') * 1000 +
                (text[1] - '0') * 100 +
                (text[2] - '0') * 10 +
                (text[3] - '0');
  const int mo = (text[5] - '0') * 10 + (text[6] - '0');
  const int d = (text[8] - '0') * 10 + (text[9] - '0');
  const int h = (text[11] - '0') * 10 + (text[12] - '0');
  const int mi = (text[14] - '0') * 10 + (text[15] - '0');
  const int s = (text[17] - '0') * 10 + (text[18] - '0');

  if (y < 1970 || y > 2105 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
      h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 60) return false;

  tmElements_t tm;
  tm.Year = CalendarYrToTm(y);
  tm.Month = (uint8_t)mo;
  tm.Day = (uint8_t)d;
  tm.Hour = (uint8_t)h;
  tm.Minute = (uint8_t)mi;
  tm.Second = (uint8_t)(s > 59 ? 59 : s);
  *unixTime = (int64_t)makeTime(tm);
  return true;
}

static bool FLASHMEM identityParseAndVerifyCertificate(const char* json,
                                              IdentityCertificateSlot* result,
                                              char* errorText,
                                              size_t errorTextSize)
{
  if (json == nullptr || result == nullptr || !identityKeyValid)
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t033);
    return false;
  }

  char format[48];
  char deviceType[16];
  char serial[8];
  char deviceKeyId[20];
  char spkiBase64[160];
  char issuerKeyId[20];
  char certificateSerial[40];
  char issuedUtc[48];
  char requestId[40];
  char requestManifest[72];
  char requestFirmwareVersion[64];
  char manifestHex[72];
  char signatureAlgorithm[32];
  char signatureEncoding[24];
  char signatureBase64[120];

#define ID_GET(field, target) \
  do { if (!identityJsonGetString(json, field, target, sizeof(target))) \
       { snprintf(errorText, errorTextSize, identityFlashText.t002, field); return false; } } while (0)

  ID_GET(identityFlashText.t034, format);
  ID_GET(identityFlashText.t035, deviceType);
  ID_GET(identityFlashText.t036, serial);
  ID_GET(identityFlashText.t037, deviceKeyId);
  ID_GET(identityFlashText.t038, spkiBase64);
  ID_GET(identityFlashText.t039, issuerKeyId);
  ID_GET(identityFlashText.t040, certificateSerial);
  ID_GET(identityFlashText.t041, issuedUtc);
  ID_GET(identityFlashText.t042, requestId);
  ID_GET(identityFlashText.t043, requestManifest);
  ID_GET(identityFlashText.t044, requestFirmwareVersion);
  ID_GET(identityFlashText.t045, manifestHex);
  ID_GET(identityFlashText.t046, signatureAlgorithm);
  ID_GET(identityFlashText.t047, signatureEncoding);
  ID_GET(identityFlashText.t048, signatureBase64);
#undef ID_GET

  if (strcmp(format, IDENTITY_CERT_FORMAT) != 0 ||
      strcmp(deviceType, IDENTITY_DEVICE_TYPE) != 0 ||
      strcmp(signatureAlgorithm, IDENTITY_SIGNATURE_ALGORITHM) != 0 ||
      strcmp(signatureEncoding, IDENTITY_SIGNATURE_ENCODING) != 0)
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t049);
    return false;
  }
  if (!identityValidSerial(serial))
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t050);
    return false;
  }
  if (!identityValidFirmwareVersion(requestFirmwareVersion))
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t051);
    return false;
  }
  if (strcmp(deviceKeyId, identityDeviceKeyIdText) != 0)
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t052);
    return false;
  }
  if (strcmp(issuerKeyId, IDENTITY_TEST_ROOT_KEY_ID) != 0)
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t053);
    return false;
  }

  uint8_t decodedSpki[96];
  size_t decodedSpkiLength = 0U;
  if (!identityBase64Decode(spkiBase64, decodedSpki, sizeof(decodedSpki), &decodedSpkiLength) ||
      decodedSpkiLength != sizeof(identitySpki) ||
      !identityBytesEqual(decodedSpki, identitySpki, sizeof(identitySpki)))
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t054);
    return false;
  }

  IdentityCertificateSlot certificate;
  memset(&certificate, 0, sizeof(certificate));
  memcpy(certificate.deviceSerial, serial, 5U);
  certificate.deviceSerial[5] = '\0';
  strncpy(certificate.requestFirmwareVersion,
          requestFirmwareVersion,
          sizeof(certificate.requestFirmwareVersion) - 1U);

  if (!identityHexToBytes(deviceKeyId, certificate.deviceKeyId, sizeof(certificate.deviceKeyId)) ||
      !identityHexToBytes(issuerKeyId, certificate.issuerKeyId, sizeof(certificate.issuerKeyId)) ||
      !identityHexToBytes(certificateSerial, certificate.certificateSerial, sizeof(certificate.certificateSerial)) ||
      !identityHexToBytes(requestId, certificate.requestId, sizeof(certificate.requestId)) ||
      !identityHexToBytes(requestManifest, certificate.requestManifestSha256, sizeof(certificate.requestManifestSha256)) ||
      !identityHexToBytes(manifestHex, certificate.manifestSha256, sizeof(certificate.manifestSha256)))
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t055);
    return false;
  }
  if (!identityParseIsoUtc(issuedUtc, &certificate.issuedUtc))
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t056);
    return false;
  }

  size_t signatureLength = 0U;
  if (!identityBase64Decode(signatureBase64,
                            certificate.rootSignature,
                            sizeof(certificate.rootSignature),
                            &signatureLength) ||
      signatureLength != sizeof(certificate.rootSignature))
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t057);
    return false;
  }

  // Magic/Version sind fuer den kanonischen Zertifikatsdatensatz irrelevant.
  // Sie werden erst beim Speichern gesetzt.
  size_t canonicalLength = 0U;
  if (!identityBuildCertificateCanonical(certificate, &canonicalLength))
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t058);
    return false;
  }

  uint8_t calculatedManifest[32];
  TpSha256::hash(identityCanonicalBuffer, canonicalLength, calculatedManifest);
  if (!identityBytesEqual(calculatedManifest,
                          certificate.manifestSha256,
                          sizeof(calculatedManifest)))
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t059);
    return false;
  }

  if (uECC_verify(identityTestRootPublicKey,
                  calculatedManifest,
                  sizeof(calculatedManifest),
                  certificate.rootSignature,
                  uECC_secp256r1()) != 1)
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t060);
    return false;
  }

  *result = certificate;
  identitySetError(errorText, errorTextSize, identityFlashText.t016);
  return true;
}

static bool FLASHMEM identityReadFile(File& file, char* buffer, size_t bufferSize, size_t* length)
{
  if (length != nullptr) *length = 0U;
  if (!file || buffer == nullptr || bufferSize < 2U) return false;
  const uint32_t fileSize = file.size();
  if (fileSize == 0U || fileSize >= bufferSize) return false;

  size_t readLength = 0U;
  while (file.available() && readLength < bufferSize - 1U)
  {
    const int value = file.read();
    if (value < 0) break;
    buffer[readLength++] = (char)value;
  }
  buffer[readLength] = '\0';
  if (length != nullptr) *length = readLength;
  return readLength == fileSize;
}

static bool FLASHMEM identityTryCertificateFile(const char* path,
                                       IdentityCertificateSlot* certificate,
                                       char* errorText,
                                       size_t errorTextSize)
{
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  size_t length = 0U;
  const bool readOk = identityReadFile(file,
                                       identityCertificateJson,
                                       sizeof(identityCertificateJson),
                                       &length);
  file.close();
  if (!readOk)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t061);
    return false;
  }
  return identityParseAndVerifyCertificate(identityCertificateJson,
                                           certificate,
                                           errorText,
                                           errorTextSize);
}


bool FLASHMEM deviceIdentitySignHash(const uint8_t hash[32], uint8_t signature[64])
{
  if (!identityKeyValid || hash == nullptr || signature == nullptr) return false;
  return uECC_sign(identityKeySlot.privateKey,
                   hash,
                   32U,
                   signature,
                   uECC_secp256r1()) == 1;
}

bool FLASHMEM deviceIdentityRandomBytes(uint8_t* output, size_t outputLength)
{
  if (output == nullptr || outputLength == 0U) return false;
  identityEntropyBegin();
  return identityMicroEccRng(output, (unsigned)outputLength) == 1;
}

bool FLASHMEM deviceIdentityVerifyRootSignedHash(const uint8_t hash[32],
                                                  const uint8_t signature[64])
{
  if (hash == nullptr || signature == nullptr) return false;
  return uECC_verify(identityTestRootPublicKey,
                     hash,
                     32U,
                     signature,
                     uECC_secp256r1()) == 1;
}

bool FLASHMEM deviceIdentityVerifyDeviceSignedHash(const uint8_t hash[32],
                                                    const uint8_t signature[64])
{
  if (!identityKeyValid || !identityCertificateValidFlag ||
      hash == nullptr || signature == nullptr) return false;
  return uECC_verify(identityPublicKey,
                     hash,
                     32U,
                     signature,
                     uECC_secp256r1()) == 1;
}

const char* FLASHMEM deviceIdentityCertificateSerialText(void)
{
  if (!identityCertificateValidFlag)
  {
    identityCertificateSerialTextBuffer[0] = '\0';
    return identityCertificateSerialTextBuffer;
  }
  identityBytesToHex(identityCertificateSlot.certificateSerial,
                     sizeof(identityCertificateSlot.certificateSerial),
                     identityCertificateSerialTextBuffer,
                     sizeof(identityCertificateSerialTextBuffer),
                     true);
  return identityCertificateSerialTextBuffer;
}

int64_t FLASHMEM deviceIdentityCertificateIssuedUtc(void)
{
  return identityCertificateValidFlag ? identityCertificateSlot.issuedUtc : 0;
}

bool FLASHMEM deviceIdentityCertificateManifestHash(uint8_t output[32])
{
  if (!identityCertificateValidFlag || output == nullptr) return false;
  memcpy(output, identityCertificateSlot.manifestSha256, 32U);
  return true;
}

bool FLASHMEM deviceIdentityBuildCertificateJson(char* output,
                                                  size_t outputSize,
                                                  size_t* outputLength)
{
  if (outputLength != nullptr) *outputLength = 0U;
  if (!identityCertificateValidFlag || output == nullptr || outputSize < 768U) return false;

  char spkiBase64[125];
  char signatureBase64[89];
  char deviceKeyId[17];
  char issuerKeyId[17];
  char certificateSerial[33];
  char requestId[33];
  char requestManifest[65];
  char manifest[65];
  char issuedText[40];

  if (!identityBase64Encode(identitySpki, sizeof(identitySpki), spkiBase64, sizeof(spkiBase64)) ||
      !identityBase64Encode(identityCertificateSlot.rootSignature,
                            sizeof(identityCertificateSlot.rootSignature),
                            signatureBase64,
                            sizeof(signatureBase64)))
  {
    return false;
  }

  identityBytesToHex(identityCertificateSlot.deviceKeyId, sizeof(identityCertificateSlot.deviceKeyId),
                     deviceKeyId, sizeof(deviceKeyId), true);
  identityBytesToHex(identityCertificateSlot.issuerKeyId, sizeof(identityCertificateSlot.issuerKeyId),
                     issuerKeyId, sizeof(issuerKeyId), true);
  identityBytesToHex(identityCertificateSlot.certificateSerial, sizeof(identityCertificateSlot.certificateSerial),
                     certificateSerial, sizeof(certificateSerial), true);
  identityBytesToHex(identityCertificateSlot.requestId, sizeof(identityCertificateSlot.requestId),
                     requestId, sizeof(requestId), true);
  identityBytesToHex(identityCertificateSlot.requestManifestSha256,
                     sizeof(identityCertificateSlot.requestManifestSha256),
                     requestManifest, sizeof(requestManifest), false);
  identityBytesToHex(identityCertificateSlot.manifestSha256,
                     sizeof(identityCertificateSlot.manifestSha256),
                     manifest, sizeof(manifest), false);
  identityFormatIsoUtc(identityCertificateSlot.issuedUtc, issuedText, sizeof(issuedText));

  static const char certificateJsonFormat[] TP_IDENTITY_RODATA =
    "{\n"
    "    \"Format\": \"TP3000-DEVICE-CERTIFICATE-2\",\n"
    "    \"DeviceType\": \"TP-3000\",\n"
    "    \"DeviceSerial\": \"%s\",\n"
    "    \"DeviceKeyId\": \"%s\",\n"
    "    \"DevicePublicKeySpkiBase64\": \"%s\",\n"
    "    \"IssuerKeyId\": \"%s\",\n"
    "    \"CertificateSerial\": \"%s\",\n"
    "    \"IssuedUtc\": \"%s\",\n"
    "    \"RequestId\": \"%s\",\n"
    "    \"RequestManifestSha256\": \"%s\",\n"
    "    \"RequestFirmwareVersion\": \"%s\",\n"
    "    \"ManifestSha256\": \"%s\",\n"
    "    \"SignatureAlgorithm\": \"ECDSA-P256-SHA256\",\n"
    "    \"SignatureEncoding\": \"IEEE-P1363\",\n"
    "    \"RootSignatureBase64\": \"%s\"\n"
    "  }";

  const int length = snprintf(output,
                              outputSize,
                              certificateJsonFormat,
                              identityCertificateSlot.deviceSerial,
                              deviceKeyId,
                              spkiBase64,
                              issuerKeyId,
                              certificateSerial,
                              issuedText,
                              requestId,
                              requestManifest,
                              identityCertificateSlot.requestFirmwareVersion,
                              manifest,
                              signatureBase64);
  if (length <= 0 || (size_t)length >= outputSize) return false;
  if (outputLength != nullptr) *outputLength = (size_t)length;
  return true;
}

bool FLASHMEM deviceIdentityImportCertificateFromSd(char* importedPath,
                                                     size_t importedPathSize,
                                                     char* errorText,
                                                     size_t errorTextSize)
{
  if (!identityKeyValid)
  {
    identitySetError(errorText, errorTextSize, identityFlashText.t062);
    return false;
  }

  File directory = SD.open(IDENTITY_SD_DIRECTORY);
  if (!directory || !directory.isDirectory())
  {
    if (directory) directory.close();
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t063);
    return false;
  }

  bool foundCertificateFile = false;
  char lastError[160];
  strncpy(lastError, identityFlashText.t064, sizeof(lastError) - 1U);
  lastError[sizeof(lastError) - 1U] = '\0';
  IdentityCertificateSlot certificate;
  char matchedPath[96] = {0};

  while (true)
  {
    File entry = directory.openNextFile();
    if (!entry) break;
    const bool regularFile = !entry.isDirectory();
    const char* entryName = entry.name();
    char nameCopy[64];
    strncpy(nameCopy, entryName != nullptr ? entryName : identityFlashText.t004, sizeof(nameCopy) - 1U);
    nameCopy[sizeof(nameCopy) - 1U] = '\0';
    entry.close();

    if (!regularFile || !identityValidFileExtension(nameCopy, identityFlashText.t065)) continue;
    foundCertificateFile = true;
    snprintf(matchedPath, sizeof(matchedPath), identityFlashText.t029, IDENTITY_SD_DIRECTORY, nameCopy);

    if (identityTryCertificateFile(matchedPath,
                                   &certificate,
                                   lastError,
                                   sizeof(lastError)))
    {
      if (!identitySaveCertificateSlot(certificate))
      {
        directory.close();
        identitySetError(errorText, errorTextSize,
                         identityFlashText.t066);
        return false;
      }

      identityCertificateSlot = certificate;
      // Nach dem Speichern erneut aus dem A/B-Slot laden, damit Sequenz/CRC
      // exakt dem persistenten Datensatz entsprechen.
      if (!identityLoadCertificateSlots() ||
          !identityVerifyStoredCertificate(identityCertificateSlot))
      {
        directory.close();
        identitySetError(errorText, errorTextSize,
                         identityFlashText.t067);
        return false;
      }

      identityCertificateValidFlag = true;
      identityCertificateStoredButInvalid = false;
      strncpy(identityCertifiedSerialText,
              identityCertificateSlot.deviceSerial,
              sizeof(identityCertifiedSerialText) - 1U);
      identityCertifiedSerialText[sizeof(identityCertifiedSerialText) - 1U] = '\0';
      strncpy(identityLastCertificateFile,
              matchedPath,
              sizeof(identityLastCertificateFile) - 1U);
      identityLastCertificateFile[sizeof(identityLastCertificateFile) - 1U] = '\0';

      strncpy(R.geraete_name,
              identityCertifiedSerialText,
              sizeof(R.geraete_name) - 1U);
      R.geraete_name[sizeof(R.geraete_name) - 1U] = '\0';
      tpMainConfigSave();

      if (importedPath != nullptr && importedPathSize > 0U)
      {
        strncpy(importedPath, matchedPath, importedPathSize - 1U);
        importedPath[importedPathSize - 1U] = '\0';
      }
      directory.close();
      identitySetError(errorText, errorTextSize, identityFlashText.t016);
      return true;
    }
  }

  directory.close();
  if (!foundCertificateFile)
  {
    identitySetError(errorText, errorTextSize,
                     identityFlashText.t068);
  }
  else
  {
    identitySetError(errorText, errorTextSize, lastError);
  }
  return false;
}
