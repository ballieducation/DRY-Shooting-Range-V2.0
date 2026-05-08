DIY Laser Dry Fire Shooting Range with Wireless Score Tracking
A compact, portable laser shooting range built with two Arduino Nanos, NRF24L01 wireless modules, and TM1637 displays — tracks shots fired, hits on target, and accuracy in real time.
________________________________________
The Pitch
Most commercial dry fire training systems cost hundreds of dollars and lock you into proprietary apps. This project builds a complete laser shooting range from scratch using off-the-shelf Arduino components for under ₹800 (~$10 USD). Point the laser gun at the LDR target, pull the trigger, and watch both units instantly update — shots fired, hits registered, and live accuracy percentage — all communicated wirelessly with zero wires between gun and target.
Perfect for:
•	Shooting sports practice at home
•	Reaction time and accuracy training
•	STEM/electronics learning project
•	School or college project showcase
________________________________________
Project Overview
The system consists of two independent Arduino Nano units:
Laser Gun Unit — A handheld controller with a laser diode, fire button, reset button, and a 4-digit TM1637 display. Every trigger press fires the KY-008 laser for 80ms and increments the shot counter. A red LED on the front confirms wireless link status and flashes on every shot.
Target Unit — A standalone box with an LDR (light-dependent resistor) sensor, buzzer, green status LED, and its own TM1637 display showing the hit count. When the laser beam hits the LDR for more than 50ms, a valid hit is registered, the buzzer beeps, and the hit count is instantly transmitted to the gun via NRF24L01 radio.
Both units talk over NRF24L01 2.4GHz radio with hardware auto-acknowledgement and auto-retry — making the wireless link rock-solid compared to bare 433MHz ASK modules.
________________________________________
How It Works
Shot Detection (Gun Side)
When the fire button is pressed (debounced, 30ms), the Arduino pulls the KY-008 laser HIGH for 80ms, triggers a short buzzer beep, and increments the internal fireCount variable (capped at 99). The TM1637 briefly shows FIRE then returns to the rotating stats display.
Hit Detection (Target Side)
The LDR is connected to D8 with an external 10kΩ pull-up resistor to 5V. When the laser strikes the LDR, the pin goes LOW. A non-blocking pulse-width filter measures how long the pin stays LOW:
•	Under 50ms → noise, ignored
•	50ms to 2000ms → valid hit
•	Over 2000ms → ambient light, ignored
On a valid hit, hitCount increments, the buzzer beeps 60ms, the status LED flashes, and the display updates immediately.
Wireless Communication (NRF24L01)
The target transmits a 2-byte packet to the gun every time a hit is registered:
byte[0] = 0x01  (MSG_HIT)
byte[1] = hitCount
The target also sends a heartbeat packet every 1.5 seconds so the gun knows the link is alive. The gun shows LinE on its display and the red LED turns on solid when the link is established. If no packet is received for 3 seconds, the gun shows - - - - dashes and the LED turns off.
The gun can send a reset command (0xFF) to the target when the reset button is pressed, zeroing both units simultaneously.
Display Rotation (Gun Side)
The gun display rotates every 2 seconds through three screens:
•	Fr XX — Shots fired
•	Ht XX — Hits received
•	AC XX — Accuracy percentage (hits × 100 / shots)
The colon blinks every 500ms on all screens as a system heartbeat indicator.
Self-Test (Target Side)
On power-up the target runs a self-test sequence:
•	1111 → startup
•	8888 → all segments check
•	LED flash → status LED test
•	Buzzer beep × 2 → buzzer test
•	6666 → reset button check (5 second window)
•	7777 → LDR laser check (7 second window)
•	8888 + 3 beeps + LED blink → PASS
________________________________________
Component List
Laser Gun Unit
Component	Specification	Quantity
Arduino Nano	Rev 3.0 (ATmega328P)	1
NRF24L01+ Breakout Board	2.4GHz, SPI, with PA+LNA recommended	1
TM1637 4-Digit Display	0.36" or 0.56"	1
KY-008 Laser Diode Module	650nm Red, 5V	1
Tactile Push Button	6×6mm	2
Active Buzzer	5V	1
Red LED	633nm, 5mm	1
Resistor	220Ω ±5%	1
9V Battery + Connector	High current recommended	1
Capacitor	10µF electrolytic (NRF24 decoupling)	1
Enclosure / Handle	3D printed or project box	1
Target Unit
Component	Specification	Quantity
Arduino Nano	Rev 3.0 (ATmega328P)	1
NRF24L01+ Breakout Board	2.4GHz, SPI	1
TM1637 4-Digit Display	0.36" or 0.56"	1
LDR (Light Dependent Resistor) 20mm 	GL5528 or equivalent	1
Active Buzzer	5V	1
Green LED	560nm, 5mm	1
Red LED	633nm, 5mm (power indicator)	1
Resistor	220Ω ±5%	2
Resistor	10kΩ ±5% (LDR pull-up)	1
ON/OFF Slide Switch	SPST	1
Capacitor	10µF electrolytic (NRF24 decoupling)	1
External 5V DC Supply	USB power bank or 5V adapter	1
Enclosure	3D printed or project box	1
Tools and Consumables
Item	Notes
Arduino IDE	Version 2.x recommended
Soldering iron + solder	For permanent build
Breadboard + jumper wires	For prototyping
Multimeter	For debugging
Hot glue gun	For mounting components
________________________________________
Libraries Required
Install all three from Arduino IDE → Tools → Manage Libraries:
Library	Author	Purpose
RF24	TMRh20	NRF24L01 wireless communication
TM1637Display	Avishay Orpaz	4-digit 7-segment display driver
SPI	Arduino (built-in)	SPI bus for NRF24L01
________________________________________
Pin Connections
Gun Unit (Arduino Nano)
Arduino Pin	Connected To
D2	TM1637 DIO
D3	TM1637 CLK
D5	Buzzer (+)
D6	Fire Button (other end to GND)
D7	Reset Button (other end to GND)
D8	KY-008 Laser S pin
D9	NRF24L01 CE
D10	NRF24L01 CSN
D11	NRF24L01 MOSI
D12	NRF24L01 MISO
D13	NRF24L01 SCK
A1	Red LED → 220Ω → GND
3.3V	NRF24L01 VCC
GND	Common ground
VIN	9V battery (+)
Target Unit (Arduino Nano)
Arduino Pin	Connected To
D2	TM1637 CLK
D3	TM1637 DIO
D4	Buzzer (+)
D5	Green LED → 220Ω → GND
D8	LDR (other end: 10kΩ to 5V)
D7	Reset Button (other end to GND)
D9	NRF24L01 CE
D10	NRF24L01 CSN
D11	NRF24L01 MOSI
D12	NRF24L01 MISO
D13	NRF24L01 SCK
3.3V	NRF24L01 VCC
GND	Common ground
5V (via switch)	Power rail, Red LED indicator
Critical: NRF24L01 must be powered from 3.3V, not 5V. Add a 10µF capacitor across the VCC and GND pins of the NRF24L01 on both units to prevent voltage drop during transmission.
________________________________________
RF Communication Protocol
Both units use channel 108 (2.508 GHz) at 250 kbps with hardware auto-ACK and up to 15 retries per packet. The packet structure is 2 bytes:
Byte	Value	Meaning
[0]	0x01	MSG_HIT — target sends updated hit count
[0]	0x02	MSG_HEARTBEAT — target is alive
[0]	0xFF	MSG_RESET — reset both units
[1]	0–99	hitCount value (or 0 for heartbeat/reset)
The two pipe addresses are:
•	Gun listens on: "GUNIT"
•	Target listens on: "TGTXX"
________________________________________
Build Tips
1.	NRF24L01 power — The most common cause of wireless failure is voltage instability on the NRF24L01. Always add a 10µF capacitor directly across its VCC and GND pins. The PA+LNA version with external antenna gives significantly better range and reliability.
2.	LDR placement — Mount the LDR in a small tube or collar (a piece of black heat-shrink works perfectly) to shield it from ambient light from the sides. This prevents false triggers from room lighting.
3.	Laser alignment — The KY-008 emits a visible red dot. At distances over 3m, consider a collimating lens to keep the dot tight enough to reliably hit the 5mm LDR sensor.
4.	Debouncing — Both buttons use a 30ms software debounce. If you experience double-firing, increase the debounce delay to 50ms in the code.
5.	Battery life — A 9V alkaline battery runs the gun unit for approximately 4–6 hours depending on laser usage. A USB power bank on the target gives 8+ hours.
6.	Range — Tested reliably at 10m indoors with standard NRF24L01 modules. With the PA+LNA version, range extends to 50m+ in open environments.
________________________________________
Possible Upgrades
•	Multiple targets — Add a second target with a different NRF24 pipe address for multi-target drills
•	Timed rounds — Add a countdown timer mode for timed accuracy challenges
•	OLED display — Swap TM1637 for a 128×64 OLED for richer stats (split time, shots per minute)
•	Mobile app — Add an ESP8266 to the gun to push stats to a phone via Wi-Fi
•	Sound effects — Add a DFPlayer Mini module for realistic gunshot and ricochet audio
•	IR instead of visible laser — Replace KY-008 with an IR LED and TSOP receiver for invisible beam operation
________________________________________
Source Code
The full source code for both units is available on GitHub (link your repo here).
•	LASER_GUN_NRF24.ino — Gun unit firmware
•	LASER_TARGET_NRF24.ino — Target unit firmware
Both sketches are fully commented and use non-blocking millis() timing throughout — no delay() in the main loop except for button debounce.
________________________________________
