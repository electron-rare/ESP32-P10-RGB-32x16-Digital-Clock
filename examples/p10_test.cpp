/**
 * Test de la matrice P10 RGB 32x16
 * Ce programme permet de tester l'affichage sur la matrice P10
 */

#include <Arduino.h>
#include <PxMatrix.h>

// Configuration SPI
#define PxMATRIX_SPI_FREQUENCY 10000000

// Pins pour la matrice LED
#define P_LAT 5
#define P_A   19
#define P_B   23
#define P_C   18
#define P_OE  4

// Dimensions de la matrice
#define MATRIX_WIDTH  32
#define MATRIX_HEIGHT 16

// Configuration des panneaux en cascade
#ifndef MATRIX_PANELS_X
#define MATRIX_PANELS_X 3  // Nombre de panneaux horizontaux (modifiable)
#endif

#ifndef MATRIX_PANELS_Y
#define MATRIX_PANELS_Y 1  // Nombre de panneaux verticaux (généralement 1)
#endif

// Calcul des dimensions totales
#define TOTAL_WIDTH (MATRIX_WIDTH * MATRIX_PANELS_X)
#define TOTAL_HEIGHT (MATRIX_HEIGHT * MATRIX_PANELS_Y)

// Configuration du timer
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;



// Temps d'affichage
uint8_t display_draw_time = 30;

// Temps d'affichage ajusté selon le nombre de panneaux
//uint8_t display_draw_time = (MATRIX_PANELS_X * MATRIX_PANELS_Y > 4) ? 20 : 30;

// Objet matrice avec dimensions totales
PxMATRIX display(TOTAL_WIDTH, TOTAL_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C);

// Couleurs
uint16_t myRED      = display.color565(255, 0, 0);
uint16_t myGREEN    = display.color565(0, 255, 0);
uint16_t myBLUE     = display.color565(0, 0, 255);
uint16_t myYELLOW   = display.color565(255, 255, 0);
uint16_t myCYAN     = display.color565(0, 255, 255);
uint16_t myFUCHSIA  = display.color565(255, 0, 255);
uint16_t myWHITE    = display.color565(255, 255, 255);
uint16_t myBLACK    = display.color565(0, 0, 0);

uint16_t myCOLOR_ARRAY[4] = {myRED, myGREEN, myBLUE, myWHITE};

// Gestionnaire d'interruption pour l'affichage
void IRAM_ATTR display_updater() {
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Activation/désactivation du timer d'affichage
void display_update_enable(bool is_enable) {
  if (is_enable) {
    if (timer == NULL) {
      timer = timerBegin(0, 80, true);
      if (timer != NULL) {
        timerAttachInterrupt(timer, &display_updater, true);
        timerAlarmWrite(timer, 1500, true);
        timerAlarmEnable(timer);
        Serial.println("Display timer enabled successfully");
      } else {
        Serial.println("ERROR: Failed to create display timer");
      }
    } else {
      Serial.println("Display timer already enabled");
    }
  } else {
    if (timer != NULL) {
      timerAlarmDisable(timer);
      timerDetachInterrupt(timer);
      timerEnd(timer);
      timer = NULL;
      Serial.println("Display timer disabled");
    }
  }
}

void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.println();
  Serial.printf("=== Test P10 RGB %dx%d Matrix ===\n", TOTAL_WIDTH, TOTAL_HEIGHT);
  Serial.printf("Configuration: %dx%d panels (%dx%d total resolution)\n", 
                MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);

  // Initialisation de l'affichage
  display.begin(8); // 1/8 scan
  display.setScanPattern(ZAGZIG);
  /*
  display.setMuxPattern(BINARY); 
  display.setPanelsWidth(MATRIX_PANELS_X);
  const int muxdelay = 1; // Délai de multiplexage réduit pour plusieurs panneaux
  display.setMuxDelay(muxdelay, muxdelay, muxdelay, muxdelay, muxdelay);
  */
  delay(100);

  // Activation des interruptions timer
  display_update_enable(true);
  delay(100);

  display.clearDisplay();
  delay(1000);

  // Ajustement de la luminosité en fonction du nombre de panneaux
  int brightness = 125;
  if (MATRIX_PANELS_X > 2) brightness = 100;
  if (MATRIX_PANELS_X > 4) brightness = 80;
  
  display.setBrightness(brightness);
  Serial.printf("Brightness set to: %d\n", brightness);
  delay(100);

  // Test des couleurs pleines
  Serial.println("Testing full screen colors...");
  display.fillScreen(myRED);
  delay(1000);
  display.fillScreen(myGREEN);
  delay(1000);
  display.fillScreen(myBLUE);
  delay(1000);
  display.fillScreen(myWHITE);
  delay(1000);

  display.clearDisplay();
  delay(1000);

  display.setTextWrap(false);
  display.setTextSize(1);
  display.setRotation(0);
  delay(100);

  // Test d'affichage de texte
  Serial.println("Testing text display...");
  display.fillScreen(myRED);
  display.setTextColor(myWHITE);
  display.setCursor(0, 0);
  display.print("TEST");
  display.setCursor(15, 9);
  display.print("P10");
  delay(2500);

  display.clearDisplay();
  delay(1000);

  Serial.println("Setup completed. Starting color loop...");
}

void loop() {
  int myCOLOR_ARRAY_Length = sizeof(myCOLOR_ARRAY) / sizeof(myCOLOR_ARRAY[0]);
  
  // Test 1: Grille des panneaux
  display.clearDisplay();
  Serial.println("Affichage de la grille des panneaux...");
  
  // Dessiner les bordures de chaque panneau
  for (int x = 0; x < MATRIX_PANELS_X; x++) {
    for (int y = 0; y < MATRIX_PANELS_Y; y++) {
      int panelX = x * MATRIX_WIDTH;
      int panelY = y * MATRIX_HEIGHT;
      
      // Bordure colorée pour chaque panneau (couleur différente pour chaque panneau)
      uint16_t color = myCOLOR_ARRAY[(x + y) % myCOLOR_ARRAY_Length];
      
      // Dessiner le contour du panneau
      display.drawRect(panelX, panelY, MATRIX_WIDTH, MATRIX_HEIGHT, color);
      
      // Afficher le numéro du panneau au centre
      char panelText[3];
      sprintf(panelText, "%d", x + y * MATRIX_PANELS_X);
      
      int textX = panelX + (MATRIX_WIDTH - strlen(panelText) * 6) / 2;
      int textY = panelY + (MATRIX_HEIGHT - 8) / 2;
      
      display.setTextColor(myWHITE);
      display.setCursor(textX, textY);
      display.print(panelText);
    }
  }
  delay(3000);
  
  // Test 2: Cadre complet
  display.clearDisplay();
  Serial.println("Affichage du cadre complet...");
  
  // Cadre extérieur
  display.drawRect(0, 0, TOTAL_WIDTH, TOTAL_HEIGHT, myWHITE);
  
  // Diagonales pour visualiser la taille complète
  display.drawLine(0, 0, TOTAL_WIDTH-1, TOTAL_HEIGHT-1, myYELLOW);
  display.drawLine(0, TOTAL_HEIGHT-1, TOTAL_WIDTH-1, 0, myYELLOW);
  
  // Afficher la résolution totale
  char resText[20];
  sprintf(resText, "%dx%d", TOTAL_WIDTH, TOTAL_HEIGHT);
  
  display.setTextColor(myCYAN);
  display.setCursor((TOTAL_WIDTH - strlen(resText) * 6) / 2, TOTAL_HEIGHT / 2 - 4);
  display.print(resText);
  delay(3000);
  
  // Test 3: Couleurs pleines
  Serial.println("Test des couleurs pleines...");
  for (byte i = 0; i < myCOLOR_ARRAY_Length; i++) {
    display.fillScreen(myCOLOR_ARRAY[i]);
    delay(1000);
  }
  
  display.clearDisplay();
  delay(1000);

  // Test 4: Texte défilant
  Serial.println("Test de texte défilant...");
  char scrollText[50];
  sprintf(scrollText, "CONFIGURATION: %dx%d PANNEAUX (%dx%d PIXELS)", 
          MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);
  
  // Faire défiler le texte de droite à gauche
  int textWidth = strlen(scrollText) * 6;
  for (int pos = TOTAL_WIDTH; pos > -textWidth; pos -= 2) {
    display.clearDisplay();
    display.setTextColor(myWHITE);
    display.setCursor(pos, (TOTAL_HEIGHT - 8) / 2);
    display.print(scrollText);
    delay(20);
  }
  
  display.clearDisplay();
  delay(1000);
  
  // Test 5: Texte statique sur chaque panneau
  Serial.println("Test de texte sur chaque panneau...");
  display.clearDisplay();
  
  for (int i = 0; i < myCOLOR_ARRAY_Length; i++) {
    // Test position 1
    display.setTextColor(myCOLOR_ARRAY[i]);
    display.setCursor(0, 0);
    display.print("1234");
    display.setCursor(0, 9);
    display.print("ABCD");
    delay(2500);

    display.clearDisplay();
    delay(1000);
  }
}
