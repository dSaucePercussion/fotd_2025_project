#define PIR_MOTION_SENSOR D0
#define SPEAKER_PIN WIO_BUZZER

#include "TFT_eSPI.h"
#include "Seeed_FS.h"
#include "RawImage.h"
#include "pitches.h"

TFT_eSPI tft;

// --- FSM States ---
enum State {
  WAIT_FOR_BUTTON,
  IDLE,
  PLAYING,
  SAURON,
  COOLDOWN,
  SAMWISE
};
State currentState = WAIT_FOR_BUTTON;

// --- Flags and Control ---
bool notePlaying = false;
bool frameNeedsUpdate = false;
bool sauronWasTriggered = false;

// --- Indices and Timing ---
int songNoteIndex = 0;
int currentFrame = 0;
int lastFrameShown = -1;
int frameToShow = -1;
unsigned long songNoteStart = 0;
unsigned long lastFrameRenderTime = 0;
unsigned long sauronStartTime = 0;
unsigned long cooldownStartTime = 0;

// --- Sauron Tracking ---
bool searchingState = false;
unsigned long timeMotionIsDetected = 0;
unsigned long nextCheck = 0;
unsigned long randomDelay = 0;
const unsigned long sauronDuration = 5000;
const unsigned long cooldownDuration = 7000;

// --- Constants ---
const int totalFrames = 4;
int halfNoteCH = 1154, quarterNoteCH = 577, eighthNoteCH = 288;
int sixteenthNoteCH = 144, dotQuarterCH = 865;

int noteDurationsCH[] = {
  sixteenthNoteCH, sixteenthNoteCH, quarterNoteCH, quarterNoteCH,
  quarterNoteCH, sixteenthNoteCH, sixteenthNoteCH, sixteenthNoteCH,
  halfNoteCH, quarterNoteCH, eighthNoteCH, dotQuarterCH,
  eighthNoteCH, dotQuarterCH, eighthNoteCH, dotQuarterCH,
  sixteenthNoteCH, sixteenthNoteCH, quarterNoteCH
};

int melodyCH[] = {
  NOTE_D4, NOTE_E4, NOTE_FS4, NOTE_A4,
  NOTE_FS4, NOTE_E4, NOTE_FS4, NOTE_E4,
  NOTE_D4, NOTE_FS4, NOTE_A4, NOTE_B4,
  NOTE_D5, NOTE_CS5, NOTE_A4, NOTE_FS4,
  NOTE_G4, NOTE_FS4, NOTE_E4
};

int frameSchedule[] = {
  0, -1, -1, -1,
  1, -1, -1, -1,
  2, -1, -1, -1,
  3, -1, -1, -1,
  -1, -1, -1
};

void setup() {
  Serial.begin(115200);
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    Serial.println("SD Card Initialization Failed!");
    while (1);
  }
  tft.begin();
  tft.setRotation(1); // Rotate 180 degrees (3 upside-down -> 1)
  pinMode(PIR_MOTION_SENSOR, INPUT);
  pinMode(13, OUTPUT);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  randomSeed(analogRead(A2));

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(20, 80);
  tft.println("Press the blue circle");
  tft.setCursor(20, 110);
  tft.println("to begin your CTP");
  tft.setCursor(20, 140);
  tft.println("adventure!");
}

void loop() {
  int motionState = digitalRead(PIR_MOTION_SENSOR);
  digitalWrite(13, motionState);
  unsigned long now = millis();

  Serial.println(currentState);

  switch (currentState) {
    case WAIT_FOR_BUTTON:
      if (digitalRead(WIO_5S_PRESS) == LOW) {
        delay(300);
        currentState = IDLE;
        tft.fillScreen(TFT_BLACK);
        Serial.println("System Armed");
      }
      break;

    case IDLE:
      if (motionState) {
        songNoteIndex = 0;
        notePlaying = false;
        currentFrame = 0;
        frameNeedsUpdate = false;
        searchingState = true;
        timeMotionIsDetected = now;
        nextCheck = now + 500;
        randomDelay = random(5000, 10001);
        currentState = PLAYING;
        Serial.println("Motion detected — starting song");
      } else {
        drawImageSafe("shire/theShire00.bmp");
        delay(500);
      }
      break;

    case PLAYING:
      if (searchingState && now > nextCheck) {
        if (!motionState) {
          searchingState = false;
          sauronWasTriggered = false;
        } else if (now - timeMotionIsDetected > randomDelay) {
          drawImageSafe("sauron/sauron0.bmp");
          sauronWasTriggered = true;
          sauronStartTime = now;
          noTone(SPEAKER_PIN);
          notePlaying = false;
          songNoteIndex = 0;
          searchingState = false;
          currentState = SAURON;
          break;
        }
        nextCheck = now + 500;
      }

      if (!notePlaying) {
        tone(SPEAKER_PIN, melodyCH[songNoteIndex], noteDurationsCH[songNoteIndex]);
        delay(10);
        songNoteStart = now;
        notePlaying = true;
        int idx = frameSchedule[songNoteIndex] + 1; //offset to not re-display first image
        if (idx != -1 && idx != lastFrameShown) {
          frameToShow = idx;
          frameNeedsUpdate = true;
        }
      }
      if (notePlaying && now - songNoteStart >= noteDurationsCH[songNoteIndex]) {
        noTone(SPEAKER_PIN);
        songNoteIndex++;
        notePlaying = false;

        if (songNoteIndex >= sizeof(melodyCH)/sizeof(melodyCH[0])) {
          if (!motionState || sauronWasTriggered) {
            currentState = IDLE;
          } else {
            songNoteIndex = 0;
            currentFrame = 0;
          }
        }
      }
      // if (frameNeedsUpdate && now != lastFrameRenderTime) {
      //   String filename = "shire/theShire" + formatFrameNumber(frameToShow) + ".bmp";
      //   // drawImageSafe(filename);
      //   lastFrameShown = frameToShow;
      //   frameNeedsUpdate = false;
      //   lastFrameRenderTime = now;
      // }
      break;

    case SAURON:
      playTrembleEffectNonBlocking(440, 30);
      if (now - sauronStartTime >= sauronDuration) {
        sauronWasTriggered = false;
        cooldownStartTime = now;
        currentState = COOLDOWN;
        noTone(SPEAKER_PIN);
        Serial.println("Sauron done. Entering cooldown...");

        // Show cooldown screen (rotated display)
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 70);
        tft.println("Sauron has found you!");
        tft.setCursor(10, 100);
        tft.println("Choose your Samwise and");
        tft.setCursor(10, 130);
        tft.println("press the blue circle to");
        tft.setCursor(10, 160);
        tft.println("continue the CTP journey.");
      }
      break;

    case COOLDOWN:
      if (digitalRead(WIO_5S_PRESS) == LOW) {
        delay(300);
        currentState = SAMWISE;
        // tft.fillScreen(TFT_BLACK);
        drawImageSafe("shire/theShire01.bmp");
        delay(500);
        Serial.println("Cooldown acknowledged. Returning to IDLE.");
      }
      break;

    case SAMWISE:
      if (!notePlaying) {
        tone(SPEAKER_PIN, melodyCH[songNoteIndex], noteDurationsCH[songNoteIndex]);
        delay(10);
        songNoteStart = now;
        notePlaying = true;
      }
      if (notePlaying && now - songNoteStart >= noteDurationsCH[songNoteIndex]) {
        noTone(SPEAKER_PIN);
        songNoteIndex++;
        notePlaying = false;

        if (songNoteIndex >= sizeof(melodyCH)/sizeof(melodyCH[0])) {
          currentState = WAIT_FOR_BUTTON;
          songNoteIndex = 0;
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(20, 80);
            tft.println("Press the blue circle");
            tft.setCursor(20, 110);
            tft.println("to begin your CTP");
            tft.setCursor(20, 140);
            tft.println("adventure!");
        }
      }
      break;

  }
}

void drawImageSafe(String filename) {
  File f = SD.open(filename.c_str());
  if (f) {
    f.close();
    Serial.println("Displaying: " + filename);
    drawImage<uint16_t>(filename.c_str(), 0, 0);
  } else {
    Serial.println("ERROR: Missing image: " + filename);
  }
}

void playTrembleEffectNonBlocking(int baseFreq, int variation) {
  static unsigned long lastToggle = 0;
  static bool toggleState = false;
  if (millis() - lastToggle >= 50) {
    toggleState = !toggleState;
    tone(SPEAKER_PIN, toggleState ? baseFreq + variation : baseFreq - variation);
    lastToggle = millis();
  }
}

String formatFrameNumber(int num) {
  char buffer[5];
  sprintf(buffer, "%02d", num);
  return String(buffer);
}
