// -------- esp32_floor/include/config.h --------
#pragma once

// =============================
// Debug controls
// =============================
// 0=OFF, 1=ERROR, 2=INFO+VERBOSE (default), 3=DEBUG, 4=TRACE (very noisy)
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 2
#endif

// Truncate long payloads in logs to avoid flooding the serial monitor
#ifndef DEBUG_TRUNC
#define DEBUG_TRUNC 96
#endif

// =============================
// Build‑time configuration
// =============================
// Security toggles
#define USE_TLS 1 // 0: MQTT over TCP/1883 (bench) | 1: MQTTS 8883 (HiveMQ Cloud)
#define ESPNOW_ENCRYPT 0 // 0: plaintext ESP‑NOW broadcast | 1: encrypted ESP‑NOW peers
#define USE_I2C_ROOMBUS 1 // 1: poll Nanos over I²C and parse compact room strings


// Scale targets
#define MAX_FLOORS 8 // capability (indexes 0..7)
#define MAX_ROOMS 8 // capability per floor


// Identity (adjust per device)
#define SITE_ID "ELEC520"
#define FLOOR_ID 1 // ordinal 0..7. Example: Floor 0,1,2,...


// Wi‑Fi / MQTT
#define WIFI_SSID "VM0258084"
#define WIFI_PASS "b7xHsPcqmhyz"
#define MQTT_HOST "6db0e6b08705485e91842edd2b50fb71.s1.eu.hivemq.cloud"
#define MQTT_PORT (USE_TLS ? 8883 : 1883)
#define MQTT_USER "elec520"
#define MQTT_PASS "Elec520secsys"


// ESP‑NOW keys (used if ESPNOW_ENCRYPT==1)
#define ESPNOW_PMK "0123456789ABCDEF0123456789ABCDEF" // 32 hex (16 bytes)
#define ESPNOW_LTK "ABCDEF0123456789ABCDEF0123456789" // 32 hex (16 bytes)


// I²C (ESP32 master <-> Nano slaves)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_CLOCK_HZ 100000
// Up to 8 room nodes; set 0 for unused slots
#define ROOM_I2C_ADDRS {0x21,0x22,0x23,0x00,0x00,0x00,0x00,0x00}
#define ROOM_ADDR_COUNT 8
#define I2C_REQUEST_LEN 64 // bytes requested per transaction
#define I2C_REPLY_WAIT_MS 3 // settle time between request/response (if needed)
#define I2C_RX_MAX 128 // max line length accepted from Nano
#define I2C_POLL_MS 50 // poll cadence (was 250)

// ---------------- Simulation (for testing only) ----------------
// Enable to generate synthetic I2C room lines locally (useful for MQTT testing)
#ifndef SIMULATE_I2C
#define SIMULATE_I2C 1
#endif

#ifndef SIMULATE_I2C_MS
#define SIMULATE_I2C_MS 100u // was 500
#endif


// Cadence (ms)
#define GOSSIP_BASE_MS 100u // base gossip period (was 400)
#define GOSSIP_JITTER_MS 50u // add per‑node jitter based on MAC rank (was 200)
#define MQTT_SUMMARY_MS 2000u // leader publishes summary every 2s (was 5s)
#define GOSSIP_ROOM_MS 300u // resend one cached local room line every 300ms (was 1.2s)


// Gossip de‑duplication
#define RECENT_CACHE_SIZE 32 // remember last 32 distinct messages 
#define RECENT_TTL_MS 1000u // ignore duplicates seen within 1s (was 4s)

