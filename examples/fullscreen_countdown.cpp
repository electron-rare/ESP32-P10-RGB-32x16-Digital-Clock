/**
 * ESP32 P10 RGB Fullscreen Countdown
 * Compte à rebours plein écran en vert
 * 
 * Ce programme affiche un compte à rebours en plein écran avec :
 * - Chiffres de grande taille (en vert)
 * - Occupation de toute la largeur de l'écran
 * - Compatible avec toutes les configurations de cascade
 * 
 * Auteur: GitHub Copilot
 * Date: Août 2025
 */

#define PxMATRIX_SPI_FREQUENCY 10000000

#include <Arduino.h>
#include <PxMatrix.h>
#include <RTClib.h>
#include <Fonts/FreeSansBold9pt7b.h>  // Police plus grande pour le compte à rebours

// Pins pour la matrice LED
#define P_LAT 5
#define P_A   19
#define P_B   23
#define P_C   18
#define P_OE  4

// Configuration des panneaux - définie par les build flags
#ifndef MATRIX_WIDTH
#define MATRIX_WIDTH 32
#endif

#ifndef MATRIX_HEIGHT
#define MATRIX_HEIGHT 16
#endif

#ifndef MATRIX_PANELS_X
#define MATRIX_PANELS_X 3
#endif

#ifndef MATRIX_PANELS_Y
#define MATRIX_PANELS_Y 1
#endif

// Calcul des dimensions totales
#define TOTAL_WIDTH (MATRIX_WIDTH * MATRIX_PANELS_X)
#define TOTAL_HEIGHT (MATRIX_HEIGHT * MATRIX_PANELS_Y)

// Configuration du timer
hw_timer_t * timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Temps d'affichage ajusté selon le nombre de panneaux
uint8_t display_draw_time = (MATRIX_PANELS_X * MATRIX_PANELS_Y > 4) ? 20 : 30;

// Objet matrice
PxMATRIX display(TOTAL_WIDTH, TOTAL_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C);

// RTC
RTC_DS3231 rtc;

// Couleurs
uint16_t myGREEN    = display.color565(0, 255, 0);    // Vert vif pour le compte à rebours
uint16_t myDARKGREEN = display.color565(0, 150, 0);   // Vert foncé pour les séparateurs
uint16_t myBLACK    = display.color565(0, 0, 0);
uint16_t myRED      = display.color565(255, 0, 0);    // Rouge pour les dernières secondes

// Variables pour le countdown
DateTime countdownTarget;
bool countdownExpired = false;
bool blinkLastSeconds = false;  // Clignotement pour les 10 dernières secondes
bool blinkState = true;
unsigned long lastBlinkTime = 0;
const int BLINK_INTERVAL = 500;  // Intervalle de clignotement 0.5 seconde

// Format d'affichage (0=jours, 1=heures, 2=minutes, 3=secondes uniquement)
int displayFormat = 0;

// Gestionnaire d'interruption pour l'affichage
void IRAM_ATTR display_updater() {
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Activation du timer d'affichage
void display_update_enable() {
  timer = timerBegin(0, 80, true);
  if (timer != NULL) {
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 1500, true);
    timerAlarmEnable(timer);
    Serial.println("Display timer enabled successfully");
  } else {
    Serial.println("ERROR: Failed to create display timer");
  }
}

// Calcul du temps restant
void updateCountdown(int &days, int &hours, int &minutes, int &seconds) {
  DateTime now = rtc.now();
  
  // Vérifier si le countdown est expiré
  if (now >= countdownTarget) {
    countdownExpired = true;
    days = hours = minutes = 0;
    seconds = 0;
    return;
  }
  
  countdownExpired = false;
  
  // Calculer la différence
  TimeSpan diff = countdownTarget - now;
  long totalSeconds = diff.totalseconds();
  
  days = totalSeconds / 86400;
  hours = (totalSeconds % 86400) / 3600;
  minutes = (totalSeconds % 3600) / 60;
  seconds = totalSeconds % 60;
  
  // Activer le clignotement pour les 10 dernières secondes
  blinkLastSeconds = (days == 0 && hours == 0 && minutes == 0 && seconds <= 10);
}

// Détermine le format d'affichage en fonction du temps restant
void updateDisplayFormat(int days, int hours, int minutes, int seconds) {
  if (days > 0) {
    displayFormat = 0;  // Format jours
  } else if (hours > 0) {
    displayFormat = 1;  // Format heures
  } else if (minutes > 0) {
    displayFormat = 2;  // Format minutes
  } else {
    displayFormat = 3;  // Format secondes uniquement
  }
}

// Fonction pour obtenir la largeur du texte
uint16_t getTextWidth(const char* text, const GFXfont* font = NULL) {
  int16_t x1, y1;
  uint16_t w, h;
  
  if (font) display.setFont(font);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setFont(); // Reset to default font
  
  return w;
}

// Dessine les deux points du séparateur d'horloge
void drawColon(int16_t x, int16_t y, uint16_t colonColor) {
  display.fillRect(x, y, 2, 2, colonColor);
  display.fillRect(x, y+4, 2, 2, colonColor);
}

// Affichage du compte à rebours en plein écran
void displayFullscreenCountdown(int days, int hours, int minutes, int seconds) {
  display.clearDisplay();
  
  // Choisir la couleur
  uint16_t countdownColor = myGREEN;
  
  // Si on est dans les 10 dernières secondes et qu'il faut clignoter
  if (blinkLastSeconds) {
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
      lastBlinkTime = currentTime;
      blinkState = !blinkState;
    }
    
    if (!blinkState) {
      countdownColor = myBLACK;  // Faire clignoter en éteignant
    } else {
      countdownColor = myRED;    // Clignoter en rouge
    }
  }
  
  // Si le countdown est expiré
  if (countdownExpired) {
    display.setFont();  // Petite police pour le message expiré
    display.setTextSize(1);
    
    const char* expiredMsg = "TEMPS ECOULE!";
    int16_t textWidth = getTextWidth(expiredMsg);
    int16_t textX = (TOTAL_WIDTH - textWidth) / 2;
    
    display.setCursor(textX, (TOTAL_HEIGHT - 8) / 2);
    display.setTextColor(myRED);
    display.print(expiredMsg);
    return;
  }
  
  char countdownText[20];
  int16_t textWidth, textX;
  
  // Utiliser différents formats selon le temps restant
  switch (displayFormat) {
    case 0:  // Format jours
      sprintf(countdownText, "%dd %02d:%02d", days, hours, minutes);
      display.setFont();
      display.setTextSize(2);
      textWidth = getTextWidth(countdownText);
      textX = (TOTAL_WIDTH - textWidth) / 2;
      display.setTextColor(countdownColor);
      display.setCursor(textX, (TOTAL_HEIGHT - 16) / 2);
      display.print(countdownText);
      break;
      
    case 1:  // Format heures
      sprintf(countdownText, "%02d:%02d:%02d", hours, minutes, seconds);
      display.setFont();
      display.setTextSize(2);
      textWidth = getTextWidth(countdownText);
      textX = (TOTAL_WIDTH - textWidth) / 2;
      display.setTextColor(countdownColor);
      display.setCursor(textX, (TOTAL_HEIGHT - 16) / 2);
      display.print(countdownText);
      break;
      
    case 2:  // Format minutes
      sprintf(countdownText, "%02d:%02d", minutes, seconds);
      display.setFont();
      display.setTextSize(2);
      textWidth = getTextWidth(countdownText);
      textX = (TOTAL_WIDTH - textWidth) / 2;
      display.setTextColor(countdownColor);
      display.setCursor(textX, (TOTAL_HEIGHT - 16) / 2);
      display.print(countdownText);
      break;
      
    case 3:  // Format secondes uniquement
      sprintf(countdownText, "%02d", seconds);
      
      // Si l'écran est suffisamment grand et qu'on a une seule rangée de panneaux
      if (TOTAL_WIDTH >= 64 && MATRIX_PANELS_Y == 1) {
        display.setFont(&FreeSansBold9pt7b);
        textWidth = getTextWidth(countdownText, &FreeSansBold9pt7b);
        textX = (TOTAL_WIDTH - textWidth) / 2;
        display.setTextColor(countdownColor);
        display.setCursor(textX, TOTAL_HEIGHT - 4);
        display.print(countdownText);
      } else {
        // Sinon utiliser une police standard mais plus grande
        display.setFont();
        display.setTextSize(2);
        textWidth = getTextWidth(countdownText);
        textX = (TOTAL_WIDTH - textWidth) / 2;
        display.setTextColor(countdownColor);
        display.setCursor(textX, (TOTAL_HEIGHT - 16) / 2);
        display.print(countdownText);
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 P10 RGB FULLSCREEN COUNTDOWN ===");
  Serial.printf("Configuration: %dx%d panels (%dx%d total resolution)\n", 
                MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);
  
  // Initialisation du RTC
  Serial.println("Initializing RTC...");
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1) delay(10);
  }
  Serial.println("RTC initialized successfully");
  
  // Initialisation de l'affichage avec configuration P10 optimisée
  display.begin(4); // 1/8 scan pour P10
  display.setScanPattern(ZAGZIG);
  display.setMuxPattern(BINARY); 
  const int muxdelay = 10; // Délai de multiplexage
  display.setMuxDelay(muxdelay, muxdelay, muxdelay, muxdelay, muxdelay);
  delay(100);
  
  display_update_enable();
  display.clearDisplay();
  
  // Luminosité adaptée au nombre de panneaux
  int brightness = 150;
  if (MATRIX_PANELS_X > 2) brightness = 100;
  if (MATRIX_PANELS_X > 4) brightness = 80;
  if (MATRIX_PANELS_X > 6) brightness = 60;
  
  display.setBrightness(brightness);
  Serial.printf("Brightness set to: %d\n", brightness);
  
  display.setTextWrap(false);
  display.setRotation(0);
  
  // Configurer le compte à rebours pour 1 minute dans le futur (test)
  DateTime now = rtc.now();
  countdownTarget = now + TimeSpan(0, 0, 1, 0); // +1 minute
  
  Serial.println("Countdown target set to: ");
  Serial.printf("%02d-%02d-%d %02d:%02d:%02d\n", 
                countdownTarget.day(), countdownTarget.month(), countdownTarget.year(),
                countdownTarget.hour(), countdownTarget.minute(), countdownTarget.second());
  
  // Message de démarrage
  display.setTextColor(myGREEN);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("COUNTDOWN");
  
  display.setCursor(0, 8);
  display.print("FULLSCREEN");
  
  delay(2000);
  display.clearDisplay();
  
  Serial.println("Starting fullscreen countdown...");
}

void loop() {
  // Mise à jour du countdown chaque 100ms pour une réactivité accrue
  static unsigned long lastUpdate = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastUpdate >= 100) {
    lastUpdate = currentTime;
    
    int days, hours, minutes, seconds;
    updateCountdown(days, hours, minutes, seconds);
    updateDisplayFormat(days, hours, minutes, seconds);
    displayFullscreenCountdown(days, hours, minutes, seconds);
  }
  
  // Délai minimal pour éviter la saturation du CPU
  delay(10);
}
