/*
 * AranyaLink — Ground Control Station (GCS)
 * Role     : Passive ESP-NOW receiver. Logs all wildfire alerts to Serial.
 * No sensors, no peers needed. Flash this to a dedicated ESP32 connected to a PC.
 * Open Serial Monitor at 115200 baud to view incoming alerts.
 * Libraries: WiFi.h | esp_now.h
 */

#include <WiFi.h>
#include <esp_now.h>

// ─── Packet Structure (must be identical to the Node sketch) ───────────────────
#define MAX_VERIFIERS  5

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

// ─── Deduplication ─────────────────────────────────────────────────────────────
#define SEEN_PACKET_SIZE  30

uint32_t seenPackets[SEEN_PACKET_SIZE];
int      seenHead    = 0;
uint32_t totalRx     = 0;
uint32_t totalAlerts = 0;

bool packetSeen(uint32_t id) {
    for (int i = 0; i < SEEN_PACKET_SIZE; i++)
        if (seenPackets[i] == id) return true;
    return false;
}

void markSeen(uint32_t id) {
    seenPackets[seenHead] = id;
    seenHead = (seenHead + 1) % SEEN_PACKET_SIZE;
}

// ─── ESP-NOW Receive Callback ──────────────────────────────────────────────────
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(AranyaPacket)) {
        Serial.printf("[GCS] Malformed packet (%d bytes) from %02X:%02X:%02X:%02X:%02X:%02X\n",
                      len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return;
    }

    AranyaPacket pkt;
    memcpy(&pkt, incomingData, sizeof(AranyaPacket));
    totalRx++;

    if (packetSeen(pkt.packetID)) {
        Serial.printf("[DUP]  ID:%-10u  via %02X:%02X:%02X:%02X:%02X:%02X  (Total Rx:%u)\n",
                      pkt.packetID,
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                      totalRx);
        return;
    }

    markSeen(pkt.packetID);
    totalAlerts++;

    const char *conf =
        pkt.verificationCount == 0 ? "UNVERIFIED" :
        pkt.verificationCount == 1 ? "LOW"        :
        pkt.verificationCount <= 3 ? "MEDIUM"     : "HIGH";

    unsigned long mins = pkt.timestampMillis / 60000UL;
    unsigned long secs = (pkt.timestampMillis % 60000UL) / 1000UL;

    Serial.println("\n==========================================");
    Serial.printf( "  ARANYALINK FIRE ALERT  #%u\n", totalAlerts);
    Serial.println("==========================================");
    Serial.printf( "  Packet ID    : %u\n",              pkt.packetID);
    Serial.printf( "  Origin Node  : Node %d\n",          pkt.originNodeID);
    Serial.printf( "  Relayed via  : %02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.println("------------------------------------------");
    Serial.printf( "  Temperature  : %.2f C\n",           pkt.temperature);
    Serial.printf( "  Humidity     : %.2f %%\n",          pkt.humidity);
    Serial.printf( "  Flame Sensor : %s\n",
                   pkt.flameDetected ? "** FLAME DETECTED **" : "No flame");
    Serial.println("------------------------------------------");
    Serial.printf( "  Latitude     : %.8f\n",             pkt.latitude);
    Serial.printf( "  Longitude    : %.8f\n",             pkt.longitude);
    Serial.printf( "  Maps Link    : https://maps.google.com/?q=%.8f,%.8f\n",
                   pkt.latitude, pkt.longitude);
    Serial.println("------------------------------------------");
    Serial.printf( "  Node Uptime  : %lum %lus (%lu ms)\n",
                   mins, secs, pkt.timestampMillis);
    Serial.printf( "  Verifications: %d / %d\n",
                   pkt.verificationCount, MAX_VERIFIERS);
    Serial.printf( "  Confidence   : %s\n",               conf);

    if (pkt.verificationCount > 0) {
        Serial.print("  Verified by  : Nodes [ ");
        for (int i = 0; i < pkt.verificationCount; i++) {
            Serial.print(pkt.verifyingNodes[i]);
            if (i < pkt.verificationCount - 1) Serial.print(", ");
        }
        Serial.println(" ]");
    }

    Serial.println("==========================================\n");
}

// ─── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("[FATAL] ESP-NOW init failed. Restarting.");
        delay(1000);
        ESP.restart();
    }

    esp_now_register_recv_cb(OnDataRecv);
    memset(seenPackets, 0, sizeof(seenPackets));

    Serial.println("\n==========================================");
    Serial.println("  ARANYALINK GROUND CONTROL STATION");
    Serial.println("==========================================");
    Serial.printf( "  GCS MAC  : %s\n",  WiFi.macAddress().c_str());
    Serial.println("  Protocol : ESP-NOW (Passive Receiver)");
    Serial.println("  Baud     : 115200");
    Serial.println("  Status   : LISTENING FOR ALERTS");
    Serial.println("==========================================\n");
}

// ─── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    static unsigned long lastHB = 0;
    unsigned long now = millis();
    if (now - lastHB >= 30000UL) {
        lastHB = now;
        unsigned long upMins = now / 60000UL;
        unsigned long upSecs = (now % 60000UL) / 1000UL;
        Serial.printf("[GCS] Up:%lum%lus  Total Rx:%u  Unique Alerts:%u\n",
                      upMins, upSecs, totalRx, totalAlerts);
    }
}
