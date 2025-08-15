/**
 * Test de cascade de panneaux P10 RGB
 * 
 * Ce programme permet de tester l'affichage sur plusieurs panneaux P10 en cascade
 * Il affiche différents motifs visuels pour vérifier que toute la surface est utilisée
 * 
 * Compatible avec toutes les configurations de cascade définies dans platformio.ini
 */

#define PxMATRIX_SPI_FREQUENCY 100000000

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

// Couleurs
uint16_t myRED      = display.color565(255, 0, 0);
uint16_t myGREEN    = display.color565(0, 255, 0);
uint16_t myBLUE     = display.color565(0, 0, 255);
uint16_t myYELLOW   = display.color565(255, 255, 0);
uint16_t myCYAN     = display.color565(0, 255, 255);
uint16_t myMAGENTA  = display.color565(255, 0, 255);
uint16_t myWHITE    = display.color565(255, 255, 255);
uint16_t myBLACK    = display.color565(0, 0, 0);
uint16_t myORANGE   = display.color565(255, 165, 0);
uint16_t myPURPLE   = display.color565(128, 0, 255);
uint16_t myPINK     = display.color565(255, 128, 128);

// Tableau de couleurs pour les tests
uint16_t myColorArray[] = {
  myRED, myGREEN, myBLUE, myYELLOW, 
  myCYAN, myMAGENTA, myORANGE, myPURPLE, 
  myPINK, myWHITE
};

// Variables pour les animations
unsigned long lastUpdate = 0;
uint8_t testMode = 0;
uint8_t testStep = 0;
bool transitionActive = false;
unsigned long transitionStartTime = 0;
const unsigned long TEST_DURATION = 5000; // 5 secondes par test

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

// Affichage de la grille des panneaux
void testGrid() {
  display.clearDisplay();
  
  // Dessiner les bordures de chaque panneau
  for (int x = 0; x < MATRIX_PANELS_X; x++) {
    for (int y = 0; y < MATRIX_PANELS_Y; y++) {
      int panelX = x * MATRIX_WIDTH;
      int panelY = y * MATRIX_HEIGHT;
      
      // Bordure colorée pour chaque panneau (couleur différente pour chaque panneau)
      uint16_t color = myColorArray[(x + y * MATRIX_PANELS_X) % 10];
      
      // Dessiner le contour du panneau
      display.drawRect(panelX, panelY, MATRIX_WIDTH, MATRIX_HEIGHT, color);
      
      // Afficher le numéro du panneau au centre
      char panelText[5];
      sprintf(panelText, "%d,%d", x, y);
      
      int textX = panelX + (MATRIX_WIDTH - strlen(panelText) * 6) / 2;
      int textY = panelY + (MATRIX_HEIGHT - 8) / 2;
      
      display.setTextColor(myWHITE);
      display.setCursor(textX, textY);
      display.print(panelText);
    }
  }
}

// Afficher un cadre complet autour de tous les panneaux
void testFullFrame() {
  display.clearDisplay();
  
  // Cadre extérieur
  display.drawRect(0, 0, TOTAL_WIDTH, TOTAL_HEIGHT, myWHITE);
  
  // Cadre intérieur
  display.drawRect(2, 2, TOTAL_WIDTH-4, TOTAL_HEIGHT-4, myRED);
  
  // Diagonales pour visualiser la taille complète
  display.drawLine(0, 0, TOTAL_WIDTH-1, TOTAL_HEIGHT-1, myYELLOW);
  display.drawLine(0, TOTAL_HEIGHT-1, TOTAL_WIDTH-1, 0, myYELLOW);
  
  // Afficher la résolution totale
  char resText[20];
  sprintf(resText, "%dx%d", TOTAL_WIDTH, TOTAL_HEIGHT);
  
  display.setTextColor(myCYAN);
  display.setCursor((TOTAL_WIDTH - strlen(resText) * 6) / 2, TOTAL_HEIGHT / 2 - 4);
  display.print(resText);
}

// Test avec des barres de couleur horizontales
void testColorBars() {
  display.clearDisplay();
  
  // Nombre de barres à afficher
  int numBars = min(10, TOTAL_HEIGHT / 2);
  int barHeight = TOTAL_HEIGHT / numBars;
  
  // Dessiner des barres horizontales de couleurs différentes
  for (int i = 0; i < numBars; i++) {
    int y = i * barHeight;
    uint16_t color = myColorArray[i % 10];
    display.fillRect(0, y, TOTAL_WIDTH, barHeight, color);
  }
}

// Test avec quadrillage sur toute la surface
void testCheckerboard() {
  display.clearDisplay();
  
  // Taille des carrés
  int squareSize = 8;
  
  // Dessiner un quadrillage alternant deux couleurs
  for (int x = 0; x < TOTAL_WIDTH; x += squareSize) {
    for (int y = 0; y < TOTAL_HEIGHT; y += squareSize) {
      // Alterner les couleurs
      uint16_t color = ((x/squareSize + y/squareSize) % 2 == 0) ? myBLUE : myYELLOW;
      display.fillRect(x, y, squareSize, squareSize, color);
    }
  }
}

// Test avec du texte qui défile sur toute la largeur
void testScrollingText() {
  static int scrollX = TOTAL_WIDTH;
  static unsigned long lastScrollTime = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastScrollTime >= 50) {
    lastScrollTime = currentTime;
    
    // Effacer l'écran
    display.clearDisplay();
    
    // Texte de test qui affiche la configuration des panneaux
    char text[100];
    sprintf(text, "CONFIGURATION: %dx%d PANNEAUX (%dx%d PIXELS)", 
            MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);
    
    // Calculer la largeur approximative du texte
    int textWidth = strlen(text) * 6;
    
    // Afficher le texte à la position actuelle
    display.setCursor(scrollX, (TOTAL_HEIGHT - 8) / 2);
    display.setTextColor(myWHITE);
    display.print(text);
    
    // Déplacer le texte
    scrollX--;
    
    // Réinitialiser la position quand le texte est complètement sorti de l'écran
    if (scrollX < -textWidth) {
      scrollX = TOTAL_WIDTH;
    }
    
    // Afficher un indicateur de la largeur totale
    display.drawPixel(0, 0, myRED);
    display.drawPixel(TOTAL_WIDTH-1, 0, myRED);
    display.drawPixel(0, TOTAL_HEIGHT-1, myRED);
    display.drawPixel(TOTAL_WIDTH-1, TOTAL_HEIGHT-1, myRED);
  }
}

// Test avec des cercles concentriques
void testCircles() {
  display.clearDisplay();
  
  // Centre de l'écran
  int centerX = TOTAL_WIDTH / 2;
  int centerY = TOTAL_HEIGHT / 2;
  
  // Rayon maximum possible
  int maxRadius = min(TOTAL_WIDTH, TOTAL_HEIGHT) / 2;
  
  // Dessiner plusieurs cercles concentriques de couleurs différentes
  for (int r = maxRadius; r > 0; r -= 2) {
    uint16_t color = myColorArray[(maxRadius - r) % 10];
    display.drawCircle(centerX, centerY, r, color);
  }
}

// Transition entre les tests
void performTransition() {
  static int transitionStep = 0;
  static unsigned long lastTransitionUpdate = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastTransitionUpdate >= 50) {
    lastTransitionUpdate = currentTime;
    
    if (transitionStep < TOTAL_HEIGHT) {
      // Transition par balayage horizontal
      display.fillRect(0, 0, TOTAL_WIDTH, transitionStep, myBLACK);
      transitionStep++;
    } else {
      // Transition terminée
      transitionStep = 0;
      transitionActive = false;
      testMode = (testMode + 1) % 6; // Passer au test suivant (6 tests au total)
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 P10 RGB CASCADE TEST ===");
  Serial.printf("Configuration: %dx%d panels (%dx%d total resolution)\n", 
                MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);
  
  // Initialisation de l'affichage avec configuration P10 optimisée
  display.begin(8); // 1/8 scan pour P10
  display.setScanPattern(ZAGZIG);
  display.setMuxPattern(BINARY); 
  const int muxdelay = 1; // Délai de multiplexage
  display.setMuxDelay(muxdelay, muxdelay, muxdelay, muxdelay, muxdelay);
  display.setPanelsWidth(MATRIX_PANELS_X);
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
  
  // Message de bienvenue
  display.clearDisplay();
  display.setTextColor(myGREEN);
  
  char welcomeText[50];
  sprintf(welcomeText, "P10 CASCADE TEST");
  int textWidth = strlen(welcomeText) * 6;
  int textX = (TOTAL_WIDTH - textWidth) / 2;
  if (textX < 0) textX = 0;
  
  display.setCursor(textX, TOTAL_HEIGHT / 2 - 4);
  display.print(welcomeText);
  
  delay(2000);
  
  // Démarrer les tests
  lastUpdate = millis();
  
  Serial.println("Starting cascade tests...");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Vérifier s'il faut changer de mode de test
  if (!transitionActive && (currentTime - lastUpdate >= TEST_DURATION)) {
    transitionActive = true;
    transitionStartTime = currentTime;
  }
  
  // Gérer la transition entre les tests
  if (transitionActive) {
    performTransition();
  } else {
    // Exécuter le test actuel
    switch (testMode) {
      case 0:
        testGrid();
        break;
      case 1:
        testFullFrame();
        break;
      case 2:
        testColorBars();
        break;
      case 3:
        testCheckerboard();
        break;
      case 4:
        testScrollingText();
        break;
      case 5:
        testCircles();
        break;
    }
  }
  
  delay(10);
}
