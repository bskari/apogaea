// Only compiled into the ArtNet image (see ARTNET_IMAGE in piddle.ino).
#ifdef ARTNET_IMAGE
#include "artnetReceiver.hpp"

#include <string.h>
#include <WiFi.h>
#include <AsyncUDP.h>

#define ARTNET_PORT           6454
#define ARTNET_TIMEOUT_MS     2 * 60 * 1000
#define ARTNET_FRAME_TIMEOUT_MS 30 // Draw a partial frame if the rest hasn't arrived by then
#define ARTNET_UNIVERSE_OFFSET   0   // Strip 0 = universe 0

CRGB* artNetPixels = nullptr;
SemaphoreHandle_t artNetPixelsMutex = nullptr;

// AsyncUDP rather than WiFiUDP: a sender emits all universes of a frame back to back, and the
// socket-based WiFiUDP only buffers a handful of packets, so the later universes were being dropped.
static AsyncUDP udp;
static volatile uint32_t lastPacketMs = 0;
// Universes (bit N = strip N) received since the last frame was drawn. Guarded by artNetPixelsMutex.
static uint32_t receivedMask = 0;
static uint32_t firstPacketMs = 0; // When the first universe of the pending frame arrived.
static TaskHandle_t collectSamplesTaskHandle = nullptr;
static volatile bool* artNetModePtr = nullptr;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void handleArtDmx(const uint8_t* buf, int len);
static void onArtNetPacket(AsyncUDPPacket& packet);
static void sendArtPollReply(const IPAddress& dest, uint8_t bindIndex,
                             uint8_t startUniverse, uint8_t numPorts);

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void setupArtNet(TaskHandle_t collectSamplesTask, volatile bool* artNetMode) {
  collectSamplesTaskHandle = collectSamplesTask;
  artNetModePtr = artNetMode;
  *artNetModePtr = true;
  const char* const ssid = "Phonic Bloom Artnet";
  Serial.printf("%lu.%lu: ArtNet: starting AP '%s'...\n", millis() / 1000, millis() % 1000, ssid);
  WiFi.softAP(ssid);
  Serial.printf("%lu.%lu: ArtNet: AP up, IP %s\n", millis() / 1000, millis() % 1000, WiFi.softAPIP().toString().c_str());

  artNetPixelsMutex = xSemaphoreCreateMutex();
  udp.onPacket(onArtNetPacket);
  udp.listen(ARTNET_PORT);
  Serial.printf("%lu.%lu: ArtNet: listening on UDP port %d\n", millis() / 1000, millis() % 1000, ARTNET_PORT);

  xTaskCreatePinnedToCore(
    artNetReceiverFunction,
    "artNetReceiver",
    4000,    // Stack bytes
    nullptr,
    2,       // Priority - higher than display task (1) so packets are never dropped
    nullptr,
    1);      // Core 1, opposite of display task
}

bool artNetFrameReadyLocked() {
  if (receivedMask == 0) return false;
  const uint32_t allStrips = (1UL << STRIP_COUNT) - 1;
  if (receivedMask != allStrips && millis() - firstPacketMs < ARTNET_FRAME_TIMEOUT_MS) {
    return false;
  }
  receivedMask = 0;
  return true;
}

// ---------------------------------------------------------------------------
// Receiver task
// ---------------------------------------------------------------------------

// Runs on AsyncUDP's task for every packet received.
static void onArtNetPacket(AsyncUDPPacket& packet) {
  const uint8_t* buf = packet.data();
  const int readLen = packet.length();

  // Validate Art-Net ID header
  if (readLen < 10 || memcmp(buf, "Art-Net\0", 8) != 0) {
    Serial.printf("%lu: Received non-ArtNet packet\n", millis());
    return;
  }

  uint16_t opcode = (uint16_t)buf[8] | ((uint16_t)buf[9] << 8);

  if (opcode == 0x5000 && readLen >= 18) {
    // ArtDMX
    handleArtDmx(buf, readLen);
    lastPacketMs = millis();
    if (!artNetActive) {
      artNetActive = true;
      vTaskSuspend(collectSamplesTaskHandle);
      Serial.println("ArtNet: receiving");
    }
  } else if (opcode == 0x2000) {
    // ArtPoll - reply once per group of 4 universes using BindIndex
    IPAddress src = packet.remoteIP();
    Serial.printf("%lu.%lu: ArtNet: replying to ArtPoll from %s (%lu)\n", millis() / 1000, millis() % 1000, src.toString().c_str(), static_cast<uint32_t>(src));
    sendArtPollReply(src, 1,  0, 4);
    sendArtPollReply(src, 2,  4, 4);
    sendArtPollReply(src, 3,  8, 4);
    sendArtPollReply(src, 4, 12, 3);
  }
}

// Falls back to audio when the signal is lost. Packets themselves are handled by onArtNetPacket.
void artNetReceiverFunction(void*) {
  while (1) {
    if (artNetActive && millis() - lastPacketMs > ARTNET_TIMEOUT_MS) {
      Serial.printf("%lu.%lu: ArtNet: signal lost, resuming audio\n", millis() / 1000, millis() % 1000);
      udp.close(); // Stop packets re-activating ArtNet now that this task is going away
      artNetActive = false;
      vTaskResume(collectSamplesTaskHandle);
      *artNetModePtr = false;
      vTaskDelete(NULL);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
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
  const int activeLeds = LEDS_PER_STRIP - BLANK_LEDS_AT_END;
  const int pixels = min((int)(dmxLen / 3), activeLeds);

  xSemaphoreTake(artNetPixelsMutex, portMAX_DELAY);
  if (receivedMask == 0) {
    firstPacketMs = millis();
  }
  receivedMask |= 1UL << strip;
  for (int i = 0; i < pixels; i++) {
    artNetPixels[strip * LEDS_PER_STRIP + i].r = dmx[i * 3 + 0];
    artNetPixels[strip * LEDS_PER_STRIP + i].g = dmx[i * 3 + 1];
    artNetPixels[strip * LEDS_PER_STRIP + i].b = dmx[i * 3 + 2];
  }
  // The trailing blank LEDs are never driven by ArtNet data.
  for (int i = activeLeds; i < LEDS_PER_STRIP; i++) {
    artNetPixels[strip * LEDS_PER_STRIP + i] = CRGB::Black;
  }
  xSemaphoreGive(artNetPixelsMutex);
}

static void sendArtPollReply(const IPAddress& dest, uint8_t bindIndex,
                             uint8_t startUniverse, uint8_t numPorts) {
  // ArtPollReply is always exactly 239 bytes.
  uint8_t reply[239];
  ::memset(reply, 0, sizeof(reply));

  IPAddress ip = WiFi.softAPIP();
  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);

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
  strncpy((char*)reply + 26, "Phonic Bloom", 17); // ShortName (18 bytes)
  strncpy((char*)reply + 44, "Phonic ArtNet Controller", 63); // LongName (64 bytes)
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

  udp.writeTo(reply, sizeof(reply), dest, ARTNET_PORT);
}
#endif // ARTNET_IMAGE
