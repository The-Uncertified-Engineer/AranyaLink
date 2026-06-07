# AranyaLink 🌲

### Open-Source ESP32 Wildfire Detection Mesh Network

**Aranya (अरण्य)** is Sanskrit for *forest*. This project is named after what it protects.

AranyaLink is a self-healing, infrastructure-free wildfire detection mesh network built on ESP32 microcontrollers. Nodes communicate directly over ESP-NOW — no Wi-Fi router, no cellular tower, no cloud subscription, no internet connection required at any point in the detection chain. A fire detected deep in a forest hops node-to-node until a GPS-precise alert reaches a Ground Control Station at the forest perimeter, in seconds.

**Cost per node: under $15. Infrastructure required: none.**

---

## How It Works

Each node continuously monitors temperature (DHT11) and infrared flame (KY-026). When both thresholds are crossed simultaneously, the node generates a unique alert packet containing its GPS coordinates, sensor readings, and Node ID, then broadcasts it over ESP-NOW.

Every neighboring node that receives the packet:
- Reads its own sensors immediately
- If it also detects fire — appends its Node ID to a `verifyingNodes[]` array, increments `verificationCount`
- If sensors are normal — forwards the packet unchanged

The packet propagates hop-by-hop until it reaches the **Ground Control Station (GCS)** — a dedicated ESP32 connected to a laptop — which prints a formatted alert with GPS coordinates, confidence level, and a live Google Maps link.

A rolling 10-slot seen-packet buffer on every node prevents alerts from looping indefinitely through the mesh.

```
Node 1 (fire) ──broadcasts──► Node 2 (verifies) ──► GCS → ALERT
                    └──────────► Node 3 (relays)  ──► GCS → [DUP]
```

---

## Features

- **Zero infrastructure** — No Wi-Fi, no router, no internet, no cloud
- **Self-healing mesh** — Nodes are fully independent; destroying any single node does not break the network
- **Dual-sensor verification** — Temperature AND flame must both trigger to generate an alert, minimising false positives
- **Multi-node confirmation** — `verifyingNodes[]` array builds a confidence score as more nodes independently confirm the fire
- **Live GPS coordinates** — Neo-6M GPS on every node; hardcoded fallback used until fix is acquired
- **Google Maps link** — GCS prints a direct Maps URL for every unique alert
- **Loop prevention** — Rolling packetID buffer stops alerts from circling the mesh
- **$15/node** — All components available globally from standard electronics suppliers

---

## Hardware

### Per sensor node

| Component | Purpose |
|---|---|
| ESP32 DevKit v1 (30-pin) | Controller + ESP-NOW radio |
| DHT11 | Temperature and humidity |
| IR Flame Sensor (KY-026 / 3-pin) | Infrared flame detection |
| Neo-6M GPS module | Real-time coordinates |
| 10kΩ resistor | DHT11 data line pull-up |

### Ground Control Station

| Component | Purpose |
|---|---|
| ESP32 DevKit v1 | ESP-NOW receiver |
| USB cable + laptop | Serial Monitor alert display |

---

## Pin Connections

### Sensor Node

| Sensor | Sensor Pin | ESP32 Pin | Note |
|---|---|---|---|
| DHT11 | VCC | 3V3 | |
| DHT11 | DATA | GPIO 23 | + 10kΩ pull-up to 3V3 |
| DHT11 | GND | GND | |
| IR Flame | VCC | 3V3 | |
| IR Flame | DO | GPIO 25 | Active LOW by default |
| IR Flame | GND | GND | |
| Neo-6M GPS | VCC | 3V3 | Check your module |
| Neo-6M GPS | TX | GPIO 16 | ESP32 Serial2 RX |
| Neo-6M GPS | RX | GPIO 17 | ESP32 Serial2 TX (optional) |
| Neo-6M GPS | GND | GND | |

---

## Software & Libraries

**Arduino IDE** with ESP32 board support installed via Boards Manager:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Required libraries (install via Library Manager):

| Library | Author |
|---|---|
| DHT sensor library | Adafruit |
| Adafruit Unified Sensor | Adafruit |
| TinyGPS++ | Mikal Hart |

`WiFi.h` and `esp_now.h` are included with the ESP32 board package — no separate install needed.

---

## Installation

1. Clone this repository
```bash
git clone https://github.com/yourusername/AranyaLink.git
```

2. Open `AranyaLink_Node/AranyaLink_Node.ino` in Arduino IDE

3. Edit the configuration block at the top of the file:
```cpp
#define NODE_ID             1        // Unique integer per node
#define NODE_LAT_FALLBACK   12.345678
#define NODE_LON_FALLBACK   98.765432
#define TEMP_THRESHOLD      50.0f    // °C
#define FLAME_ACTIVE        LOW      // Change to HIGH if your module is active-HIGH
```

4. Flash to ESP32. Repeat for each node, incrementing `NODE_ID` each time.

5. Flash `AranyaLink_GCS/AranyaLink_GCS.ino` to a separate ESP32 — no configuration needed.

6. Open Serial Monitor at **115200 baud** on the GCS unit.

---

## Configuration Reference

| Define | Default | Description |
|---|---|---|
| `NODE_ID` | `1` | Unique integer identifier for this node |
| `NODE_LAT_FALLBACK` | `12.345678` | Latitude used before GPS fix |
| `NODE_LON_FALLBACK` | `98.765432` | Longitude used before GPS fix |
| `TEMP_THRESHOLD` | `50.0f` | Fire detection temperature in °C |
| `FLAME_ACTIVE` | `LOW` | Flame sensor active state |
| `SEEN_PACKET_SIZE` | `10` | Loop prevention buffer depth |
| `MAX_VERIFIERS` | `5` | Max nodes in verifyingNodes[] |
| `SENSOR_INTERVAL` | `2000UL` | Sensor polling interval in ms |

---

## GCS Alert Output

```
==========================================
  ARANYALINK FIRE ALERT  #1
==========================================
  Packet ID    : 2847193042
  Origin Node  : Node 1
  Relayed via  : A4:CF:12:B3:44:F1
==========================================
  Temperature  : 63.40 C
  Humidity     : 28.10 %
  Flame Sensor : ** FLAME DETECTED **
==========================================
  Latitude     : 12.34567800
  Longitude    : 98.76543200
  Maps Link    : https://maps.google.com/?q=12.34567800,98.76543200
==========================================
  Node Uptime  : 4m 12s (252187 ms)
  Verifications: 2 / 5
  Confidence   : MEDIUM
  Verified by  : Nodes [ 2, 3 ]
==========================================
```

### Confidence levels

| verificationCount | Confidence | Suggested response |
|---|---|---|
| 0 | Unverified | Monitor |
| 1 | Low | Investigate |
| 2–3 | Medium | Dispatch |
| 4–5 | High | Full emergency response |

---

## Repository Structure

```
AranyaLink/
├── AranyaLink_Node/
│   └── AranyaLink_Node.ino     # Sensor node sketch
├── AranyaLink_GCS/
│   └── AranyaLink_GCS.ino      # Ground Control Station sketch
├── docs/
│   ├── wiring_diagram.png
│   ├── network_topology.png
│   └── gcs_output_example.png
├── README.md
└── LICENSE
```

---

## Roadmap

- [ ] LoRa (SX1276) variant for 1–2km hop range
- [ ] Telegram Bot alert forwarding via SIM800L
- [ ] SD card logging on GCS
- [ ] OLED display on node for standalone status
- [ ] MQTT bridge for cloud dashboard integration
- [ ] ESP32-CAM visual confirmation module

---

## Coverage Guide

| Nodes | Spacing | Coverage |
|---|---|---|
| 4 | 120m | ~400m forest edge |
| 8 | 120m | ~900m forest edge |
| 10 | 100m grid | ~9 hectares interior |
| 20 | 100m grid | ~20 hectares interior |

---

## Contributing

Pull requests are welcome. For major changes, open an issue first.

If you deploy AranyaLink anywhere, please open an issue tagged `deployment` and share where — every real-world deployment helps validate and improve the project.

---

## Links

- 📖 Full Instructables build guide — *[link]*

---

## License

MIT License — free to use, modify, and deploy. If AranyaLink helps protect a forest somewhere in the world, that is attribution enough.

---

*Built by a college student with $15 in components and an afternoon. If that is possible, the question worth asking is what exactly is stopping a nation with a forest ministry, a disaster management budget, and thousands of kilometres of fire-vulnerable forest from deploying ten thousand of these tomorrow.*
