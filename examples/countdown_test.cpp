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
long scrollX = TOTAL_WIDTH; // Commence depuis la droite de tous les panneaux
unsigned long lastScrollTime = 0;
int scrollSpeed = 50;

// Déclaration anticipée de la fonction getTextWidth
uint16_t getTextWidth(const char* text);

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
  
  // Adapter le format selon la taille de l'écran
  char tempText[101];
  if (days > 0) {
    sprintf(tempText, "%s: %dd %02dh %02dm %02ds", countdownTitle, days, hours, minutes, seconds);
  } else if (hours > 0) {
    sprintf(tempText, "%s: %02dh %02dm %02ds", countdownTitle, hours, minutes, seconds);
  } else {
    sprintf(tempText, "%s: %02dm %02ds", countdownTitle, minutes, seconds);
  }
  
  // Vérifier si le texte tient sur l'écran
  uint16_t textWidth = getTextWidth(tempText);
  if (textWidth <= TOTAL_WIDTH) {
    // Le texte tient, pas besoin de défilement
    strcpy(countdownText, tempText);
  } else {
    // Si le texte ne tient pas, utiliser un format plus compact si possible
    if (days > 0) {
      // Format sans titre, avec jours
      sprintf(countdownText, "%dd %02dh %02dm %02ds", days, hours, minutes, seconds);
      // Vérifier si ce format plus court tient sur l'écran
      if (getTextWidth(countdownText) > TOTAL_WIDTH && MATRIX_PANELS_X < 3) {
        // Format ultra-compact pour petits affichages
        sprintf(countdownText, "%dd%02dh", days, hours);
      }
    } else if (hours > 0) {
      // Format sans titre, avec heures
      sprintf(countdownText, "%02dh %02dm %02ds", hours, minutes, seconds);
      // Vérifier si ce format plus court tient sur l'écran
      if (getTextWidth(countdownText) > TOTAL_WIDTH && MATRIX_PANELS_X < 2) {
        // Format ultra-compact pour petits affichages
        sprintf(countdownText, "%02dh%02dm", hours, minutes);
      }
    } else {
      // Format sans titre, minutes seulement
      sprintf(countdownText, "%02dm %02ds", minutes, seconds);
      // Ce format devrait tenir même sur un seul panneau
    }
    
    // Si le format compact ne tient toujours pas, on garde le texte original pour défilement
    if (getTextWidth(countdownText) > TOTAL_WIDTH) {
      strcpy(countdownText, tempText);
    }
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
    
    // Calculer la largeur du texte avec la méthode précise
    int textWidth = getTextWidth(text);
    
    // Effacer toute la ligne de texte sur toute la largeur des panneaux
    display.fillRect(0, 8, TOTAL_WIDTH, 8, myBLACK);
    
    // Afficher le texte à la position actuelle de défilement
    // Le texte doit pouvoir entrer et sortir complètement de l'écran
    display.setCursor(scrollX, 8);
    display.setTextColor(color);
    display.print(text);
    
    // Ajuster la vitesse de défilement en fonction du nombre de panneaux
    int scrollStep = 1;
    if (MATRIX_PANELS_X >= 4) scrollStep = 2;
    if (MATRIX_PANELS_X >= 6) scrollStep = 3;
    
    // Déplacer le texte
    scrollX -= scrollStep;
    
    // Réinitialiser la position quand le texte est complètement sorti de l'écran
    // Il faut attendre que le texte soit complètement sorti
    if (scrollX < -textWidth) {
      // Recommencer depuis la droite de tous les panneaux
      scrollX = TOTAL_WIDTH;
    }
    
    // Pour le débogage, afficher des marqueurs aux extrémités
    display.drawPixel(0, 15, myWHITE);            // Marqueur à gauche
    display.drawPixel(TOTAL_WIDTH-1, 15, myWHITE); // Marqueur à droite
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
  display.setTextSize(1);
  
  // Configuration du countdown de test : 5 minutes dans le futur
  DateTime now = rtc.now();
  countdownTarget = now + TimeSpan(0, 0, 5, 0); // +5 minutes
  
  Serial.println("Countdown target set to: ");
  Serial.printf("%02d-%02d-%d %02d:%02d:%02d\n", 
                countdownTarget.day(), countdownTarget.month(), countdownTarget.year(),
                countdownTarget.hour(), countdownTarget.minute(), countdownTarget.second());
  
  // Test de la largeur d'affichage - afficher une ligne horizontale pour vérifier l'utilisation de tous les panneaux
  display.clearDisplay();
  display.drawFastHLine(0, 0, TOTAL_WIDTH, myWHITE);  // Ligne horizontale en haut
  display.drawFastHLine(0, TOTAL_HEIGHT-1, TOTAL_WIDTH, myWHITE);  // Ligne horizontale en bas
  display.drawFastVLine(0, 0, TOTAL_HEIGHT, myWHITE);  // Ligne verticale à gauche
  display.drawFastVLine(TOTAL_WIDTH-1, 0, TOTAL_HEIGHT, myWHITE);  // Ligne verticale à droite
  
  // Afficher la résolution totale
  char resText[20];
  sprintf(resText, "%dx%d", TOTAL_WIDTH, TOTAL_HEIGHT);
  int resWidth = getTextWidth(resText);
  int resX = (TOTAL_WIDTH - resWidth) / 2;
  if (resX < 0) resX = 0;
  display.setCursor(resX, 4);
  display.setTextColor(myYELLOW);
  display.print(resText);
  
  delay(3000);
  
  // Message de démarrage
  display.clearDisplay();
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
  
  // Vérifier si le texte tient sur l'écran
  uint16_t textWidth = getTextWidth(countdownText);
  uint16_t textColor = countdownExpired ? myRED : myORANGE;
  
  // Effacer la ligne du texte sur toute la largeur disponible
  display.fillRect(0, 8, TOTAL_WIDTH, 8, myBLACK);
  
  // Afficher des marqueurs aux extrémités de l'écran pour visualiser la largeur totale
  display.drawPixel(0, 15, myWHITE);            // Marqueur à gauche
  display.drawPixel(TOTAL_WIDTH-1, 15, myWHITE); // Marqueur à droite
  
  // Affichage du texte (centré ou défilant)
  if (textWidth <= TOTAL_WIDTH) {
    // Le texte tient sur l'écran, on le centre
    int textX = (TOTAL_WIDTH - textWidth) / 2;
    if (textX < 0) textX = 0;
    
    // Afficher le texte centré
    display.setCursor(textX, 8);
    display.setTextColor(textColor);
    display.print(countdownText);
  } else {
    // Le texte est trop long, on utilise le défilement
    runScrollingText(countdownText, textColor);
  }
  
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
