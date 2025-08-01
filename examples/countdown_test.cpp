/**
 * ESP32 P10 RGB Countdown Test
 * Test pour vérifier le bon fonctionnement du compte à rebours
 * 
 * Ce test affiche :
 * - Un countdown configuré pour 5 minutes dans le futur
 * - Différents formats d'affichage selon le temps restant
 * - Test d'expiration du countdown
 * 
 * Compatible avec toutes les configurations de cascade définies dans platformio.ini
 */

#define PxMATRIX_SPI_FREQUENCY 10000000

#include <Arduino.h>
#include <PxMatrix.h>
#include <RTClib.h>

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
#define MATRIX_PANELS_X 1
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

// Couleurs prédéfinies
uint16_t myRED      = display.color565(255, 0, 0);
uint16_t myGREEN    = display.color565(0, 255, 0);
uint16_t myBLUE     = display.color565(0, 0, 255);
uint16_t myYELLOW   = display.color565(255, 255, 0);
uint16_t myCYAN     = display.color565(0, 255, 255);
uint16_t myMAGENTA  = display.color565(255, 0, 255);
uint16_t myWHITE    = display.color565(255, 255, 255);
uint16_t myBLACK    = display.color565(0, 0, 0);
uint16_t myORANGE   = display.color565(255, 165, 0);

// Variables pour le countdown de test
DateTime countdownTarget;
char countdownText[101];
bool countdownExpired = false;
const char* countdownTitle = "TEST COUNTDOWN";

// Variables pour le texte défilant
long scrollX = TOTAL_WIDTH;
unsigned long lastScrollTime = 0;
int scrollSpeed = 50;

// Gestionnaire d'interruption pour l'affichage
void IRAM_ATTR display_updater() {
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Activation du timer d'affichage
void display_update_enable() {
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &display_updater, true);
  timerAlarmWrite(timer, 1500, true);
  timerAlarmEnable(timer);
}

// Fonction de calcul et formatage du countdown
void updateCountdown() {
  DateTime now = rtc.now();
  
  // Vérifier si le countdown est expiré
  if (now >= countdownTarget) {
    countdownExpired = true;
    strcpy(countdownText, countdownTitle);
    strcat(countdownText, " - EXPIRED!");
    return;
  }
  
  countdownExpired = false;
  
  // Calculer la différence
  TimeSpan diff = countdownTarget - now;
  long totalSeconds = diff.totalseconds();
  
  int days = totalSeconds / 86400;
  int hours = (totalSeconds % 86400) / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;
  
  // Formater le texte selon la durée restante
  if (days > 0) {
    sprintf(countdownText, "%s: %dd %02dh %02dm %02ds", countdownTitle, days, hours, minutes, seconds);
  } else if (hours > 0) {
    sprintf(countdownText, "%s: %02dh %02dm %02ds", countdownTitle, hours, minutes, seconds);
  } else {
    sprintf(countdownText, "%s: %02dm %02ds", countdownTitle, minutes, seconds);
  }
}

// Fonction pour obtenir la largeur du texte
uint16_t getTextWidth(const char* text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// Fonction de texte défilant
void runScrollingText(const char* text, uint16_t color) {
  unsigned long currentTime = millis();
  if (currentTime - lastScrollTime >= scrollSpeed) {
    lastScrollTime = currentTime;
    
    // Calculer la largeur du texte
    int textWidth = strlen(text) * 6; // Approximation
    
    // Effacer la ligne de texte
    display.fillRect(0, 8, TOTAL_WIDTH, 8, myBLACK);
    
    // Afficher le texte à la nouvelle position
    display.setCursor(scrollX, 8);
    display.setTextColor(color);
    display.print(text);
    
    scrollX--;
    if (scrollX < -textWidth) {
      scrollX = TOTAL_WIDTH;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 P10 RGB COUNTDOWN TEST ===");
  Serial.printf("Configuration: %dx%d panels (%dx%d total resolution)\n", 
                MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);
  
  // Initialisation du RTC
  Serial.println("Initializing RTC...");
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1) delay(10);
  }
  Serial.println("RTC initialized successfully");
  
  // Initialisation de l'affichage
  display.begin(8);
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
  display.setTextSize(1);
  
  // Configuration du countdown de test : 5 minutes dans le futur
  DateTime now = rtc.now();
  countdownTarget = now + TimeSpan(0, 0, 5, 0); // +5 minutes
  
  Serial.println("Countdown target set to: ");
  Serial.printf("%02d-%02d-%d %02d:%02d:%02d\n", 
                countdownTarget.day(), countdownTarget.month(), countdownTarget.year(),
                countdownTarget.hour(), countdownTarget.minute(), countdownTarget.second());
  
  // Message de démarrage
  display.setTextColor(myWHITE);
  
  String startMsg = "COUNTDOWN TEST";
  int msgWidth = startMsg.length() * 6;
  int startX = (TOTAL_WIDTH - msgWidth) / 2;
  if (startX < 0) startX = 0;
  
  display.setCursor(startX, 0);
  display.print(startMsg);
  
  if (TOTAL_HEIGHT > 8) {
    String targetMsg = "5 MIN TARGET";
    int targetWidth = targetMsg.length() * 6;
    int targetX = (TOTAL_WIDTH - targetWidth) / 2;
    if (targetX < 0) targetX = 0;
    
    display.setCursor(targetX, 8);
    display.print(targetMsg);
  }
  
  delay(3000);
  display.clearDisplay();
  
  Serial.println("Starting countdown test...");
}

void loop() {
  // Mise à jour du countdown chaque seconde
  static unsigned long lastUpdate = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastUpdate >= 1000) {
    lastUpdate = currentTime;
    updateCountdown();
  }
  
  // Affichage en défilement
  uint16_t textColor = countdownExpired ? myRED : myORANGE;
  runScrollingText(countdownText, textColor);
  
  // Affichage de l'heure actuelle en haut
  static unsigned long lastClockUpdate = 0;
  if (currentTime - lastClockUpdate >= 500) {
    lastClockUpdate = currentTime;
    
    DateTime now = rtc.now();
    char timeText[10];
    sprintf(timeText, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    
    // Effacer la ligne du haut
    display.fillRect(0, 0, TOTAL_WIDTH, 8, myBLACK);
    
    // Centrer l'heure
    int timeWidth = strlen(timeText) * 6;
    int timeX = (TOTAL_WIDTH - timeWidth) / 2;
    if (timeX < 0) timeX = 0;
    
    display.setCursor(timeX, 0);
    display.setTextColor(myCYAN);
    display.print(timeText);
  }
  
  delay(10);
}
