#define PIR_MOTION_SENSOR D0
#define SPEAKER_PIN WIO_BUZZER

#include "TFT_eSPI.h"
#include "Seeed_FS.h"  // SD card library
#include "RawImage.h"   // Image processing library
#include "pitches.h"

TFT_eSPI tft;

// --- Music Playback State ---
bool isPlayingSong = false;
int songNoteIndex = 0;
unsigned long songNoteStart = 0;
bool notePlaying = false;
int lastFrameShown = -1;

// --- Animation Playback State ---
const int totalFrames = 4;
int currentFrame = 0;
unsigned long lastFrameTime = 0;
const int frameInterval = 2000; // 1000ms = 1 FPS

// --- Sauron timing --
int searchingState = false;
unsigned long timeSinceMotionDetected = 0;
unsigned long timeMotionIsDetected = 0;
unsigned long searchingTime = 0;
unsigned long nextCheck = 0;
unsigned long sauronThreshold = 0;
unsigned long randomDelay = 0;
bool inSauronMode = false;
unsigned long sauronStartTime = 0;
const unsigned long sauronDuration = 5000; // 5 seconds
bool sauronWasTriggered = false;

bool systemArmed = false; // True after button press



void playTrembleEffectNonBlocking(int baseFreq, int variation, int duration) {
  static unsigned long lastToggle = 0;
  static bool toggleState = false;

  if (millis() - lastToggle >= 50) {
    toggleState = !toggleState;
    tone(SPEAKER_PIN, toggleState ? baseFreq + variation : baseFreq - variation);
    lastToggle = millis();
  }
}

// Define note durations based on correct tempo for concerning hobbits
int halfNoteCH = 1154;
int quarterNoteCH = 577;
int eighthNoteCH = 288;
int sixteenthNoteCH = 144;
int dotQuarterCH = 865;

// Corresponding note durations: 4 = quarter note, 8 = eighth note, etc.
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

// 4 image frames: 1, 2, 3, and 4 at specific note indexes
int frameSchedule[] = {
  0, -1, -1, -1,   
  1, -1, -1, -1,   // frame 2 at note 4
  2, -1, -1, -1,   // frame 3 at note 8
  3, -1, -1, -1,   // frame 4 at note 12
  -1, -1, -1       // no new frames after that
};

void setup() {
    Serial.begin(115200);

    // Initialize SD card
    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
        Serial.println("SD Card Initialization Failed!");
        while (1);
    }

    tft.begin();
    tft.setRotation(3);
    pinMode(PIR_MOTION_SENSOR, INPUT);
    pinMode(13, OUTPUT);  // LED for motion debug
    pinMode(WIO_5S_PRESS, INPUT_PULLUP);

    randomSeed(analogRead(A2));

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("Press the blue button");
    tft.setCursor(20, 130);
    tft.println("to begin your CTP");
    tft.setCursor(20, 160);
    tft.println("adventure!");

    while (digitalRead(WIO_5S_PRESS) == HIGH); // Wait for button press
    delay(500);  // Debounce
    systemArmed = true;

    tft.fillScreen(TFT_BLACK);
}

void loop() {
  int motionState = digitalRead(PIR_MOTION_SENSOR);
  Serial.println(motionState);
  digitalWrite(13, motionState); // LED debug

  if (!systemArmed) return;

  if (inSauronMode) {
    playTrembleEffectNonBlocking(440, 30, sauronDuration);

    if (millis() - sauronStartTime >= sauronDuration) {
      inSauronMode = false;
      sauronWasTriggered = false;
      Serial.println("Sauron gone. Back to idle.");
      noTone(SPEAKER_PIN);
    }

    return; // skip rest of loop while in Sauron mode
  }

  // If motion is still active after Sauron mode ends, restart everything
  if (!inSauronMode && sauronWasTriggered && motionState && !isPlayingSong) {
    Serial.println("Motion still detected after Sauron — restarting song.");
    isPlayingSong = true;
    songNoteIndex = 0;
    notePlaying = false;
    currentFrame = 0;
    lastFrameTime = millis();

    // Re-arm the Sauron search timer
    searchingState = true;
    timeSinceMotionDetected = 0;
    timeMotionIsDetected = millis();
    searchingTime = millis();
    nextCheck = searchingTime + 500;
    randomDelay = random(10000, 15001);
    sauronThreshold = millis() + randomDelay;
  }

  unsigned long now = millis();

  // Trigger playback once when motion is detected
  if (motionState && !isPlayingSong) {
    Serial.println("Motion detected! Starting animation + music...");
    isPlayingSong = true;
    songNoteIndex = 0;
    notePlaying = false;
    currentFrame = 1;
    lastFrameTime = now;
    searchingState = true;
    timeSinceMotionDetected = 0;
    timeMotionIsDetected = millis();
    searchingTime = millis();
    nextCheck = searchingTime + 500;
    randomDelay = random(10000, 15001);
    sauronThreshold = millis() + randomDelay;
    Serial.print(sauronThreshold);
  }

    // --- SAURON TRACKING ---
  searchingTime = millis();
  if (searchingState && !inSauronMode && now > nextCheck) {
    if (!motionState) {
      // Cancel Sauron tracking if motion stops
      searchingState = false;
      sauronWasTriggered = false;
      Serial.println("Motion stopped — Sauron tracking cancelled.");
      return;
    }

    timeSinceMotionDetected = now - timeMotionIsDetected;
    Serial.print("Time since motion started: ");
    Serial.println(timeSinceMotionDetected);
    nextCheck += 500;

    if (timeSinceMotionDetected > randomDelay) {
    Serial.println("Aughhhh Sauron found you!!");

    // Try drawing image FIRST
    String sauronImage = "sauron/eyeOfSauron.bmp";
    File f = SD.open(sauronImage.c_str());
    if (f) {
      f.close();
      Serial.println("Sauron image found, rendering...");
      drawImage<uint16_t>(sauronImage.c_str(), 0, 0);
    } else {
      Serial.println("ERROR: Sauron image not found on SD card!");
    }

    // Then enter Sauron mode
    inSauronMode = true;
    sauronWasTriggered = true;
    sauronStartTime = now;

    noTone(SPEAKER_PIN);
    isPlayingSong = false;
    notePlaying = false;
    songNoteIndex = 0;
    searchingState = false;

    }
  }


  // --- MUSIC PLAYBACK ---
  if (isPlayingSong && songNoteIndex < sizeof(melodyCH)/sizeof(melodyCH[0]) && !inSauronMode) {
    if (!notePlaying) {
      tone(SPEAKER_PIN, melodyCH[songNoteIndex], noteDurationsCH[songNoteIndex]);
      songNoteStart = now;
      notePlaying = true;
      Serial.print("Playing note ");
      Serial.println(songNoteIndex);
    }

    if (notePlaying && now - songNoteStart >= noteDurationsCH[songNoteIndex]) {
      noTone(SPEAKER_PIN);
      songNoteIndex++;
      notePlaying = false;

      // End of song
      if (songNoteIndex >= sizeof(melodyCH)/sizeof(melodyCH[0])) {
        Serial.println("Song finished.");
        songNoteIndex = 0;
        notePlaying = false;
        currentFrame = 0;
        lastFrameTime = millis();

        if (motionState && !sauronWasTriggered) {
          Serial.println("Looping song due to sustained motion.");
          // keep playing!
        } else {
          Serial.println("No more motion — pausing song.");
          isPlayingSong = false;
        }
      }
    }
  }

  // --- ANIMATION LOOPING ---
  if (isPlayingSong && now - lastFrameTime >= frameInterval && !inSauronMode) {
    lastFrameTime = now;

    String filename = "shire/theShire" + formatFrameNumber(currentFrame) + ".bmp";
    Serial.println("Displaying frame: " + filename);
    drawImage<uint16_t>(filename.c_str(), 0, 0);
    delay(10);

    currentFrame++;
    if (currentFrame > totalFrames) {
      currentFrame = 0; // loop the animation
    }
  }

  // --- IDLE STATE ---
  if (!isPlayingSong && !inSauronMode && !motionState) {
    drawImage<uint16_t>("shire/theShire00.bmp", 0, 0);
    delay(500);
  }
}

// Formats the frame number into "0001", "0002", etc.
String formatFrameNumber(int num) {
    char buffer[5];  // Buffer to store formatted number
    sprintf(buffer, "%02d", num);
    return String(buffer);
}
