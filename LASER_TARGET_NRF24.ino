// ============================================================
//  LASER TAG – TARGET UNIT  (NRF24L01 wireless)
//  Libraries : TM1637Display, RF24 by TMRh20
//
//  PIN MAP
//  ┌─────────────┬──────────┐
//  │ TM1637 CLK  │  D2      │
//  │ TM1637 DIO  │  D3      │
//  │ BUZZER      │  D4      │
//  │ STATUS_LED  │  D5      │
//  │ LDR_PIN     │  D6      │  INPUT_PULLUP, LOW = laser hit
//  │ RESET_BTN   │  D7      │  INPUT_PULLUP
//  │ NRF24 CE    │  D9      │
//  │ NRF24 CSN   │  D10     │
//  │ NRF24 MOSI  │  D11     │  (SPI)
//  │ NRF24 MISO  │  D12     │  (SPI)
//  │ NRF24 SCK   │  D13     │  (SPI)
//  │ NRF24 VCC   │  3.3V    │  ← NOT 5V !
//  │ NRF24 GND   │  GND     │
//  └─────────────┴──────────┘
//
//  Add 10µF capacitor across NRF24 VCC and GND pins.
//  Install library: RF24 by TMRh20 (Library Manager)
// ============================================================

#include <TM1637Display.h>
#include <SPI.h>
#include <RF24.h>

// ──── PIN DEFINES ────────────────────────────────────────────
#define CLK        2
#define DIO        3
#define BUZZER     4
#define STATUS_LED 5
#define LDR_PIN    6
#define RESET_BTN  7
#define NRF_CE     9
#define NRF_CSN    10

// ──── NRF24 SETUP ────────────────────────────────────────────
// Must match gun sketch exactly
const byte PIPE_TARGET[6] = "TGTXX";   // target receives on this
const byte PIPE_GUN[6]    = "GUNIT";   // target sends    on this

RF24 radio(NRF_CE, NRF_CSN);

// ──── RF PACKET PROTOCOL ─────────────────────────────────────
#define MSG_HIT       0x01
#define MSG_HEARTBEAT 0x02
#define MSG_RESET     0xFF

struct Packet {
  byte msgType;
  byte value;
};

// ──── TIMING (ms) ────────────────────────────────────────────
#define LDR_MIN_PULSE_MS   50
#define LDR_MAX_PULSE_MS 2000
#define HIT_LOCKOUT_MS    300
#define HEARTBEAT_MS     1500   // send heartbeat to gun every 1.5 s
#define STATUS_BLINK_MS  2000

// ──── OBJECTS ────────────────────────────────────────────────
TM1637Display display(CLK, DIO);

// ──── STATE ──────────────────────────────────────────────────
byte hitCount = 0;

bool          lastResetState = HIGH;

bool          ldrPulseActive = false;
unsigned long ldrPulseStart  = 0;

bool          inLockout    = false;
unsigned long lockoutStart = 0;

unsigned long lastHeartbeat = 0;
unsigned long lastStatusBlink = 0;
bool          ledState = false;

// ──── HELPERS ────────────────────────────────────────────────
void beep(int times, int onMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER, HIGH); delay(onMs);
    digitalWrite(BUZZER, LOW);  delay(120);
  }
}

void showHits() {
  display.showNumberDec(hitCount, true);  // leading zeros
}

// ──── NRF24 SEND TO GUN ──────────────────────────────────────
bool rfSend(byte msgType, byte value) {
  radio.stopListening();

  Packet pkt = { msgType, value };
  bool ok = radio.write(&pkt, sizeof(pkt));

  radio.startListening();

  Serial.print("NRF24 TX [");
  Serial.print(msgType == MSG_HIT ? "HIT" :
               msgType == MSG_HEARTBEAT ? "HB" : "RESET");
  Serial.print("] val=");
  Serial.print(value);
  Serial.println(ok ? " OK" : " FAILED");

  return ok;
}

// ──── SELF TEST ──────────────────────────────────────────────
void selfTest() {
  // 1 – startup
  display.showNumberDec(1111); beep(1, 200); delay(600);

  // 2 – all segments
  display.showNumberDec(8888); delay(1200); display.clear();

  // 3 – STATUS_LED
  digitalWrite(STATUS_LED, HIGH); delay(600);
  digitalWrite(STATUS_LED, LOW);

  // 4 – buzzer
  beep(2, 120);

  // 5 – NRF24 indicator (no actual TX to avoid false hit on gun)
  display.showNumberDec(5555); delay(800);

  // 6 – RESET_BTN (5 s window, non-blocking)
  display.showNumberDec(6666);
  Serial.println("SELF TEST: press RESET_BTN within 5s...");
  {
    unsigned long t = millis();
    while (millis() - t < 5000) {
      if (digitalRead(RESET_BTN) == LOW) break;
    }
    while (digitalRead(RESET_BTN) == LOW);
  }
  beep(1, 100); delay(300);

  // 7 – LDR (7 s window, non-blocking)
  display.showNumberDec(7777);
  Serial.println("SELF TEST: shine laser at LDR within 7s...");
  {
    unsigned long t = millis();
    bool ok = false;
    while (millis() - t < 7000) {
      if (digitalRead(LDR_PIN) == LOW) { ok = true; break; }
    }
    Serial.println(ok ? "SELF TEST: LDR OK" : "SELF TEST: LDR timeout (OK to continue)");
  }
  beep(1, 100);

  // Pass animation
  display.showNumberDec(8888);
  beep(3, 100);
  for (int i = 0; i < 6; i++) {
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    delay(200);
  }
  digitalWrite(STATUS_LED, LOW);
  delay(400);
}

// ──── SETUP ──────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Serial.println("=== LASER TARGET BOOTING (NRF24) ===");

  pinMode(RESET_BTN,  INPUT_PULLUP);
  pinMode(LDR_PIN,    INPUT_PULLUP);
  pinMode(BUZZER,     OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  digitalWrite(BUZZER,     LOW);
  digitalWrite(STATUS_LED, LOW);

  display.setBrightness(7);

  // ── NRF24 init ──────────────────────────────────────────────
  if (!radio.begin()) {
    Serial.println("ERROR: NRF24L01 not found! Check wiring + 3.3V.");
    display.showNumberDec(9999);
    while (true) {
      digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
      delay(200);
    }
  }

  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.setRetries(5, 15);
  radio.setChannel(108);              // must match gun
  radio.setPayloadSize(sizeof(Packet));

  // Target listens on PIPE_TARGET, writes to PIPE_GUN
  radio.openReadingPipe(1, PIPE_TARGET);
  radio.openWritingPipe(PIPE_GUN);
  radio.startListening();

  Serial.println("NRF24L01 OK");

  selfTest();

  hitCount = 0;
  showHits();
  Serial.println("Target READY – waiting for hits");

  // Send first heartbeat immediately so gun establishes link fast
  rfSend(MSG_HEARTBEAT, 0);
  lastHeartbeat = millis();
}

// ──── MAIN LOOP ──────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── 1. Lockout timer ─────────────────────────────────────────
  if (inLockout && now - lockoutStart >= HIT_LOCKOUT_MS) {
    inLockout = false;
  }

  // ── 2. Heartbeat to gun (keeps link alive on gun side) ───────
  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = now;
    rfSend(MSG_HEARTBEAT, 0);
  }

  // ── 3. LDR hit detection ─────────────────────────────────────
  if (!inLockout) {
    bool ldrNow = (digitalRead(LDR_PIN) == LOW);

    // Laser just arrived
    if (ldrNow && !ldrPulseActive) {
      ldrPulseActive = true;
      ldrPulseStart  = now;
      Serial.println("LDR: laser ON, timing...");
    }

    if (ldrPulseActive) {
      unsigned long pw = now - ldrPulseStart;

      if (!ldrNow) {
        // Laser gone – evaluate pulse width
        ldrPulseActive = false;

        Serial.print("LDR: pulse = ");
        Serial.print(pw);
        Serial.println(" ms");

        if (pw >= LDR_MIN_PULSE_MS && pw <= LDR_MAX_PULSE_MS) {
          // ── VALID HIT ────────────────────────────────────────
          if (hitCount < 99) hitCount++;

          Serial.print("VALID HIT  hitCount=");
          Serial.println(hitCount);

          // Feedback
          digitalWrite(BUZZER,     HIGH); delay(60);
          digitalWrite(BUZZER,     LOW);
          digitalWrite(STATUS_LED, HIGH); delay(80);
          digitalWrite(STATUS_LED, LOW);

          showHits();

          // Tell gun – NRF24 with auto-ACK + retry
          rfSend(MSG_HIT, hitCount);

          inLockout    = true;
          lockoutStart = now;

        } else if (pw > LDR_MAX_PULSE_MS) {
          Serial.println("LDR: too long – ambient light, ignore");
        } else {
          Serial.println("LDR: too short – noise, ignore");
        }

      } else if (pw > LDR_MAX_PULSE_MS) {
        ldrPulseActive = false;
        Serial.println("LDR: stuck LOW – ambient reset");
      }
    }
  }

  // ── 4. Receive from gun (reset command) ──────────────────────
  if (radio.available()) {
    Packet pkt;
    radio.read(&pkt, sizeof(pkt));

    if (pkt.msgType == MSG_RESET) {
      Serial.println("NRF24 RESET received from gun");
      hitCount = 0;
      beep(2, 80);
      showHits();
    }
  }

  // ── 5. RESET button ──────────────────────────────────────────
  bool currentReset = digitalRead(RESET_BTN);

  if (currentReset == LOW && lastResetState == HIGH) {
    delay(50);
    if (digitalRead(RESET_BTN) == LOW) {
      hitCount = 0;
      Serial.println("LOCAL RESET");
      beep(2, 80);
      showHits();

      rfSend(MSG_RESET, 0);    // tell gun to zero too

      while (digitalRead(RESET_BTN) == LOW);
      delay(50);
    }
  }
  lastResetState = currentReset;

  // ── 6. STATUS_LED heartbeat blink ────────────────────────────
  if (now - lastStatusBlink >= STATUS_BLINK_MS) {
    lastStatusBlink = now;
    ledState = !ledState;
    digitalWrite(STATUS_LED, ledState);
  }
}
