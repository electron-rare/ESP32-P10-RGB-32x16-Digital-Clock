#include <Arduino.h>
#include <PxMatrix.h>

// Pins pour la matrice LED
#define P_LAT 5
#define P_A   19
#define P_B   23
#define P_C   18
#define P_OE  4

// Configuration des panneaux

#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 16
#define MATRIX_PANELS_X 3
#define MATRIX_PANELS_Y 1


// Calcul des dimensions totales
#define TOTAL_WIDTH (MATRIX_WIDTH * MATRIX_PANELS_X)
#define TOTAL_HEIGHT (MATRIX_HEIGHT * MATRIX_PANELS_Y)

// Configuration du timer
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t display_draw_time = 30;

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

// Configurations de display.begin() à tester
const int SCAN_PATTERNS[] = {1, 2, 4, 8, 16};
const char* SCAN_NAMES[] = {"1/4 scan", "1/8 scan", "1/16 scan", "1/32 scan", "1/64 scan"};
const int NUM_SCAN_PATTERNS = sizeof(SCAN_PATTERNS) / sizeof(SCAN_PATTERNS[0]);

// Configurations de setScanPattern() à tester
const scan_patterns SCAN_TYPES[] = {
  LINE,
  ZIGZAG,
  ZZAGG,
  ZAGGIZ,
  WZAGZIG,
  VZAG,
  ZAGZIG,
  WZAGZIG2,
  ZZIAGG
};
const char* SCAN_TYPE_NAMES[] = {
  "LINE",
  "ZIGZAG",
  "ZZAGG",
  "ZAGGIZ",
  "WZAGZIG",
  "VZAG",
  "ZAGZIG",
  "WZAGZIG2",
  "ZZIAGG"
};
const int NUM_SCAN_TYPES = sizeof(SCAN_TYPES) / sizeof(SCAN_TYPES[0]);

// Configurations de setMuxPattern() à tester
const mux_patterns MUX_PATTERNS[] = {BINARY, STRAIGHT};
const char* MUX_PATTERN_NAMES[] = {"BINARY", "STRAIGHT"};
const int NUM_MUX_PATTERNS = sizeof(MUX_PATTERNS) / sizeof(MUX_PATTERNS[0]);

// Structure pour stocker la configuration de test en cours
struct TestConfig {
  int beginPattern;
  const char* beginName;
  scan_patterns scanType;
  const char* scanTypeName;
  mux_patterns muxPattern;
  const char* muxPatternName;
  int muxDelay;
  int brightness;
  int rotation;
};

// Prototypes de fonctions
void displayTestInfo(TestConfig config);
void setNextTestConfig();
void initDisplay(TestConfig config);

// Variables pour le test en cours
TestConfig currentTest;
// Ajout des tableaux pour les nouveaux paramètres
const int MUXDELAYS[] = {1, 5, 10, 20, 50};
const int NUM_MUXDELAYS = sizeof(MUXDELAYS) / sizeof(MUXDELAYS[0]);
const int BRIGHTNESSES[] = {50, 100, 150, 200, 255};
const int NUM_BRIGHTNESSES = sizeof(BRIGHTNESSES) / sizeof(BRIGHTNESSES[0]);
const int ROTATIONS[] = {0, 1, 2, 3};
const int NUM_ROTATIONS = sizeof(ROTATIONS) / sizeof(ROTATIONS[0]);

int currentConfigIndex = 0;
unsigned long testStartTime = 0;
const unsigned long TEST_DURATION = 5000; // 5 secondes par test
bool testRunning = false;
bool skipToNextTest = false; // Pour permettre le saut manuel

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
  timerAlarmWrite(timer, 4000, true);
  timerAlarmEnable(timer);
}

// Initialisation de l'affichage avec tous les paramètres
void initDisplay(TestConfig config) {
  // Arrêter le timer précédent s'il est actif
  if (timer != NULL) {
    timerAlarmDisable(timer);
    timerDetachInterrupt(timer);
    timerEnd(timer);
    timer = NULL;
  }
  display.setPanelsWidth(MATRIX_PANELS_X);

  // Réinitialiser l'affichage
  display.clearDisplay();

  // Initialiser avec le nouveau pattern
  display.begin(config.beginPattern);

  // Configuration scan pattern et mux pattern
  display.setScanPattern(config.scanType);
  display.setMuxPattern(config.muxPattern);

  // Délai de multiplexage variable
  display.setMuxDelay(config.muxDelay, config.muxDelay, config.muxDelay, config.muxDelay, config.muxDelay);

  // Réactiver le timer
  display_update_enable();

  // Luminosité variable
  display.setBrightness(config.brightness);

  display.setTextWrap(false);
  display.setRotation(config.rotation);

  // Afficher les informations du test
  displayTestInfo(config);
}

// Affichage des informations du test en cours
void displayTestInfo(TestConfig config) {
  display.clearDisplay();

  // Cadre autour de l'écran
  display.drawRect(0, 0, TOTAL_WIDTH, TOTAL_HEIGHT, myWHITE);

  // Texte du test en cours
  display.setTextSize(1);
  display.setTextColor(myYELLOW);

  // Afficher le mode begin
  String beginText = config.beginName;
  int beginWidth = beginText.length() * 6;
  int beginX = (TOTAL_WIDTH - beginWidth) / 2;
  display.setCursor(beginX, 1);
  display.print(beginText);

  // Afficher les paramètres principaux
  if (TOTAL_HEIGHT > 8) {
    display.setTextColor(myGREEN);
    String patternText = "S:" + String(config.scanTypeName);
    patternText += " M:" + String(config.muxPatternName);
    patternText += " D:" + String(config.muxDelay);
    patternText += " B:" + String(config.brightness);
    patternText += " R:" + String(config.rotation);
    // Réduire si trop long
    if (patternText.length() * 6 > TOTAL_WIDTH) {
      patternText = String(config.scanTypeName).substring(0, 3) + "+" + 
                   String(config.muxPatternName).substring(0, 3) + "+" +
                   String(config.muxDelay) + "+" + String(config.brightness) + "+" + String(config.rotation);
    }
    int patternWidth = patternText.length() * 6;
    int patternX = (TOTAL_WIDTH - patternWidth) / 2;
    if (patternX < 0) patternX = 0;
    display.setCursor(patternX, 8);
    display.print(patternText);
  }

  // Dessiner des motifs pour tester la qualité
  int size = 2;
  for (int i = 0; i < TOTAL_WIDTH/size/4; i++) {
    display.fillRect(i*size*4, TOTAL_HEIGHT-size*2, size, size, myRED);
    display.fillRect(i*size*4+size*2, TOTAL_HEIGHT-size*2, size, size, myBLUE);
    display.fillRect(i*size*4+size, TOTAL_HEIGHT-size, size, size, myGREEN);
    display.fillRect(i*size*4+size*3, TOTAL_HEIGHT-size, size, size, myMAGENTA);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 P10 RGB DISPLAY CONFIG TEST ===");
  Serial.printf("Configuration: %dx%d panels (%dx%d resolution)\n", 
                MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);

  // Configurer le premier test
  setNextTestConfig();

  // Informations sur le nombre total de tests
  int totalTests = NUM_SCAN_PATTERNS * NUM_SCAN_TYPES * NUM_MUX_PATTERNS * NUM_MUXDELAYS * NUM_BRIGHTNESSES * NUM_ROTATIONS;
  Serial.printf("Total configurations à tester: %d\n", totalTests);
  Serial.printf("Durée de chaque test: %d secondes\n", TEST_DURATION / 1000);
  Serial.printf("Durée totale approximative: %d minutes\n", (totalTests * TEST_DURATION) / 60000);
  Serial.println("Commandes série disponibles :");
  Serial.println("  n : test suivant");
  Serial.println("  b : changer display.begin()");
  Serial.println("  s : changer scan pattern");
  Serial.println("  m : changer mux pattern");
  Serial.printf("  d : changer muxdelay (actuel : %d)\n", currentTest.muxDelay);
  Serial.println("  l : changer brightness");
  Serial.println("  r : changer rotation");
  Serial.println("  h : afficher l'aide");

  // Affichage amélioré pour le premier test
  Serial.println("\n--- DÉBUT DU TEST ---");
  Serial.printf("[CONFIG] begin: %s (%d), scan: %s, mux: %s, muxdelay: %d, brightness: %d, rotation: %d\n",
                currentTest.beginName,
                currentTest.beginPattern,
                currentTest.scanTypeName,
                currentTest.muxPatternName,
                currentTest.muxDelay,
                currentTest.brightness,
                currentTest.rotation);
  Serial.println("---------------------\n");

  initDisplay(currentTest);

  testStartTime = millis();
  testRunning = true;
}

void loop() {
  // Vérifier s'il y a des commandes sur le port série
  if (Serial.available()) {
    char cmd = Serial.read();
    bool paramChanged = false;
    switch (cmd) {
      case 'n':
      case 'N':
        skipToNextTest = true;
        Serial.println("Passage manuel au test suivant...");
        break;
      case 'b':
      case 'B':
        {
          int beginIndex = 0;
          for (int i = 0; i < NUM_SCAN_PATTERNS; i++) {
            if (SCAN_PATTERNS[i] == currentTest.beginPattern) beginIndex = i;
          }
          beginIndex = (beginIndex + 1) % NUM_SCAN_PATTERNS;
          currentTest.beginPattern = SCAN_PATTERNS[beginIndex];
          currentTest.beginName = SCAN_NAMES[beginIndex];
          paramChanged = true;
          Serial.printf("Changement display.begin() -> %s (%d)\n", currentTest.beginName, currentTest.beginPattern);
        }
        break;
      case 's':
      case 'S':
        {
          int scanIndex = 0;
          for (int i = 0; i < NUM_SCAN_TYPES; i++) {
            if (SCAN_TYPES[i] == currentTest.scanType) scanIndex = i;
          }
          scanIndex = (scanIndex + 1) % NUM_SCAN_TYPES;
          currentTest.scanType = SCAN_TYPES[scanIndex];
          currentTest.scanTypeName = SCAN_TYPE_NAMES[scanIndex];
          paramChanged = true;
          Serial.printf("Changement scan pattern -> %s\n", currentTest.scanTypeName);
        }
        break;
      case 'm':
      case 'M':
        {
          int muxIndex = 0;
          for (int i = 0; i < NUM_MUX_PATTERNS; i++) {
            if (MUX_PATTERNS[i] == currentTest.muxPattern) muxIndex = i;
          }
          muxIndex = (muxIndex + 1) % NUM_MUX_PATTERNS;
          currentTest.muxPattern = MUX_PATTERNS[muxIndex];
          currentTest.muxPatternName = MUX_PATTERN_NAMES[muxIndex];
          paramChanged = true;
          Serial.printf("Changement mux pattern -> %s\n", currentTest.muxPatternName);
        }
        break;
      case 'd':
      case 'D':
        {
          int muxDelayIndex = 0;
          for (int i = 0; i < NUM_MUXDELAYS; i++) {
            if (MUXDELAYS[i] == currentTest.muxDelay) muxDelayIndex = i;
          }
          muxDelayIndex = (muxDelayIndex + 1) % NUM_MUXDELAYS;
          currentTest.muxDelay = MUXDELAYS[muxDelayIndex];
          paramChanged = true;
          Serial.printf("Changement muxdelay -> %d\n", currentTest.muxDelay);
        }
        break;
      case 'l':
      case 'L':
        {
          int brightIndex = 0;
          for (int i = 0; i < NUM_BRIGHTNESSES; i++) {
            if (BRIGHTNESSES[i] == currentTest.brightness) brightIndex = i;
          }
          brightIndex = (brightIndex + 1) % NUM_BRIGHTNESSES;
          currentTest.brightness = BRIGHTNESSES[brightIndex];
          paramChanged = true;
          Serial.printf("Changement brightness -> %d\n", currentTest.brightness);
        }
        break;
      case 'r':
      case 'R':
        {
          int rotIndex = 0;
          for (int i = 0; i < NUM_ROTATIONS; i++) {
            if (ROTATIONS[i] == currentTest.rotation) rotIndex = i;
          }
          rotIndex = (rotIndex + 1) % NUM_ROTATIONS;
          currentTest.rotation = ROTATIONS[rotIndex];
          paramChanged = true;
          Serial.printf("Changement rotation -> %d\n", currentTest.rotation);
        }
        break;
      case 'h':
      case 'H':
        Serial.println("\nCommandes série disponibles :");
        Serial.println("  n : test suivant");
        Serial.println("  b : changer display.begin()");
        Serial.println("  s : changer scan pattern");
        Serial.println("  m : changer mux pattern");
        Serial.printf("  d : changer muxdelay (actuel : %d)\n", currentTest.muxDelay);
        Serial.println("  l : changer brightness");
        Serial.println("  r : changer rotation");
        Serial.println("  h : afficher l'aide\n");
        break;
      default:
        Serial.println("\nCommande inconnue !");
        Serial.println("Commandes série disponibles :");
        Serial.println("  n : test suivant");
        Serial.println("  b : changer display.begin()");
        Serial.println("  s : changer scan pattern");
        Serial.println("  m : changer mux pattern");
        Serial.printf("  d : changer muxdelay (actuel : %d)\n", currentTest.muxDelay);
        Serial.println("  l : changer brightness");
        Serial.println("  r : changer rotation");
        Serial.println("  h : afficher l'aide\n");
        break;
    }
    if (paramChanged) {
      initDisplay(currentTest);
      Serial.println("[CONFIG MANUELLE] Paramètre modifié, nouvelle configuration :");
      Serial.printf("[CONFIG] begin: %s (%d), scan: %s, mux: %s, muxdelay: %d, brightness: %d, rotation: %d\n",
                    currentTest.beginName,
                    currentTest.beginPattern,
                    currentTest.scanTypeName,
                    currentTest.muxPatternName,
                    currentTest.muxDelay,
                    currentTest.brightness,
                    currentTest.rotation);
      Serial.println("---------------------\n");
    }
  }
  
  // Passer au test suivant si le temps est écoulé ou si l'utilisateur le demande
  if (testRunning && ((millis() - testStartTime >= TEST_DURATION) || skipToNextTest)) {
    skipToNextTest = false;
    
    // Passer au test suivant
    setNextTestConfig();

    Serial.println("\n--- CHANGEMENT DE CONFIGURATION ---");
    Serial.printf("[CONFIG] begin: %s (%d), scan: %s, mux: %s, muxdelay: %d, brightness: %d, rotation: %d\n",
                  currentTest.beginName,
                  currentTest.beginPattern,
                  currentTest.scanTypeName,
                  currentTest.muxPatternName,
                  currentTest.muxDelay,
                  currentTest.brightness,
                  currentTest.rotation);
    Serial.println("-----------------------------------\n");

    initDisplay(currentTest);

    testStartTime = millis();
  }
  
  // Animation simple pour voir la fluidité
  static int counter = 0;
  static unsigned long lastTime = 0;
  
  if (millis() - lastTime > 100) {
    lastTime = millis();
    counter = (counter + 1) % TOTAL_WIDTH;
    
    // Effacer l'ancienne ligne
    for (int y = 0; y < TOTAL_HEIGHT; y++) {
      display.drawPixel((counter + TOTAL_WIDTH - 1) % TOTAL_WIDTH, y, myBLACK);
    }
    
    // Dessiner la nouvelle ligne
    for (int y = 0; y < TOTAL_HEIGHT; y++) {
      display.drawPixel(counter, y, myCYAN);
    }
  }
}

// Calcule la configuration du prochain test
void setNextTestConfig() {
  // Calculer les indices pour les différents paramètres
  int beginIndex = currentConfigIndex % NUM_SCAN_PATTERNS;
  int scanTypeIndex = (currentConfigIndex / NUM_SCAN_PATTERNS) % NUM_SCAN_TYPES;
  int muxPatternIndex = (currentConfigIndex / (NUM_SCAN_PATTERNS * NUM_SCAN_TYPES)) % NUM_MUX_PATTERNS;
  int muxDelayIndex = (currentConfigIndex / (NUM_SCAN_PATTERNS * NUM_SCAN_TYPES * NUM_MUX_PATTERNS)) % NUM_MUXDELAYS;
  int brightnessIndex = (currentConfigIndex / (NUM_SCAN_PATTERNS * NUM_SCAN_TYPES * NUM_MUX_PATTERNS * NUM_MUXDELAYS)) % NUM_BRIGHTNESSES;
  int rotationIndex = (currentConfigIndex / (NUM_SCAN_PATTERNS * NUM_SCAN_TYPES * NUM_MUX_PATTERNS * NUM_MUXDELAYS * NUM_BRIGHTNESSES)) % NUM_ROTATIONS;

  // Configurer le test actuel
  currentTest.beginPattern = SCAN_PATTERNS[beginIndex];
  currentTest.beginName = SCAN_NAMES[beginIndex];
  currentTest.scanType = SCAN_TYPES[scanTypeIndex];
  currentTest.scanTypeName = SCAN_TYPE_NAMES[scanTypeIndex];
  currentTest.muxPattern = MUX_PATTERNS[muxPatternIndex];
  currentTest.muxPatternName = MUX_PATTERN_NAMES[muxPatternIndex];
  currentTest.muxDelay = MUXDELAYS[muxDelayIndex];
  currentTest.brightness = BRIGHTNESSES[brightnessIndex];
  currentTest.rotation = ROTATIONS[rotationIndex];

  // Passer à la configuration suivante
  currentConfigIndex = (currentConfigIndex + 1) % (NUM_SCAN_PATTERNS * NUM_SCAN_TYPES * NUM_MUX_PATTERNS * NUM_MUXDELAYS * NUM_BRIGHTNESSES * NUM_ROTATIONS);
}
