# Mr. Sonnet

An interactive web + physical computing project. The browser generates a short poem through a local Node.js proxy to the Anthropic API, speaks it with the Web Speech API, and drives a cardboard robot (Mr. Sonnet) over **Web Serial** while the poem is recited.

The physical controller is an **Arduino Uno** with two SG90-style servos (mouth and neck).

## Requirements

- [Node.js](https://nodejs.org/) (v18+ recommended; uses `--env-file` for `.env`)
- Chrome or Edge (Web Serial API)
- Arduino Uno flashed with `mr-sonnet-arduino/mr-sonnet-arduino.ino`
- Anthropic API key in `.env`

## Setup

1. Clone or copy this project folder.

2. Create `.env` in the project root:

   ```
   ANTHROPIC_API_KEY=your_key_here
   ```

3. Install dependencies and start the server:

   ```bash
   npm install
   npm start
   ```

4. Open the app:

   ```
   http://localhost:3000/
   ```

## Arduino wiring

| Servo wire | Connection |
|------------|------------|
| Mouth signal (orange/yellow) | Arduino **D9** |
| Neck signal (orange/yellow) | Arduino **D10** |
| Servo VCC (red) | Arduino **5V** (OK for initial tests) |
| Servo GND (brown/black) | Arduino **GND** |

**Power note:** Two servos on Arduino 5V is fine for bench testing. For more reliable motion, power the servos from an **external 5V supply** and connect **external GND to Arduino GND**. Keep signal wires on D9 and D10.

Flash the sketch with the Arduino IDE (or `arduino-cli`):

- Open `mr-sonnet-arduino/mr-sonnet-arduino.ino`
- Board: Arduino Uno
- Install the **Servo** library if prompted (built-in on most installs)
- Upload via USB

Close the Arduino IDE Serial Monitor before connecting from the browser — only one program can use the port at a time.

## Web Serial connection

1. Run `npm start` and open `http://localhost:3000/` in **Chrome** or **Edge**.
2. Click **Connect** in the serial bar.
3. Select the **Arduino Uno** USB serial port.
4. Generate a poem. After the typewriter finishes, the browser speaks the poem and sends motion commands to the Arduino.

## Serial protocol

Commands are sent at **115200 baud**, one line per command:

| Command | Meaning |
|---------|---------|
| `S0` | Stop — both servos return to neutral (~90°) |
| `S1` | Slowest, smallest motion |
| `S9` | Fastest, most expressive motion |

Format: `S` + digit + newline, e.g. `S5\n`.

While `S1`–`S9` is active:

- **Mouth (D9)** flaps open and closed (cardboard lid + elastic return).
- **Neck (D10)** moves subtly left and right.
- Higher values increase speed and range.

The browser schedules `S1`–`S9` from the poem text during speech (word rhythm and punctuation pauses) and sends `S0` when speech ends or you press **Stop**.

## Mouth synchronization

Mr. Sonnet’s mouth is driven by **text-driven choreography** in the browser, not by live speech-analysis from the voice engine.

We tested `speechSynthesisUtterance.onboundary` events, but with the current Chrome/Edge voices they did not fire reliably (often zero word events). Because of that, the project uses an **estimated schedule** based on the poem text instead of word-boundary callbacks.

How it works:

- The poem is split into **words**, **punctuation**, and **line breaks**.
- Each word sends an **`S1`–`S9`** command. Longer words usually produce stronger mouth movement.
- **Commas, periods, question marks, exclamation marks, and line breaks** trigger short **`S0`** pauses so the lid can close briefly.
- The schedule starts shortly after speech begins and is scaled to stay roughly aligned with the spoken poem.
- The **final stop is always authoritative** from `speechSynthesis.onend`, `speechSynthesis.onerror`, or the **Stop** button — not from the end of the text schedule.

This is **expressive poetic synchronization** for a cardboard lid mechanism. It is not perfect phoneme lip sync.

## Sync tuning

Synchronization can be tuned in `index.html` near the speech / servo logic. Open the browser console when testing.

| Constant | Purpose |
|----------|---------|
| `SYNC_MOUTH_WITH_TEXT` | `true` = text-driven choreography; `false` = single intensity for the whole poem |
| `SYNC_DEBUG` | `true` = log schedule preview, serial commands, and speech timing in the console |
| `SYNC_MOUTH_DIAGNOSTIC` | `true` = re-test `onboundary` events (diagnostic only; not used for motion) |
| `SPEECH_RATE` | Speech rate for `speechSynthesis` and for choreography timing estimates |
| `TEXT_SYNC_TIME_SCALE` | Multiplier for scheduled durations. Increase if the mouth finishes **before** the voice; decrease if it lags **behind** |
| `TEXT_SYNC_START_DELAY_MS` | Delay before the first mouth command after `synth.speak()`. Increase if the mouth starts **before** the voice |

After a full recitation, check the console log `[Mr. Sonnet sync] speech timing` and compare `actualSpeechDurationMs` with `scaledScheduleDurationMs`. The `ratio` helps guide `TEXT_SYNC_TIME_SCALE` adjustments.

## Troubleshooting

**Browser cannot connect to Arduino**  
Close the Arduino IDE Serial Monitor. Only one program can use the USB serial port at a time. Use **Chrome** or **Edge** (Web Serial is required).

**Servos jitter or Arduino resets**  
Power the servos from an **external 5V supply** and connect **external GND to Arduino GND**. Bench power from the Uno’s 5V pin is fine for quick tests but often insufficient under load.

**Mouth moves in the wrong direction**  
In `mr-sonnet-arduino/mr-sonnet-arduino.ino`, toggle `MOUTH_MIRROR`. Confirm the **closed / `S0` position** is correct before changing open-angle values.

**Mouth finishes before or after the voice**  
Tune `TEXT_SYNC_TIME_SCALE` and `TEXT_SYNC_START_DELAY_MS` in `index.html`. Enable `SYNC_DEBUG` to inspect timing logs in the console.

**Mouth motion feels too small or too strong**  
Tune mouth angle constants in the Arduino sketch: `MOUTH_OPEN_LOW`, `MOUTH_OPEN_MEDIUM`, `MOUTH_OPEN_STRONG`, `MOUTH_ACCENT_MIN`, `MOUTH_ACCENT_MAX`, and `MOUTH_MIN_SAFE` / `MOUTH_MAX_SAFE`. Reduce values if the servo buzzes or hits a mechanical stop.

**No word boundary events in the console**  
Expected with some browser voices. The project does **not** rely on boundary events; it uses text-driven choreography. Set `SYNC_MOUTH_DIAGNOSTIC = true` only if you want to re-test.

**`favicon.ico` 404 in the server log**  
Harmless. The browser is only requesting a site icon that this project does not ship.

## Project layout

```
index.html                        Web UI (poem, speech, Web Serial)
server.js                         Express static server + /api/poem proxy
package.json
.gitignore                        Excludes .env and node_modules/
.env                              API key (local only — create after clone)
mr-sonnet-arduino/
  mr-sonnet-arduino.ino           Arduino Uno firmware
```
