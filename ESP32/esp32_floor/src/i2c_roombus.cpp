#include "i2c_roombus.h"
#include "config.h"

// --- helpers (no logging macro redefinitions) ---
static inline bool isPrintableAscii_(char c) {
  return c == '\n' || c == '\r' || (c >= 32 && c <= 126);
}

// Print readable ascii representation of the buffer (non-printables are shown as '.')
static void dumpHex_(const uint8_t* p, int n) {
  if (!p || n <= 0) {
    LOGT("[I2C] STR 0B: (empty)");
    return;
  }
  // Build a printable string, replacing non-printables with '.'
  String s;
  s.reserve(n + 1);
  for (int i = 0; i < n; i++) {
    const char ch = (char)p[i];
    if (isPrintableAscii_(ch) && ch != '\r' && ch != '\n') {
      s += ch;
    } else if (ch == '\r' || ch == '\n') {
      // Keep newlines intact for readability
      s += ch;
    } else {
      s += '.'; // placeholder for non-printable
    }
  }
  LOGT("[I2C] STR %dB: %s", n, s.c_str());
}

// Keep an accumulator per configured address so we can return full lines
static String s_accum[ROOM_ADDR_COUNT];

static int idxFromAddr_(uint8_t addr) {
  static const uint8_t ADDR_LIST[] = ROOM_I2C_ADDRS;
  for (int i = 0; i < ROOM_ADDR_COUNT; ++i) {
    if (ADDR_LIST[i] == addr) return i;
  }
  return -1;
}

#if SIMULATE_I2C
// --------- COMPILE-TIME I2C SIM ---------
static uint32_t rng_ = I2C_SIM_SEED;
static inline uint32_t xorshift32(){ uint32_t x=rng_; x^=x<<13; x^=x>>17; x^=x<<5; rng_=x?x:0x1u; return rng_; }
static inline float frand(){ return (xorshift32() & 0xFFFFFF) / float(); }
0x1000000u
static uint32_t last_ms_ = 0;
static const uint8_t ADDR_LIST[] = ROOM_I2C_ADDRS;

int RoomBusI2C::readLine(uint8_t addr, uint8_t* out, size_t outMax,
                         size_t /*requestLen*/, uint16_t /*settleMs*/) {
  LOGT("SIM: readLine(addr=0x%02X, outMax=%u)", addr, (unsigned)outMax);
  const uint32_t period_ms = (uint32_t)(1000.0f / (I2C_SIM_RATE_HZ > 0 ? I2C_SIM_RATE_HZ : 1));
  const uint32_t now = millis();
  if (now - last_ms_ < period_ms) return 0;
  last_ms_ = now;

  // pick a valid address from compile-time list
  if (!addr) {
    // if caller passes 0, choose one for them (round-robin via RNG)
    uint8_t tries=0;
    do { addr = ADDR_LIST[(xorshift32()%ROOM_ADDR_COUNT)]; } while(!addr && ++tries<ROOM_ADDR_COUNT);
    if (!addr) {
      LOGV("SIM: no valid addr found after selection; returning 0");
      return 0;
    }
    LOGT("SIM: selected addr 0x%02X for this tick", addr);
  }
  }

  // drop frame (compile-time probability)
  if ((int)(frand()*100) < I2C_SIM_DROP_PCT) {
    LOGV("SIM: randomly dropping frame for 0x%02X (drop pct=%d)", addr, I2C_SIM_DROP_PCT);
    return 0;
  }

  // Generate values based on mode
  float t, h; int co2, pir;
  switch (I2C_SIM_MODE) {
    case 0: // STATIC
      t=21.5f; h=42.0f; co2=520; pir=0; break;
    case 1: { // SINE (60 s cycle)
      const float ph = (now % 60000) / 60000.0f;
      t   = 20.0f + 3.0f * sinf(6.2831853f * ph + 0.3f);
      h   = 35.0f + 8.0f * sinf(6.2831853f * ph + 1.7f);
      co2 = 480   + (int)(80 * sinf(6.2831853f * ph + 0.9f));
      pir = (ph>0.25f && ph<0.35f) ? 1 : 0;
    } break;
    case 3: { // BURST (5 s high / 5 s low)
      bool burst = ((now/5000) % 2) == 1;
      t = burst ? 26.0f : 21.0f;
      h = burst ? 55.0f : 40.0f;
      co2 = burst ? 900 : 500;
      pir = burst ? 1 : 0;
    } break;
    default: // RANDOM
      t   = 18.0f + 8.0f * frand();
      h   = 30.0f + 30.0f * frand();
      co2 = 450   + (int)(700 * frand());
      pir = frand() > 0.8f;
      break;
  }

  // Basic corruption (truncate tail) if enabled
  char line[128];
  int roomId = 1 + (addr & 0x0F);
  int n = snprintf(line, sizeof(line),
                   "f/%d r/%02d t/%.1f h/%.0f co2/%d pir/%d",
                   FLOOR_ID, roomId, t, h, co2, pir);

  if ((int)(frand()*100) < I2C_SIM_CORRUPT_PCT && n>6) n -= 1 + (xorshift32() % min(n-6, 4));

  if ((size_t)n > outMax) {
    LOGV("SIM: generated line length %d > outMax %u; truncating", n, (unsigned)outMax);
    n = (int)outMax;
  }
  memcpy(out, line, n);
  // Show the line and raw bytes in logs for visibility
  LOGT("SIM 0x%02X -> %.*s", addr, n, (const char*)out);
  dumpHex_(out, n);
  return n;
}

#else

int RoomBusI2C::readLine(uint8_t addr, uint8_t* out, size_t outMax,
                         size_t requestLen, uint16_t settleMs)
{
  LOGT("HW: readLine(addr=0x%02X, outMax=%u, requestLen=%u, settleMs=%u)", addr, (unsigned)outMax, (unsigned)requestLen, (unsigned)settleMs);
  if (!addr || !out || !outMax) {
    LOGV("HW: invalid params to readLine(addr=0x%02X, out=%p, outMax=%u)", addr, out, (unsigned)outMax);
    return 0;
  }

  // Quick probe (ACK?) — avoids long timeouts if the Nano is absent
  Wire.beginTransmission(addr);
  uint8_t tx = Wire.endTransmission(true); // send STOP
  if (tx != 0) {
    // No device at this address right now
    LOGV("HW: probe failed at 0x%02X (tx=%u)", addr, (unsigned)tx);
    return 0;
  }

  if (settleMs) {
    LOGT("HW: waiting %u ms for settle", (unsigned)settleMs);
    delayMicroseconds(settleMs * 1000UL);
  }

  const int want = (int)min(requestLen, outMax);
  LOGT("HW: requesting %d bytes from 0x%02X (want %d)", want, addr, want);
  const int got  = Wire.requestFrom((int)addr, want, (int)true); // STOP after read
  if (got <= 0) {
    LOGT("HW: Wire.requestFrom returned %d for addr 0x%02X", got, addr);
    return 0;
  }
  LOGT("HW: Wire.requestFrom got %d bytes from 0x%02X", got, addr);

  const int idx = idxFromAddr_(addr);
  if (idx < 0) {
    // Address not in ROOM_I2C_ADDRS — discard bytes
    LOGT("HW: addr 0x%02X not in ROOM_I2C_ADDRS; discarding %d bytes", addr, got);
    while (Wire.available()) (void)Wire.read();
    return 0;
  }

  // Append only printable bytes into the per-address accumulator
  String& acc = s_accum[idx];
  LOGT("HW: acc[%d] length before read = %d", idx, acc.length());
  int appendCount = 0;
  int skipCount = 0;
  while (Wire.available()) {
    char c = (char)Wire.read();
    if (!isPrintableAscii_(c)) {
      skipCount++;
      continue;
    }
    acc += c;
    appendCount++;
    // Avoid unbounded growth if no newline ever arrives
    if (acc.length() > (int)I2C_RX_MAX * 2) {
      LOGT("HW: acc[%d] length %d > %d; trimming", idx, acc.length(), I2C_RX_MAX * 2);
      acc.remove(0, acc.length() - I2C_RX_MAX);
      LOGT("HW: acc[%d] length after trim = %d", idx, acc.length());
    }
  }
  if (appendCount || skipCount) {
    LOGT("HW: acc[%d] read summary: appended=%d skipped=%d (len=%d)", idx, appendCount, skipCount, acc.length());
  }

  // Try to extract a full line terminated with '\n'
  int nl = acc.indexOf('\n');
  if (nl < 0) {
    LOGT("HW: acc[%d] no newline yet (len %d)", idx, acc.length());
    return 0;   // no complete frame yet
  }

  // Grab the first line and trim CR/LF/space
  String line = acc.substring(0, nl);
  acc.remove(0, nl + 1);
  line.trim();
  LOGT("HW: extracted line from acc[%d] => '%s' (len %d), acc now len %d", idx, line.c_str(), line.length(), acc.length());

  // Only accept our compact protocol frames
  if (!line.startsWith("f/")) {
    LOGT("HW: ignoring non-protocol line from 0x%02X: '%s'", addr, line.c_str());
    return 0;
  }

  // Return the line to caller
  const int n = (int)min(line.length(), outMax);
  memcpy(out, line.c_str(), n);
  // Optional: visibility while bringing up
  LOGT("HW  0x%02X -> %.*s", addr, n, (const char*)out);
  dumpHex_(out, n);
  return n;
}
#endif

