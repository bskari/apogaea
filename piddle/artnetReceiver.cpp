#include "artnetReceiver.hpp"
#include "bluetoothAudio.hpp"
#include "secrets.hpp"

#include <string.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

#define ARTNET_PORT           6454
#define ARTNET_TIMEOUT_MS     5000
#define ARTNET_UNIVERSE_OFFSET   0   // Strip 0 = universe 0

volatile bool artnetActive  = false;
volatile bool artnetEnabled = false;

CRGB artnetPixels[STRIP_COUNT][LEDS_PER_STRIP];

static WiFiUDP udp;
static volatile uint32_t lastPacketMs = 0;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void handleArtDmx(const uint8_t* buf, int len);
static void sendArtPollReply(const IPAddress& dest, uint8_t bindIndex,
                             uint8_t startUniverse, uint8_t numPorts);

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void setupArtnet() {
  teardownBluetoothAudio();
  Serial.printf("ArtNet: connecting to WiFi '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ArtNet: WiFi connection failed");
    return;
  }

  Serial.printf("ArtNet: connected, IP %s\n", WiFi.localIP().toString().c_str());

  if (MDNS.begin("piddle")) {
    Serial.println("ArtNet: mDNS piddle.local registered");
  }

  udp.begin(ARTNET_PORT);
  Serial.printf("ArtNet: listening on UDP port %d\n", ARTNET_PORT);

  xTaskCreatePinnedToCore(
    artnetReceiverFunction,
    "artnetReceiver",
    4000,    // Stack words
    nullptr,
    2,       // Priority - higher than display task (1) so packets are never dropped
    nullptr,
    0);      // Core 0, alongside display task
}

// ---------------------------------------------------------------------------
// Receiver task
// ---------------------------------------------------------------------------

void artnetReceiverFunction(void*) {
  static uint8_t buf[530]; // Largest valid ArtNet packet is 18 + 512 = 530 bytes

  while (1) {
    int pktLen = udp.parsePacket();

    if (pktLen <= 0) {
      // Nothing arrived - check timeout and yield to display task
      if (artnetActive && millis() - lastPacketMs > ARTNET_TIMEOUT_MS) {
        artnetActive = false;
        Serial.println("ArtNet: signal lost, reverting to audio mode");
      }
      vTaskDelay(1 / portTICK_PERIOD_MS);
      continue;
    }

    int readLen = udp.read(buf, sizeof(buf));

    // Validate Art-Net ID header
    if (readLen < 10 || memcmp(buf, "Art-Net\0", 8) != 0) continue;

    uint16_t opcode = (uint16_t)buf[8] | ((uint16_t)buf[9] << 8);

    if (opcode == 0x5000 && readLen >= 18) {
      // ArtDMX
      handleArtDmx(buf, readLen);
      lastPacketMs = millis();
      if (!artnetActive) {
        artnetActive = true;
        Serial.println("ArtNet: receiving, switched to ArtNet mode");
      }
    } else if (opcode == 0x2000) {
      // ArtPoll - reply once per group of 4 universes using BindIndex
      IPAddress src = udp.remoteIP();
      sendArtPollReply(src, 1,  0, 4);
      sendArtPollReply(src, 2,  4, 4);
      sendArtPollReply(src, 3,  8, 4);
      sendArtPollReply(src, 4, 12, 3);
    }
  }
}

// ---------------------------------------------------------------------------
// Packet handlers
// ---------------------------------------------------------------------------

static void handleArtDmx(const uint8_t* buf, int len) {
  // ArtDMX layout:
  //   [0-7]   ID "Art-Net\0"
  //   [8-9]   OpCode 0x5000 (LE)
  //   [10-11] ProtVer (BE)
  //   [12]    Sequence
  //   [13]    Physical
  //   [14-15] Universe (LE)
  //   [16-17] Length in bytes (BE, must be even)
  //   [18+]   DMX data
  uint16_t universe = (uint16_t)buf[14] | ((uint16_t)buf[15] << 8);
  uint16_t dmxLen   = ((uint16_t)buf[16] << 8) | (uint16_t)buf[17];

  int strip = (int)universe - ARTNET_UNIVERSE_OFFSET;
  if (strip < 0 || strip >= STRIP_COUNT) return;
  if (len < 18 + (int)dmxLen) return;

  const uint8_t* dmx = buf + 18;
  int pixels = min((int)(dmxLen / 3), (int)LEDS_PER_STRIP);

  for (int i = 0; i < pixels; i++) {
    artnetPixels[strip][i].r = dmx[i * 3 + 0];
    artnetPixels[strip][i].g = dmx[i * 3 + 1];
    artnetPixels[strip][i].b = dmx[i * 3 + 2];
  }
}

static void sendArtPollReply(const IPAddress& dest, uint8_t bindIndex,
                             uint8_t startUniverse, uint8_t numPorts) {
  // ArtPollReply is always exactly 239 bytes.
  uint8_t reply[239];
  memset(reply, 0, sizeof(reply));

  IPAddress ip = WiFi.localIP();
  uint8_t mac[6];
  WiFi.macAddress(mac);

  // Header
  memcpy(reply + 0, "Art-Net\0", 8);
  reply[8]  = 0x00; reply[9]  = 0x21;          // OpCode ArtPollReply (LE)

  // IP and port
  reply[10] = ip[0]; reply[11] = ip[1];
  reply[12] = ip[2]; reply[13] = ip[3];
  reply[14] = 0x36;  reply[15] = 0x19;          // Port 6454 (LE)

  // Protocol version
  reply[16] = 0x00; reply[17] = 0x0E;           // ProtVer 14

  // Net/sub switch (universe bits 14:8 and 7:4 - all zero for universes 0-127)
  reply[18] = 0x00; reply[19] = 0x00;

  reply[20] = 0x00; reply[21] = 0xFF;           // Oem (generic)
  reply[23] = 0xD2;                             // Status1: indicators normal

  // Node names
  strncpy((char*)reply + 26, "Phonic Bloom",             17); // ShortName (18 bytes)
  strncpy((char*)reply + 44, "Piddle ArtNet Controller", 63); // LongName (64 bytes)
  snprintf((char*)reply + 108, 64, "#0001 [%04d] OK", bindIndex); // NodeReport

  // Ports
  reply[172] = 0x00; reply[173] = numPorts;     // NumPorts (BE, max 4)
  for (int i = 0; i < numPorts; i++) {
    reply[174 + i] = 0x80;                      // PortTypes: DMX output
    reply[182 + i] = 0x80;                      // GoodOutput: data transmitted
    reply[190 + i] = startUniverse + i;         // SwOut: universe number for this port
  }

  // Node style and identity
  reply[200] = 0x00;                            // Style: StNode
  memcpy(reply + 201, mac, 6);                  // MAC address

  // BindIp and BindIndex (for multi-port nodes with >4 universes)
  reply[207] = ip[0]; reply[208] = ip[1];
  reply[209] = ip[2]; reply[210] = ip[3];
  reply[211] = bindIndex;

  reply[212] = 0x08;                            // Status2: supports 15-bit universe addressing

  udp.beginPacket(dest, ARTNET_PORT);
  udp.write(reply, sizeof(reply));
  udp.endPacket();
}
