# Mr. Sonnet

An interactive web + physical computing project. The browser generates a short poem through a local Node.js proxy to the Anthropic API, speaks it with the Web Speech API, and drives a cardboard robot (Mr. Sonnet) over **Web Serial** while the poem is recited.

The physical controller is an **Arduino Uno** with two SG90-style servos (mouth and neck).

## Requirements

- [Node.js](https://nodejs.org/) (v18+ recommended; uses `--env-file` for `.env`)
- Chrome or Edge (Web Serial API)
- Arduino Uno flashed with `arduino/mr-sonnet-arduino.ino`
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

- Open `arduino/mr-sonnet-arduino.ino`
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

- **Mouth (D9)** opens and closes continuously.
- **Neck (D10)** moves subtly left and right.
- Higher values increase speed and range.

The browser maps poem length to `S1`–`S9` during speech and sends `S0` when speech ends or you press **Stop**.

## Project layout

```
index.html              Web UI (poem, speech, Web Serial)
server.js               Express static server + /api/poem proxy
package.json
.env                    API key (not committed — add your own)
mr-sonnet-arduino/
  mr-sonnet-arduino.ino  Uno firmware
```
