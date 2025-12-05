// -------- esp32_floor/include/config.h --------
#pragma once

// =============================
// Debug controls
// =============================
// 0=OFF, 1=ERROR, 2=INFO+VERBOSE (default), 3=DEBUG, 4=TRACE (very noisy)
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3
#endif

// Truncate long payloads in logs to avoid flooding the serial monitor
#ifndef DEBUG_TRUNC
#define DEBUG_TRUNC 96
#endif

// If enabled, calling code will invoke `debugPrintModel(Serial);` from the
// diagnostics path. Set to 1 to enable deep MODEL dumps (very verbose).
#ifndef DEBUG_PRINT_MODEL
#define DEBUG_PRINT_MODEL 1
#endif

// =============================

// Build‑time configuration
// =============================
// Security toggles
#define USE_TLS 1         // 0: MQTT over TCP/1883 (bench) | 1: MQTTS 8883 (HiveMQ Cloud)
#define ESPNOW_ENCRYPT 0  // 0: plaintext ESP‑NOW broadcast (encryption removed)
#define USE_I2C_ROOMBUS 1 // 1: poll Nanos over I²C and parse compact room strings

// Scale targets
#define MAX_FLOORS 8 // capability (indexes 0..7)
#define MAX_ROOMS 8  // capability per floor

// Identity (adjust per device)
#define SITE_ID "ELEC520"
#define FLOOR_ID 3 // ordinal 0..7. Example: Floor 0,1,2,...

// Wi‑Fi / MQTT
#define WIFI_SSID "VM0258084"
#define WIFI_PASS "b7xHsPcqmhyz"
#define MQTT_PORT (USE_TLS ? 8883 : 1883)

#define USE_LOCAL_MQTT_BROKER 0 

#if USE_LOCAL_MQTT_BROKER
#define MQTT_HOST "6db0e6b08705485e91842edd2b50fb71.s1.eu.hivemq.cloud"
#define MQTT_USER "elec520"
#define MQTT_PASS "Elec520secsys"

#else
#define MQTT_HOST "broker.hivemq.com"
#define MQTT_USER "user"
#define MQTT_PASS "password"
#define MQTT_CLIENT_ID SITE_ID "FLOOR" + String((int)FLOOR_ID)
#endif

// ESP‑NOW keys were removed; encryption is not used in this firmware

// I²C (ESP32 master <-> Nano slaves)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_CLOCK_HZ 100000
// Up to 8 room nodes; set 0 for unused slots
#define ROOM_I2C_ADDRS {0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19}
#define ROOM_ADDR_COUNT 8
#define I2C_REQUEST_LEN 64  // bytes requested per transaction
#define I2C_REPLY_WAIT_MS 3 // settle time between request/response (if needed)
#define I2C_RX_MAX 128      // max line length accepted from Nano
#define I2C_POLL_MS 50      // poll cadence (was 250)

// ---------------- Simulation (for testing only) ----------------
// --- I2C simulation controls (compile-time only) ---
#ifndef SIMULATE_I2C
#define SIMULATE_I2C 0 // 1=simulate; 0=real I2C
#endif

// Sim wave shape: 0=STATIC, 1=SINE, 2=RANDOM, 3=BURST
#ifndef I2C_SIM_MODE
#define I2C_SIM_MODE 1
#endif

#ifndef I2C_SIM_RATE_HZ
#define I2C_SIM_RATE_HZ 20 // lines per second (approx)
#endif

#ifndef I2C_SIM_SEED
#define I2C_SIM_SEED 0xA5C3D5u
#endif

// Crude fault injection (percent, 0..100) — compile-time
#ifndef I2C_SIM_DROP_PCT
#define I2C_SIM_DROP_PCT 0
#endif
#ifndef I2C_SIM_CORRUPT_PCT
#define I2C_SIM_CORRUPT_PCT 0
#endif

// Cadence (ms)
#define GOSSIP_BASE_MS 1000   // slower gossip to avoid WiFi conflicts
#define GOSSIP_JITTER_MS 200  // increased jitter for better collision avoidance
#define MQTT_SUMMARY_MS 10000 // reduced MQTT frequency to avoid WiFi conflicts
#define GOSSIP_ROOM_MS 2000   // slower room broadcasts to reduce conflicts

// Gossip de‑duplication
#define RECENT_CACHE_SIZE 32        // remember last 32 distinct messages
#define RECENT_TTL_MS 1000          // ignore duplicates seen within 1s (was 4s)
#define LEADER_PEER_TIMEOUT_MS 5000 // timeout for leader election
#define ROLE_SWITCH_MS 3000         // hysteresis for role changes

// ---------- Logging ----------
#if DEBUG_LEVEL >= 1
#define LOGE(...)                                           \
  do                                                        \
  {                                                         \
    Serial.printf("\n[ERR %lu] ", (unsigned long)millis()); \
    Serial.printf(__VA_ARGS__);                             \
    Serial.println();                                       \
  } while (0)
#else
#define LOGE(...) \
  do              \
  {               \
  } while (0)
#endif
#if DEBUG_LEVEL >= 2
#define LOGI(...)                                           \
  do                                                        \
  {                                                         \
    Serial.printf("\n[INF %lu] ", (unsigned long)millis()); \
    Serial.printf(__VA_ARGS__);                             \
    Serial.println();                                       \
  } while (0)
#else
#define LOGI(...) \
  do              \
  {               \
  } while (0)
#endif
#if DEBUG_LEVEL >= 3
#define LOGV(...)               \
  do                            \
  {                             \
    Serial.printf("[V] ");      \
    Serial.printf(__VA_ARGS__); \
    Serial.println();           \
  } while (0)
#else
#define LOGV(...) \
  do              \
  {               \
  } while (0)
#endif
#if DEBUG_LEVEL >= 4
#define LOGT(...)                                        \
  do                                                     \
  {                                                      \
    Serial.printf("[TR %lu] ", (unsigned long)millis()); \
    Serial.printf(__VA_ARGS__);                          \
    Serial.println();                                    \
  } while (0)
#else
#define LOGT(...) \
  do              \
  {               \
  } while (0)
#endif