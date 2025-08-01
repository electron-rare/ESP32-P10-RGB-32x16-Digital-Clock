/**
 * ESP32 P10 RGB Cascade Test
 * Test pour vérifier le bon fonctionnement des panneaux P10 en cascade
 * 
 * Ce test affiche :
 * - Des bordures pour vérifier l'alignement des panneaux
 * - La numérotation de chaque panneau
 * - Des tests de couleurs
 * - Un texte défilant sur toute la largeur
 * 
 * Compatible avec toutes les configurations de cascade définies dans platformio.ini
 */

#define PxMATRIX_SPI_FREQUENCY 10000000

#include <Arduino.h>
#include <PxMatrix.h>

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
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Temps d'affichage ajusté selon le nombre de panneaux
uint8_t display_draw_time = (MATRIX_PANELS_X * MATRIX_PANELS_Y > 4) ? 20 : 30;

// Objet matrice
PxMATRIX display(TOTAL_WIDTH, TOTAL_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C);

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

uint16_t colors[] = {myRED, myGREEN, myBLUE, myYELLOW, myCYAN, myMAGENTA, myWHITE, myORANGE};
int numColors = sizeof(colors) / sizeof(colors[0]);

// Variables pour le texte défilant
long scrollX = TOTAL_WIDTH;
unsigned long lastScrollTime = 0;
int scrollSpeed = 50;
const char* scrollText = "=== CASCADE TEST - ESP32 P10 RGB PANELS ===";

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

// Test des bordures et alignement des panneaux
void testPanelAlignment() {
  Serial.println("Testing panel alignment...");
  
  display.clearDisplay();
  
  // Bordure extérieure
  display.drawRect(0, 0, TOTAL_WIDTH, TOTAL_HEIGHT, myWHITE);
  
  // Lignes de séparation entre panneaux horizontaux
  for (int i = 1; i < MATRIX_PANELS_X; i++) {
    int x = i * MATRIX_WIDTH;
    display.drawLine(x, 0, x, TOTAL_HEIGHT - 1, myRED);
  }
  
  // Lignes de séparation entre panneaux verticaux
  for (int i = 1; i < MATRIX_PANELS_Y; i++) {
    int y = i * MATRIX_HEIGHT;
    display.drawLine(0, y, TOTAL_WIDTH - 1, y, myGREEN);
  }
  
  delay(3000);
}

// Test de numérotation des panneaux
void testPanelNumbering() {
  Serial.println("Testing panel numbering...");
  
  display.clearDisplay();
  display.setTextSize(1);
  
  for (int py = 0; py < MATRIX_PANELS_Y; py++) {
    for (int px = 0; px < MATRIX_PANELS_X; px++) {
      int panelNum = py * MATRIX_PANELS_X + px + 1;
      uint16_t color = colors[panelNum % numColors];
      
      // Position au centre de chaque panneau
      int centerX = px * MATRIX_WIDTH + 2;
      int centerY = py * MATRIX_HEIGHT + 2;
      
      // Fond coloré pour le panneau
      display.fillRect(px * MATRIX_WIDTH + 1, py * MATRIX_HEIGHT + 1, 
                      MATRIX_WIDTH - 2, MATRIX_HEIGHT - 2, color);
      
      // Numéro du panneau en noir
      display.setTextColor(myBLACK);
      display.setCursor(centerX, centerY);
      display.print("P");
      display.setCursor(centerX, centerY + 8);
      display.print(panelNum);
      
      delay(800);
    }
  }
  
  delay(3000);
}

// Test des couleurs sur tous les panneaux
void testColors() {
  Serial.println("Testing colors...");
  
  for (int i = 0; i < numColors; i++) {
    display.fillScreen(colors[i]);
    delay(800);
  }
  
  display.clearDisplay();
  delay(500);
}

// Test du texte défilant
void testScrollingText() {
  Serial.println("Testing scrolling text...");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(myWHITE);
  
  // Calculer la largeur approximative du texte
  int textWidth = strlen(scrollText) * 6;
  
  // Faire défiler le texte 2 fois
  for (int cycle = 0; cycle < 2; cycle++) {
    scrollX = TOTAL_WIDTH;
    
    while (scrollX > -textWidth) {
      unsigned long currentTime = millis();
      if (currentTime - lastScrollTime >= scrollSpeed) {
        lastScrollTime = currentTime;
        
        // Effacer la ligne de texte
        display.fillRect(0, 8, TOTAL_WIDTH, 8, myBLACK);
        
        // Afficher le texte à la nouvelle position
        display.setCursor(scrollX, 8);
        display.print(scrollText);
        
        scrollX--;
      }
      
      delay(5);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 P10 RGB CASCADE TEST ===");
  Serial.printf("Configuration: %dx%d panels (%dx%d total resolution)\n", 
                MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);
  Serial.printf("Draw time: %d\n", display_draw_time);
  
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
  
  // Message de démarrage
  display.setTextSize(1);
  display.setTextColor(myWHITE);
  
  // Calculer position centrée
  String startMsg = "CASCADE TEST";
  int msgWidth = startMsg.length() * 6;
  int startX = (TOTAL_WIDTH - msgWidth) / 2;
  if (startX < 0) startX = 0;
  
  display.setCursor(startX, 0);
  display.print(startMsg);
  
  if (TOTAL_HEIGHT > 8) {
    String configMsg = String(MATRIX_PANELS_X) + "x" + String(MATRIX_PANELS_Y) + " PANELS";
    int configWidth = configMsg.length() * 6;
    int configX = (TOTAL_WIDTH - configWidth) / 2;
    if (configX < 0) configX = 0;
    
    display.setCursor(configX, 8);
    display.print(configMsg);
  }
  
  delay(3000);
  
  Serial.println("Starting cascade tests...");
}

void loop() {
  // Séquence de tests
  
  Serial.println("\n--- Test Cycle Starting ---");
  
  // 1. Test d'alignement des panneaux
  testPanelAlignment();
  
  // 2. Test de numérotation des panneaux
  testPanelNumbering();
  
  // 3. Test des couleurs
  testColors();
  
  // 4. Test du texte défilant
  testScrollingText();
  
  // Message de fin de cycle
  display.clearDisplay();
  display.setTextColor(myGREEN);
  
  String okMsg = "TESTS OK";
  int okWidth = okMsg.length() * 6;
  int okX = (TOTAL_WIDTH - okWidth) / 2;
  if (okX < 0) okX = 0;
  
  display.setCursor(okX, 0);
  display.print(okMsg);
  
  if (TOTAL_HEIGHT > 8) {
    String restartMsg = "RESTARTING...";
    int restartWidth = restartMsg.length() * 6;
    int restartX = (TOTAL_WIDTH - restartWidth) / 2;
    if (restartX < 0) restartX = 0;
    
    display.setCursor(restartX, 8);
    display.print(restartMsg);
  }
  
  Serial.println("--- Test Cycle Completed ---");
  Serial.println("Restarting in 5 seconds...\n");
  delay(5000);
}
