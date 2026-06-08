/*
 * Mr. Sonnet — Arduino Uno firmware
 *
 * Two SG90-style servos driven while the browser recites a poem:
 *   - Mouth servo signal → pin D9  (cardboard lid lift — see mouth comment below)
 *   - Neck servo signal  → pin D10 (slow left/right theatrical sweep)
 *
 * MOUTH MECHANISM (physical):
 *   The mouth is a cardboard box-top / lid lifted by the servo through a
 *   cardboard linkage. An elastic band pulls the lid closed. That load needs
 *   much larger servo travel than a bare jaw in air — we use wide open angles
 *   on purpose so the motion reads clearly on the robot.
 *
 * WIRING (quick test — Arduino 5V):
 *   Servo signal (orange/yellow) → D9 or D10
 *   Servo VCC (red)              → Arduino 5V
 *   Servo GND (brown/black)      → Arduino GND
 *
 * For more reliable motion (especially with two servos), use an external
 * 5V supply for servo VCC. Always connect external GND to Arduino GND.
 * Do not power heavy loads only from the Arduino regulator long-term.
 *
 * SERIAL PROTOCOL (115200 baud, USB serial from Web Serial in Chrome/Edge):
 *   Browser sends a line:  S<digit>\n
 *   S0  → stop; return both servos to neutral (~90°)
 *   S1  → slowest speech-like mouth cycle
 *   S9  → fastest cycle, strongest openings
 *
 * Mouth: non-blocking closed → open → hold → closed cycle (not a slow sweep).
 * Neck: smooth sweep, slower than the mouth.
 */

#include <Servo.h>

// ── Pins
const int MOUTH_PIN = 9;
const int NECK_PIN  = 10;

// ── Angles (degrees) — tuned for cardboard lid + elastic return
// Closed is nearly fixed so the elastic rest position stays consistent.
// Open angles are large so the lid visibly lifts (mechanism needs ~2× travel
// vs a small jaw flap). If the servo buzzes or strains, reduce MOUTH_ACCENT_MAX
// or lower MOUTH_STRONG — do not force past your linkage’s mechanical stop.
const int NEUTRAL           = 90;
const int MOUTH_ANGLE_CLOSED = 89;  // 88–90: elastic holds lid down
const int MOUTH_OPEN_LOW    = 120;  // S1–S3 modest but visible
const int MOUTH_OPEN_MEDIUM = 135;  // S4–S6 clear lift
const int MOUTH_OPEN_STRONG = 150;  // S7–S9 strong lift
const int MOUTH_ACCENT_MIN  = 155;  // occasional expressive peak
const int MOUTH_ACCENT_MAX  = 160;  // safe ceiling — tune down if overdriven

// Logical open values above are “amplitude” targets; the cardboard linkage may move
// the lid the opposite way on the real servo. Mirror around closed when writing opens.
const bool MOUTH_MIRROR = true;
const int MOUTH_MIN_SAFE = 30;   // conservative floor after mirror (avoid mechanical stop)
const int MOUTH_MAX_SAFE = 175;  // conservative ceiling for physical writes

int mouthPhysicalAngle(int logicalAngle) {
  if (!MOUTH_MIRROR) {
    return constrain(logicalAngle, MOUTH_MIN_SAFE, MOUTH_MAX_SAFE);
  }
  int mirrored = MOUTH_ANGLE_CLOSED - (logicalAngle - MOUTH_ANGLE_CLOSED);
  return constrain(mirrored, MOUTH_MIN_SAFE, MOUTH_MAX_SAFE);
}

Servo mouthServo;
Servo neckServo;

int intensity = 0;
String serialBuf = "";

// Mouth state machine: closed → open (write) → hold open → closed …
enum MouthPhase {
  MOUTH_CLOSED,
  MOUTH_HOLD_OPEN
};

MouthPhase mouthPhase = MOUTH_CLOSED;
int mouthTargetOpen   = MOUTH_OPEN_LOW;
unsigned long mouthPhaseUntil = 0;

// Neck sweep state
int neckAngle = NEUTRAL;
int neckDir   = 1;
unsigned long lastNeckMove = 0;

// Phase durations (ms) — hold open longer than closed for visible lid lift
void getMouthDurations(int speed, unsigned long &closedMs, unsigned long &holdMs) {
  float t = (speed - 1) / 8.0f;
  // S1: slower cycle; S9: faster but still readable on cardboard
  closedMs = (unsigned long)(150 - t * 55);   // ~150 ms → ~95 ms
  holdMs   = (unsigned long)(closedMs * 2 + 60);  // ~2× closed + extra dwell
}

// Accent chance rises with intensity (S7+ more frequent)
bool rollAccentOpening(int speed) {
  if (speed <= 3) return false;
  if (speed <= 6) return random(0, 100) < 12;
  if (speed <= 8) return random(0, 100) < 22;
  return random(0, 100) < 32;
}

int mouthOpenAngleForSpeed(int speed, bool accent) {
  int angle;

  if (speed <= 3) {
    angle = MOUTH_OPEN_LOW;
  } else if (speed <= 6) {
    angle = (random(0, 2) == 0) ? MOUTH_OPEN_LOW : MOUTH_OPEN_MEDIUM;
  } else {
    int pick = random(0, 3);
    if (pick == 0) angle = MOUTH_OPEN_MEDIUM;
    else if (pick == 1) angle = MOUTH_OPEN_STRONG;
    else angle = MOUTH_OPEN_STRONG;
  }

  if (accent) {
    angle = random(MOUTH_ACCENT_MIN, MOUTH_ACCENT_MAX + 1);
  }

  return constrain(angle, MOUTH_OPEN_LOW, MOUTH_ACCENT_MAX);
}

void mouthBeginClosed(unsigned long now) {
  mouthPhase = MOUTH_CLOSED;
  mouthServo.write(MOUTH_ANGLE_CLOSED);
  unsigned long closedMs, holdMs;
  getMouthDurations(intensity, closedMs, holdMs);
  mouthPhaseUntil = now + closedMs;
}

void mouthBeginOpen(unsigned long now) {
  bool accent = rollAccentOpening(intensity);
  mouthTargetOpen = mouthOpenAngleForSpeed(intensity, accent);
  mouthServo.write(mouthPhysicalAngle(mouthTargetOpen));
  mouthPhase = MOUTH_HOLD_OPEN;
  unsigned long closedMs, holdMs;
  getMouthDurations(intensity, closedMs, holdMs);
  mouthPhaseUntil = now + holdMs;
}

// Neck: smooth theatrical sweep (slower than mouth)
void getNeckParams(
  int speed,
  int &neckMin, int &neckMax,
  int &neckStep,
  unsigned long &neckInterval
) {
  float t = (speed - 1) / 8.0f;

  neckMin = NEUTRAL - (int)(6 + t * 14);
  neckMax = NEUTRAL + (int)(6 + t * 14);
  neckStep = 1 + (int)(t * 3);
  neckInterval = (unsigned long)(95 - t * 55);
}

void goNeutral() {
  intensity = 0;
  mouthPhase = MOUTH_CLOSED;
  mouthPhaseUntil = 0;
  neckAngle = NEUTRAL;
  neckDir = 1;
  mouthServo.write(MOUTH_ANGLE_CLOSED);
  neckServo.write(NEUTRAL);
}

void processLine(const String &msg) {
  if (msg.length() != 2 || msg.charAt(0) != 'S') return;

  char c = msg.charAt(1);
  if (c < '0' || c > '9') return;

  int spd = c - '0';
  intensity = spd;

  if (spd == 0) {
    goNeutral();
  } else {
    // Restart mouth cycle on new intensity so S changes feel immediate
    mouthPhaseUntil = 0;
  }
}

void readSerial() {
  while (Serial.available() > 0) {
    char ch = Serial.read();

    if (ch == '\n' || ch == '\r') {
      if (serialBuf.length() > 0) {
        serialBuf.trim();
        processLine(serialBuf);
        serialBuf = "";
      }
      continue;
    }

    if (serialBuf.length() < 16) {
      serialBuf += ch;
    } else {
      serialBuf = "";
    }
  }
}

void animateMouth(unsigned long now) {
  if (mouthPhaseUntil == 0) {
    mouthBeginClosed(now);
    return;
  }

  if (now < mouthPhaseUntil) return;

  if (mouthPhase == MOUTH_CLOSED) {
    mouthBeginOpen(now);
  } else {
    mouthBeginClosed(now);
  }
}

void animateNeck(unsigned long now) {
  int neckMin, neckMax, neckStep;
  unsigned long neckInterval;

  getNeckParams(intensity, neckMin, neckMax, neckStep, neckInterval);

  if (now - lastNeckMove < neckInterval) return;
  lastNeckMove = now;

  neckAngle += neckDir * neckStep;
  if (neckAngle >= neckMax) {
    neckAngle = neckMax;
    neckDir = -1;
  } else if (neckAngle <= neckMin) {
    neckAngle = neckMin;
    neckDir = 1;
  }
  neckServo.write(neckAngle);
}

void animateServos() {
  unsigned long now = millis();
  animateMouth(now);
  animateNeck(now);
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  mouthServo.attach(MOUTH_PIN);
  neckServo.attach(NECK_PIN);
  goNeutral();
}

void loop() {
  readSerial();

  if (intensity > 0) {
    animateServos();
  }
}
