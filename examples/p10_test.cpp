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

// Configuration du timer
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Temps d'affichage
uint8_t display_draw_time = 30;

// Objet matrice
PxMATRIX display(MATRIX_WIDTH, MATRIX_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C);

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
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 1500, true);
    timerAlarmEnable(timer);
  } else {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }
}

void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== Test P10 RGB 32x16 Matrix ===");

  // Initialisation de l'affichage
  display.begin(8); // Valeur 8 pour un panneau 1/8 scan
  delay(100);

  // Activation des interruptions timer
  display_update_enable(true);
  delay(100);

  display.clearDisplay();
  delay(1000);

  display.setBrightness(125); // 0-255
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
  
  for (byte i = 0; i < myCOLOR_ARRAY_Length; i++) {
    // Test position 1
    display.setTextColor(myCOLOR_ARRAY[i]);
    display.setCursor(0, 0);
    display.print("1234");
    display.setCursor(0, 9);
    display.print("ABCD");
    delay(2500);

    display.clearDisplay();
    delay(1000);

    // Test position 2
    display.setTextColor(myCOLOR_ARRAY[i]);
    display.setCursor(4, 0);
    display.print("1234");
    display.setCursor(4, 9);
    display.print("ABCD");
    delay(2500);

    display.clearDisplay();
    delay(1000);

    // Test position 3
    display.setCursor(9, 0);
    display.print("1234");
    display.setCursor(9, 9);
    display.print("ABCD");
    delay(2500);
  
    display.clearDisplay();
    delay(1000);
  }
}
