/*
 * +==========================================================+
 * |     _                                _     _       _     |
 * |    / \   _ __ __ _ _ __  _   _  __ _| |   (_)_ __ | | __ |
 * |   / _ \ | '__/ _` | '_ \| | | |/ _` | |   | | '_ \| |/ / |
 * |  / ___ \| | | (_| | | | | |_| | (_| | |___| | | | |   <  |
 * | /_/   \_\_|  \__,_|_| |_|\__, |\__,_|_____|_|_| |_|_|\_\ |
 * |                          |___/                           |
 * +==========================================================+
 *
 * AranyaLink — Wildfire Detection Mesh Node
 * Sensors  : DHT11 (GPIO 23) | IR Flame Sensor (GPIO 25) | Neo-6M GPS (GPIO 16/17)
 * Network  : ESP-NOW broadcast mesh with loop-prevention and multi-node verification
 */

#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>
#include <TinyGPSPlus.h>

// ─── Node Identity ─────────────────────────────────────────────────────────────
#define NODE_ID             1

// ─── GPS Fallback Coordinates (used until GPS acquires a fix) ──────────────────
#define NODE_LAT_FALLBACK   12.345678
#define NODE_LON_FALLBACK   98.765432

// ─── Detection Thresholds ──────────────────────────────────────────────────────
#define TEMP_THRESHOLD      50.0f

// ─── Pin Definitions ───────────────────────────────────────────────────────────
#define DHT_PIN             23
#define DHT_TYPE            DHT11
#define FLAME_PIN           25
#define FLAME_ACTIVE        LOW
#define GPS_RX_PIN          16
#define GPS_TX_PIN          17
#define GPS_BAUD            9600

// ─── Network & Timing ──────────────────────────────────────────────────────────
#define SEEN_PACKET_SIZE    10
#define MAX_VERIFIERS       5
#define SENSOR_INTERVAL     2000UL

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ─── Packet Structure ──────────────────────────────────────────────────────────
typedef struct __attribute__((packed)) AranyaPacket {
    uint32_t      packetID;
    int           originNodeID;
    float         temperature;
    float         humidity;
    uint8_t       flameDetected;
    double        latitude;
    double        longitude;
    unsigned long timestampMillis;
    int           verifyingNodes[MAX_VERIFIERS];
    int           verificationCount;
} AranyaPacket;

// ─── Globals ───────────────────────────────────────────────────────────────────
DHT                 dht(DHT_PIN, DHT_TYPE);
TinyGPSPlus         gps;
HardwareSerial      gpsSerial(2);
esp_now_peer_info_t peerInfo;

uint32_t      seenPackets[SEEN_PACKET_SIZE];
int           seenHead     = 0;
unsigned long lastSensorMs = 0;
double        currentLat   = NODE_LAT_FALLBACK;
double        currentLon   = NODE_LON_FALLBACK;
bool          gpsFix       = false;

// ─── Helpers ───────────────────────────────────────────────────────────────────
bool packetSeen(uint32_t id) {
    for (int i = 0; i < SEEN_PACKET_SIZE; i++)
        if (seenPackets[i] == id) return true;
    return false;
}

void markSeen(uint32_t id) {
    seenPackets[seenHead] = id;
    seenHead = (seenHead + 1) % SEEN_PACKET_SIZE;
}

void broadcastPacket(AranyaPacket &pkt) {
    esp_now_send(broadcastAddress, (uint8_t *)&pkt, sizeof(AranyaPacket));
}

// ─── ESP-NOW Callbacks ─────────────────────────────────────────────────────────
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(AranyaPacket)) return;

    AranyaPacket pkt;
    memcpy(&pkt, incomingData, sizeof(AranyaPacket));

    if (packetSeen(pkt.packetID)) return;

    float   localTemp  = dht.readTemperature();
    float   localHum   = dht.readHumidity();
    uint8_t localFlame = (digitalRead(FLAME_PIN) == FLAME_ACTIVE) ? 1 : 0;

    if (!isnan(localTemp) && !isnan(localHum) &&
        localTemp > TEMP_THRESHOLD && localFlame == 1) {
        if (pkt.verificationCount < MAX_VERIFIERS) {
            pkt.verifyingNodes[pkt.verificationCount] = NODE_ID;
            pkt.verificationCount++;
        }
    }

    broadcastPacket(pkt);
    markSeen(pkt.packetID);

    Serial.printf("[RELAY] ID:%u  Origin:Node%d  Verif:%d\n",
                  pkt.packetID, pkt.originNodeID, pkt.verificationCount);
}

// ─── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    pinMode(FLAME_PIN, INPUT);
    dht.begin();
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[FATAL] ESP-NOW init failed. Restarting.");
        delay(1000);
        ESP.restart();
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[FATAL] Peer add failed. Restarting.");
        delay(1000);
        ESP.restart();
    }

    memset(seenPackets, 0, sizeof(seenPackets));
    randomSeed(esp_random());

    Serial.println("==============================");
    Serial.printf( " AranyaLink  |  Node %d\n", NODE_ID);
    Serial.println("==============================");
    Serial.printf( " MAC : %s\n", WiFi.macAddress().c_str());
    Serial.printf( " Temp threshold : %.1f C\n", TEMP_THRESHOLD);
    Serial.printf( " Flame pin      : GPIO %d\n", FLAME_PIN);
    Serial.println(" GPS            : Acquiring...");
    Serial.println("==============================\n");
}

// ─── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    while (gpsSerial.available())
        gps.encode(gpsSerial.read());

    if (gps.location.isValid()) {
        currentLat = gps.location.lat();
        currentLon = gps.location.lng();
        gpsFix     = true;
    }

    unsigned long now = millis();
    if (now - lastSensorMs >= SENSOR_INTERVAL) {
        lastSensorMs = now;

        float   temp  = dht.readTemperature();
        float   hum   = dht.readHumidity();
        uint8_t flame = (digitalRead(FLAME_PIN) == FLAME_ACTIVE) ? 1 : 0;

        if (isnan(temp) || isnan(hum)) {
            Serial.println("[WARN] DHT11 read failed.");
            return;
        }

        Serial.printf("[%lu] T:%.1fC  H:%.1f%%  Flame:%-3s  GPS:%-4s  (%.6f, %.6f)\n",
                      now, temp, hum,
                      flame  ? "YES" : "no",
                      gpsFix ? "FIX" : "SRCH",
                      currentLat, currentLon);

        if (temp > TEMP_THRESHOLD && flame == 1) {
            AranyaPacket pkt;
            pkt.packetID          = (uint32_t)esp_random();
            pkt.originNodeID      = NODE_ID;
            pkt.temperature       = temp;
            pkt.humidity          = hum;
            pkt.flameDetected     = 1;
            pkt.latitude          = currentLat;
            pkt.longitude         = currentLon;
            pkt.timestampMillis   = millis();
            memset(pkt.verifyingNodes, 0, sizeof(pkt.verifyingNodes));
            pkt.verificationCount = 0;

            if (!packetSeen(pkt.packetID)) {
                broadcastPacket(pkt);
                markSeen(pkt.packetID);
                Serial.printf("[ALERT] FIRE DETECTED! ID:%u  T:%.1fC  Lat:%.6f  Lon:%.6f\n",
                              pkt.packetID, temp, currentLat, currentLon);
            }
        }
    }
}
