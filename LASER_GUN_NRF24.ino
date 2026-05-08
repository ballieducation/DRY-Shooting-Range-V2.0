// ============================================================
//  LASER TAG – GUN UNIT  (NRF24L01 wireless)
//  Libraries : TM1637Display, RF24 by TMRh20
//
//  PIN MAP
//  ┌─────────────┬──────────┐
//  │ TM1637 DIO  │  D2      │
//  │ TM1637 CLK  │  D3      │
//  │ BUZZER      │  D5      │
//  │ FIRE_BTN    │  D6      │  INPUT_PULLUP
//  │ RESET_BTN   │  D7      │  INPUT_PULLUP
//  │ LASER       │  D8      │
//  │ NRF24 CE    │  D9      │
//  │ NRF24 CSN   │  D10     │
//  │ NRF24 MOSI  │  D11     │  (SPI – do not change)
//  │ NRF24 MISO  │  D12     │  (SPI – do not change)
//  │ NRF24 SCK   │  D13     │  (SPI – do not change)
//  │ NRF24 VCC   │  3.3V    │  ← NOT 5V !
//  │ NRF24 GND   │  GND     │
//  │ LED_LINK    │  A1      │  RED LED (link + fire indicator)
//  └─────────────┴──────────┘
//
//  LED_LINK behaviour
//    Steady ON  = wireless link alive
//    OFF        = no link / waiting
//    Flash 80ms = laser just fired
//
//  Install library: RF24 by TMRh20 (Library Manager)
// ============================================================

#include <TM1637Display.h>
#include <SPI.h>
#include <RF24.h>

// ──── PIN DEFINES ────────────────────────────────────────────
#define DIO        2
#define CLK        3
#define BUZZER     5
#define FIRE_BTN   6
#define RESET_BTN  7
#define LASER      8
#define NRF_CE     9
#define NRF_CSN    10
#define LED_LINK   A1   // RED LED

// ──── NRF24 SETUP ────────────────────────────────────────────
// Two pipes – gun listens on PIPE_GUN, target listens on PIPE_TARGET
// Both addresses must match exactly in target sketch
const byte PIPE_GUN[6]    = "GUNIT";   // gun  receives on this
const byte PIPE_TARGET[6] = "TGTXX";   // gun  sends    on this

RF24 radio(NRF_CE, NRF_CSN);

// ──── RF PACKET PROTOCOL ─────────────────────────────────────
#define MSG_HIT       0x01   // target → gun : hit update
#define MSG_HEARTBEAT 0x02   // target → gun : i am alive
#define MSG_RESET     0xFF   // gun → target : reset command

struct Packet {
  byte msgType;
  byte value;      // hitCount for MSG_HIT, 0 otherwise
};

// ──── TIMING (ms) ────────────────────────────────────────────
#define LASER_ON_MS        80
#define EVENT_MSG_MS      600
#define LINK_MSG_MS      1500   // "LinE" shown for longer
#define COLON_BLINK_MS    500
#define ROTATE_MS        2000
#define LINK_TIMEOUT_MS  3000
#define LED_FLASH_MS       80

// ──── OBJECTS ────────────────────────────────────────────────
TM1637Display display(CLK, DIO);

// ──── COUNTERS ───────────────────────────────────────────────
byte fireCount = 0;
byte hitCount  = 0;

// ──── BUTTON STATES ──────────────────────────────────────────
bool lastFireState  = HIGH;
bool lastResetState = HIGH;

// ──── COLON BLINK ────────────────────────────────────────────
bool          colonState    = false;
unsigned long lastColonBlink = 0;

// ──── DISPLAY ROTATION ───────────────────────────────────────
byte          displayMode = 0;        // 0=Fr 1=Ht 2=Ac
unsigned long lastRotate  = 0;

// ──── EVENT MESSAGE OVERLAY ──────────────────────────────────
bool          eventMessage  = false;
unsigned long eventStart    = 0;
unsigned long eventDuration = EVENT_MSG_MS;

// ──── OUTPUT PULSE (laser + buzzer) ──────────────────────────
bool          outputsActive = false;
unsigned long outputStart   = 0;

// ──── LINK LED ───────────────────────────────────────────────
bool          ledFlashActive = false;
unsigned long ledFlashStart  = 0;
bool          linkAlive      = false;
bool          linkWasAlive   = false;
unsigned long lastPacketSeen = 0;

// ──── SEGMENT DEFINITIONS ────────────────────────────────────
const uint8_t FRSEG[2] = {
  SEG_A|SEG_E|SEG_F|SEG_G,               // F
  SEG_E|SEG_G                             // r
};
const uint8_t HTSEG[2] = {
  SEG_B|SEG_C|SEG_E|SEG_F|SEG_G,         // H
  SEG_D|SEG_E|SEG_F|SEG_G                // t
};
const uint8_t ACSEG[2] = {
  SEG_A|SEG_B|SEG_C|SEG_E|SEG_F|SEG_G,  // A
  SEG_D|SEG_E|SEG_G                       // c
};
const uint8_t FIREMSG[4] = {
  SEG_A|SEG_E|SEG_F|SEG_G,               // F
  SEG_E|SEG_F,                            // I
  SEG_A|SEG_B|SEG_E|SEG_F|SEG_G,         // r (looks like P)
  SEG_A|SEG_D|SEG_E|SEG_F|SEG_G          // E
};
const uint8_t HITMSG[4] = {
  SEG_B|SEG_C|SEG_E|SEG_F|SEG_G,         // H
  SEG_E|SEG_F,                            // I
  SEG_D|SEG_E|SEG_F|SEG_G,               // t
  0
};
const uint8_t RSMSG[4] = {
  SEG_E|SEG_G,                            // r
  SEG_A|SEG_C|SEG_D|SEG_F|SEG_G,         // S
  0, 0
};
// "LinE" – wireless link established confirmation
const uint8_t LINKMSG[4] = {
  SEG_D|SEG_E|SEG_F,                      // L
  SEG_E|SEG_F,                            // I
  SEG_C|SEG_E|SEG_G,                      // n
  SEG_A|SEG_D|SEG_E|SEG_F|SEG_G          // E
};
// "- - - -" waiting for link
const uint8_t WAITDASH[4] = {
  SEG_G, SEG_G, SEG_G, SEG_G
};

// ──── DISPLAY HELPERS ────────────────────────────────────────
void showPrefixed(const uint8_t* prefix, byte value) {
  uint8_t data[4];
  data[0] = prefix[0];
  data[1] = prefix[1];
  data[2] = display.encodeDigit(value / 10);
  data[3] = display.encodeDigit(value % 10);
  if (colonState) data[1] |= 0x80;
  display.setSegments(data);
}

void updateDisplay() {
  if (eventMessage) return;
  if (!linkAlive) {
    display.setSegments(WAITDASH);   // "- - - -" until link up
    return;
  }
  if (displayMode == 0) showPrefixed(FRSEG, fireCount);
  if (displayMode == 1) showPrefixed(HTSEG, hitCount);
  if (displayMode == 2) {
    byte acc = (fireCount > 0) ? (hitCount * 100) / fireCount : 0;
    if (acc > 99) acc = 99;
    showPrefixed(ACSEG, acc);
  }
}

void showEvent(const uint8_t* msg4, unsigned long dur = EVENT_MSG_MS) {
  eventMessage  = true;
  eventStart    = millis();
  eventDuration = dur;
  display.setSegments(msg4);
}

// ──── DEBUG ──────────────────────────────────────────────────
void debugCounts() {
  Serial.print("Fr="); Serial.print(fireCount);
  Serial.print(" Ht="); Serial.print(hitCount);
  Serial.print(" Ac=");
  Serial.print(fireCount > 0 ? (hitCount * 100) / fireCount : 0);
  Serial.println("%");
}

// ──── NRF24 SEND TO TARGET ───────────────────────────────────
bool rfSend(byte msgType, byte value) {
  radio.stopListening();                   // switch to TX mode

  Packet pkt = { msgType, value };
  bool ok = radio.write(&pkt, sizeof(pkt));

  radio.startListening();                  // back to RX mode

  if (ok)
    Serial.println("NRF24 send OK");
  else
    Serial.println("NRF24 send FAILED (no ACK)");

  return ok;
}

// ──── SETUP ──────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Serial.println("=== LASER GUN BOOTING (NRF24) ===");

  // Outputs
  pinMode(BUZZER,    OUTPUT);
  pinMode(LASER,     OUTPUT);
  pinMode(LED_LINK,  OUTPUT);
  digitalWrite(BUZZER,   LOW);
  digitalWrite(LASER,    LOW);
  digitalWrite(LED_LINK, LOW);

  // Inputs
  pinMode(FIRE_BTN,  INPUT_PULLUP);
  pinMode(RESET_BTN, INPUT_PULLUP);

  display.setBrightness(7);
  display.setSegments(WAITDASH);    // show dashes immediately

  // ── NRF24 init ──────────────────────────────────────────────
  if (!radio.begin()) {
    Serial.println("ERROR: NRF24L01 not found! Check wiring + 3.3V power.");
    // Flash LED rapidly to signal hardware fault
    while (true) {
      digitalWrite(LED_LINK, !digitalRead(LED_LINK));
      delay(150);
    }
  }

  radio.setPALevel(RF24_PA_HIGH);     // HIGH power for reliability
  radio.setDataRate(RF24_250KBPS);    // slowest = best range + reliability
  radio.setRetries(5, 15);            // 5×250µs delay, up to 15 retries
  radio.setChannel(108);              // channel 108 = 2.508 GHz, avoids Wi-Fi
  radio.setPayloadSize(sizeof(Packet));

  // Gun listens on PIPE_GUN, writes to PIPE_TARGET
  radio.openReadingPipe(1, PIPE_GUN);
  radio.openWritingPipe(PIPE_TARGET);
  radio.startListening();

  Serial.println("NRF24L01 OK");
  Serial.println("Waiting for wireless link from target...");
}

// ──── MAIN LOOP ──────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── 1. Colon blink ──────────────────────────────────────────
  if (now - lastColonBlink >= COLON_BLINK_MS) {
    lastColonBlink = now;
    colonState = !colonState;
    updateDisplay();
  }

  // ── 2. Turn off laser + buzzer after pulse ───────────────────
  if (outputsActive && now - outputStart >= LASER_ON_MS) {
    digitalWrite(LASER,  LOW);
    digitalWrite(BUZZER, LOW);
    outputsActive = false;
  }

  // ── 3. LED flash timeout (fire indicator) ───────────────────
  // After flash, return LED to link-status state
  if (ledFlashActive && now - ledFlashStart >= LED_FLASH_MS) {
    ledFlashActive = false;
    digitalWrite(LED_LINK, linkAlive ? HIGH : LOW);
  }

  // ── 4. Clear event message ───────────────────────────────────
  if (eventMessage && now - eventStart >= eventDuration) {
    eventMessage = false;
    updateDisplay();
  }

  // ── 5. Display rotation ──────────────────────────────────────
  if (!eventMessage && linkAlive && now - lastRotate >= ROTATE_MS) {
    lastRotate = now;
    displayMode = (displayMode + 1) % 3;
    updateDisplay();
  }

  // ── 6. Link watchdog ─────────────────────────────────────────
  if (linkAlive && now - lastPacketSeen > LINK_TIMEOUT_MS) {
    linkAlive = false;
    digitalWrite(LED_LINK, LOW);     // LED OFF = link lost
    Serial.println("LINK LOST");
    display.setSegments(WAITDASH);   // back to dashes
  }

  // Edge: link just established
  if (linkAlive && !linkWasAlive) {
    Serial.println("WIRELESS LINK ESTABLISHED");
    digitalWrite(LED_LINK, HIGH);    // LED ON solid = link alive
    showEvent(LINKMSG, LINK_MSG_MS); // show "LinE" for 1.5 s
    // Confirmation beep – two short
    digitalWrite(BUZZER, HIGH); delay(60);
    digitalWrite(BUZZER, LOW);  delay(80);
    digitalWrite(BUZZER, HIGH); delay(60);
    digitalWrite(BUZZER, LOW);
  }
  linkWasAlive = linkAlive;

  // ── 7. Receive packet from target ───────────────────────────
  if (radio.available()) {
    Packet pkt;
    radio.read(&pkt, sizeof(pkt));

    lastPacketSeen = now;
    if (!linkAlive) linkAlive = true;   // first packet = link up

    if (pkt.msgType == MSG_HIT) {
      hitCount = pkt.value;
      Serial.print("NRF24 HIT UPDATE  hitCount=");
      Serial.println(hitCount);
      debugCounts();
      showEvent(HITMSG);

    } else if (pkt.msgType == MSG_HEARTBEAT) {
      // Just keeps link alive – no display change needed
      Serial.println("NRF24 heartbeat from target");

    } else if (pkt.msgType == MSG_RESET) {
      fireCount = 0;
      hitCount  = 0;
      Serial.println("NRF24 RESET from target");
      debugCounts();
      showEvent(RSMSG);
    }
  }

  // ── 8. FIRE button ───────────────────────────────────────────
  bool currentFire = digitalRead(FIRE_BTN);

  if (currentFire == LOW && lastFireState == HIGH) {
    delay(30);
    if (digitalRead(FIRE_BTN) == LOW) {

      if (fireCount < 99) fireCount++;
      else                fireCount = 0;

      Serial.print("SHOT FIRED  ");
      debugCounts();

      // Laser + buzzer pulse
      digitalWrite(LASER,  HIGH);
      digitalWrite(BUZZER, HIGH);
      outputsActive = true;
      outputStart   = now;

      // LED flash (overrides steady-ON briefly)
      digitalWrite(LED_LINK, HIGH);
      ledFlashActive = true;
      ledFlashStart  = now;

      showEvent(FIREMSG);
    }
  }
  lastFireState = currentFire;

  // ── 9. RESET button ──────────────────────────────────────────
  bool currentReset = digitalRead(RESET_BTN);

  if (currentReset == LOW && lastResetState == HIGH) {
    delay(30);
    if (digitalRead(RESET_BTN) == LOW) {
      fireCount = 0;
      hitCount  = 0;
      Serial.println("LOCAL RESET");
      debugCounts();

      rfSend(MSG_RESET, 0);    // tell target to reset too

      showEvent(RSMSG);
      updateDisplay();
    }
  }
  lastResetState = currentReset;
}
