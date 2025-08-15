/**
 * ESP32 P10 RGB Fullscreen Countdown avec Interface Web
 * Compte à rebours plein écran avec réglages via interface web
 * 
 * Ce programme affiche un compte à rebours en plein écran et permet de configurer :
 * - La date/heure cible du compte à rebours
 * - La couleur du texte
 * - La taille de police
 * 
 * L'interface web est accessible à l'adresse http://ip_de_l'esp32/
 * 
 * Auteur: GitHub Copilot
 * Date: Août 2025
 */

#define PxMATRIX_SPI_FREQUENCY 10000000

// Option d'optimisation démarrage : définir FAST_BOOT pour réduire fortement les délais init
#ifndef FAST_BOOT
#define FAST_BOOT 1
#endif

#if FAST_BOOT
#define BOOT_DELAY(ms) ((void)0)
#define BOOT_SPLASH_MS 300   // durée splash réduite
#define WIFI_STEP_DELAY(ms) vTaskDelay(pdMS_TO_TICKS(5))
#else
#define BOOT_DELAY(ms) delay(ms)
#define BOOT_SPLASH_MS 2000
#define WIFI_STEP_DELAY(ms) delay(ms)
#endif

#include <Arduino.h>
#include <PxMatrix.h>
#include <RTClib.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Pins pour la matrice LED
#define P_LAT 5
#define P_A   19
#define P_B   23
#define P_C   18
#define P_OE  4

// Définition des priorités des tâches (ajustées pour éviter les conflits WiFi)
#define TASK_DISPLAY_PRIORITY      2
#define TASK_WEBSERVER_PRIORITY    1  
#define TASK_COUNTDOWN_PRIORITY    1
#define TASK_NETWORK_PRIORITY      3  // Priorité plus élevée pour le réseau

// Taille des stacks pour les tâches
#define TASK_DISPLAY_STACK     4096
#define TASK_WEBSERVER_STACK   4096
#define TASK_COUNTDOWN_STACK   2048
#define TASK_NETWORK_STACK     4096

// Handles pour les tâches
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t countdownTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

// Mutex pour protéger les ressources partagées
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t countdownMutex;
SemaphoreHandle_t preferencesMutex;

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

// Préférences
Preferences preferences;

// Configuration WiFi - Modifiez selon vos besoins
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Configuration Point d'accès (si pas de WiFi)
// DNS pour le portail captif
const byte DNS_PORT = 53;
DNSServer dnsServer;
const char* ap_ssid = "HOKA_CLOCK";
const char* ap_password = "hokahoka";


// Mode de fonctionnement WiFi
bool useStationMode = false; // true = se connecter au WiFi, false = créer un point d'accès

// Serveur web
WebServer server(80);

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

// Prototypes des fonctions
void IRAM_ATTR display_updater();

// Utilitaire : copie tronquée en respectant les limites UTF-8 (nombre de caractères et non octets)
// maxChars : nombre maximum de caractères (glyphes) à conserver (excluant le nul final)
// destSize : taille du buffer destination (incluant place pour nul)
size_t utf8SafeCopyTruncate(const String &src, char *dest, size_t destSize, size_t maxChars) {
  if (destSize == 0) return 0;
  const uint8_t *bytes = (const uint8_t*)src.c_str();
  size_t i = 0;       // index source en octets
  size_t o = 0;       // index destination
  size_t chars = 0;   // compte de caractères
  while (bytes[i] != '\0' && chars < maxChars && o + 1 < destSize) {
    uint8_t c = bytes[i];
    size_t seqLen = 1;
    if ((c & 0x80) == 0x00) seqLen = 1;                // 0xxxxxxx
    else if ((c & 0xE0) == 0xC0) seqLen = 2;           // 110xxxxx
    else if ((c & 0xF0) == 0xE0) seqLen = 3;           // 1110xxxx
    else if ((c & 0xF8) == 0xF0) seqLen = 4;           // 11110xxx
    else { // octet invalide -> arrêter
      break;
    }
    // Vérifier que la séquence tient dans la source
    for (size_t k = 1; k < seqLen; ++k) {
      uint8_t nc = bytes[i + k];
      if ((nc & 0xC0) != 0x80) { // pas une continuation valide
        seqLen = 1; // tronquer au premier octet
        break;
      }
    }
    // Vérifier que ça rentre dans dest
    if (o + seqLen + 1 >= destSize) break;
    // Copier
    for (size_t k = 0; k < seqLen; ++k) {
      dest[o++] = (char)bytes[i + k];
    }
    i += seqLen;
    chars++;
  }
  dest[o] = '\0';
  return o; // octets copiés
}

// Prototypes des tâches
void DisplayTask(void * parameter);
void CountdownTask(void * parameter);
void NetWebTask(void * parameter);

// Variables pour le countdown
DateTime countdownTarget;
bool countdownExpired = false;
bool blinkLastSeconds = false;  // Clignotement pour les 10 dernières secondes
bool blinkState = true;
unsigned long lastBlinkTime = 0;
const int BLINK_INTERVAL = 500;  // Intervalle de clignotement 0.5 seconde

// Variable pour indiquer qu'il faut sauvegarder les paramètres
volatile bool saveRequested = false;
volatile unsigned long saveRequestTime = 0;

// Paramètres configurables via l'interface web
int countdownYear = 2025;
int countdownMonth = 12;
int countdownDay = 31;
int countdownHour = 23;
int countdownMinute = 59;
int countdownSecond = 0;
char countdownTitle[51] = "COUNTDOWN";
int countdownTextSize = 2; // 1-3
int countdownColorR = 0;
int countdownColorG = 255;
int countdownColorB = 0;
int fontIndex = 0; // 0=Default, 1=Sans, 2=Sans Bold, 3=Mono
uint16_t countdownColor;

// Format d'affichage (0=jours, 1=heures, 2=minutes, 3=secondes uniquement)
int displayFormat = 0;

// HTML Page
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 P10 Countdown Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
            color: #333;
        }
        h1 {
            color: #009688;
            text-align: center;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }
        input, select {
            width: 100%;
            padding: 8px;
            box-sizing: border-box;
            border: 1px solid #ddd;
            border-radius: 4px;
        }
        input[type="color"] {
            height: 40px;
        }
        button {
            background-color: #009688;
            color: white;
            border: none;
            padding: 10px 15px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
            display: block;
            width: 100%;
            margin-top: 10px;
        }
        button:hover {
            background-color: #00796b;
        }
        .section {
            border-top: 1px solid #eee;
            padding-top: 15px;
            margin-top: 15px;
        }
        .color-picker {
            display: flex;
            align-items: center;
        }
        .color-preview {
            width: 30px;
            height: 30px;
            margin-left: 10px;
            border: 1px solid #ccc;
        }
        #countdown-preview {
            text-align: center;
            font-size: 22px;
            margin: 15px 0;
            padding: 10px;
            border-radius: 4px;
            background-color: #000;
            color: #0f0;
        }
        .footer {
            text-align: center;
            margin-top: 20px;
            font-size: 14px;
            color: #777;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Configuration du Compte à Rebours</h1>
        
        <div id="countdown-preview">12:34:56</div>
        
        <form action="/settings" method="POST" id="settingsForm">
            // Champ clé supprimé
            
            <div class="section">
                <h2>Date et Heure Cible</h2>
                <div class="form-group">
                    <label for="title">Titre du Compte à Rebours:</label>
                    <input type="text" id="title" name="title" maxlength="50" required>
                </div>
                <div class="form-group">
                    <label for="date">Date:</label>
                    <input type="date" id="date" name="date" required>
                </div>
                <div class="form-group">
                    <label for="time">Heure:</label>
                    <input type="time" id="time" name="time" required step="1">
                </div>
            </div>
            
            <div class="section">
                <h2>Apparence</h2>
                <div class="form-group">
                    <label for="fontIndex">Police:</label>
                    <select id="fontIndex" name="fontIndex">
                        <option value="0">Standard</option>
                        <option value="1">Sans Serif</option>
                        <option value="2">Sans Serif Bold</option>
                        <option value="3">Monospace</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="textSize">Taille du texte:</label>
                    <select id="textSize" name="textSize">
                        <option value="1">Petite</option>
                        <option value="2" selected>Moyenne</option>
                        <option value="3">Grande</option>
                    </select>
                </div>
                <div class="form-group">
                    <label for="colorPicker">Couleur:</label>
                    <div class="color-picker">
                        <input type="color" id="colorPicker" value="#00ff00">
                        <div class="color-preview" id="colorPreview"></div>
                    </div>
                    <input type="hidden" id="colorR" name="colorR" value="0">
                    <input type="hidden" id="colorG" name="colorG" value="255">
                    <input type="hidden" id="colorB" name="colorB" value="0">
                </div>
            </div>
            
            <button type="submit">Enregistrer</button>
            <button type="button" onclick="resetCountdown()">Réinitialiser</button>
        </form>
        
        <div class="footer">
            ESP32 P10 RGB Fullscreen Countdown &copy; 2025
        </div>
    </div>

    <script>
        // Fonctions pour convertir couleur hex en RGB
        function hexToRgb(hex) {
            // Supprimer le # si présent
            hex = hex.replace('#', '');
            
            // Convertir en RGB
            const r = parseInt(hex.substring(0, 2), 16);
            const g = parseInt(hex.substring(2, 4), 16);
            const b = parseInt(hex.substring(4, 6), 16);
            
            return {r, g, b};
        }
        
        // Fonctions pour convertir RGB en hex
        function rgbToHex(r, g, b) {
            return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
        }
        
        // Mettre à jour les champs cachés RGB quand la couleur change
        document.getElementById('colorPicker').addEventListener('input', function() {
            const color = hexToRgb(this.value);
            document.getElementById('colorR').value = color.r;
            document.getElementById('colorG').value = color.g;
            document.getElementById('colorB').value = color.b;
            document.getElementById('colorPreview').style.backgroundColor = this.value;
            document.getElementById('countdown-preview').style.color = this.value;
        });
        
        // Mettre à jour la prévisualisation du compte à rebours
        document.getElementById('textSize').addEventListener('change', function() {
            const size = this.value;
            document.getElementById('countdown-preview').style.fontSize = (16 + (size * 6)) + 'px';
        });
        
        // Fonction pour réinitialiser le compte à rebours
        function resetCountdown() {
            if (confirm("Voulez-vous vraiment réinitialiser le compte à rebours ?")) {
                document.getElementById('settingsForm').action = "/reset";
                document.getElementById('settingsForm').submit();
            }
        }
        
        // Charger les valeurs actuelles au chargement de la page
        window.onload = function() {
            // Définir la date courante + 1 jour comme valeur par défaut
            const now = new Date();
            const tomorrow = new Date(now);
            tomorrow.setDate(tomorrow.getDate() + 1);
            
            const dateStr = tomorrow.toISOString().split('T')[0];
            document.getElementById('date').value = dateStr;
            
            const timeStr = '23:59:59';
            document.getElementById('time').value = timeStr;
            
            // Charger les valeurs depuis l'ESP32
            fetch('/getSettings')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('title').value = data.title;
                    
                    // Format date YYYY-MM-DD
                    const dateStr = `${data.year}-${String(data.month).padStart(2, '0')}-${String(data.day).padStart(2, '0')}`;
                    document.getElementById('date').value = dateStr;
                    
                    // Format heure HH:MM:SS
                    const timeStr = `${String(data.hour).padStart(2, '0')}:${String(data.minute).padStart(2, '0')}:${String(data.second).padStart(2, '0')}`;
                    document.getElementById('time').value = timeStr;
                    
                    document.getElementById('fontIndex').value = data.fontIndex;
                    document.getElementById('textSize').value = data.textSize;
                    
                    // Mettre à jour la couleur
                    const hexColor = rgbToHex(data.colorR, data.colorG, data.colorB);
                    document.getElementById('colorPicker').value = hexColor;
                    document.getElementById('colorPreview').style.backgroundColor = hexColor;
                    document.getElementById('countdown-preview').style.color = hexColor;
                    
                    document.getElementById('colorR').value = data.colorR;
                    document.getElementById('colorG').value = data.colorG;
                    document.getElementById('colorB').value = data.colorB;
                    
                    // Mettre à jour le preview
                    document.getElementById('countdown-preview').style.fontSize = (16 + (data.textSize * 6)) + 'px';
                })
                .catch(error => {
                    console.error('Erreur:', error);
                });
        };
    </script>
</body>
</html>
)rawliteral";

// Note: La fonction display_update_enable() a été intégrée dans setup()
// pour une meilleure gestion avec FreeRTOS

// Calcul du temps restant
void updateCountdown(int &days, int &hours, int &minutes, int &seconds) {
  DateTime now = rtc.now();
  
  // Vérifier si le countdown est expiré
  if (now >= countdownTarget) {
    countdownExpired = true;
  // Arrêter tout clignotement résiduel une fois expiré
  blinkLastSeconds = false;
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

// Obtenir la police en fonction de l'index
const GFXfont* getFontByIndex(int index) {
  switch (index) {
    case 1:
      return &FreeSans9pt7b;
    case 2:
      return &FreeSansBold9pt7b;
    case 3:
      return &FreeMono9pt7b;
    default:
      return NULL; // Police par défaut
  }
}

// Affichage du compte à rebours en plein écran
void displayFullscreenCountdown(int days, int hours, int minutes, int seconds) {
  // --- Cache layout ---
  static char lastText[24] = "";
  static bool lastExpired = false;
  static int lastFontIndex = -1;
  static int lastTextSizeTier = -1;
  static int16_t cachedX = 0, cachedY = 0;
  static uint8_t cachedSetSize = 1;
  static bool cachedIsEndMsg = false;

  // Choisir la couleur (adaptée clignotement)
  uint16_t displayColor = countdownColor;
  if (blinkLastSeconds) {
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
      lastBlinkTime = currentTime;
      blinkState = !blinkState;
    }
    // Clignote maintenant entre la couleur choisie et noir (plus de rouge forcé)
    displayColor = blinkState ? countdownColor : myBLACK;
  }

  // Préparer le texte cible
  char currentText[24];
  if (countdownExpired) {
    strncpy(currentText, countdownTitle, sizeof(currentText)-1);
    currentText[sizeof(currentText)-1] = '\0';
  } else {
    switch (displayFormat) {
      case 0:  snprintf(currentText, sizeof(currentText), "%dd %02d:%02d", days, hours, minutes); break;
      case 1:  snprintf(currentText, sizeof(currentText), "%02d:%02d:%02d", hours, minutes, seconds); break;
      case 2:  snprintf(currentText, sizeof(currentText), "%02d:%02d", minutes, seconds); break;
      default: snprintf(currentText, sizeof(currentText), "%02d", seconds); break;
    }
  }

  bool needRecalc = countdownExpired != lastExpired || (strcmp(currentText, lastText) != 0) || fontIndex != lastFontIndex || countdownTextSize != lastTextSizeTier;

  if (needRecalc) {
    const GFXfont *font = getFontByIndex(fontIndex);
    display.setFont(font);
    if (!font) {
      int maxFittingSize = 1;
      for (int s = 1; s <= 5; s++) {
        display.setTextSize(s);
        int w = getTextWidth(currentText, font);
        int h = 8 * s;
        if (w < TOTAL_WIDTH && h < TOTAL_HEIGHT) maxFittingSize = s; else break;
      }
      int desiredTier = countdownTextSize;
      int chosen = maxFittingSize;
      if (desiredTier == 1) chosen = max(1, maxFittingSize - 2);
      else if (desiredTier == 2) chosen = max(1, maxFittingSize - 1);
      display.setTextSize(chosen);
      cachedSetSize = chosen;
    } else {
      display.setTextSize(1);
      cachedSetSize = 1;
    }
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(currentText, 0, 0, &x1, &y1, &w, &h);
    cachedX = (TOTAL_WIDTH - w) / 2 - x1;
    cachedY = (TOTAL_HEIGHT - h) / 2 - y1;
    strncpy(lastText, currentText, sizeof(lastText));
    lastExpired = countdownExpired;
    lastFontIndex = fontIndex;
    lastTextSizeTier = countdownTextSize;
    cachedIsEndMsg = countdownExpired;
  } else {
    const GFXfont *font = getFontByIndex(fontIndex);
    display.setFont(font);
    display.setTextSize(cachedSetSize);
  }

  // Section critique minimale : effacement + écriture tampon (réduit blocage ISR)
  portENTER_CRITICAL(&timerMux);
  display.clearDisplay();
  // Utiliser toujours la couleur choisie (ou clignotement) même pour le texte final
  display.setTextColor(displayColor);
  display.setCursor(cachedX, cachedY);
  display.print(lastText);
  portEXIT_CRITICAL(&timerMux);
}

// Chargement des paramètres depuis la mémoire flash (version thread-safe)
void loadSettings() {
  // Utiliser un timeout pour éviter les blocages
  if (preferencesMutex == NULL || xSemaphoreTake(preferencesMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire preferences mutex - using defaults");
    return;
  }
  // Prendre aussi le mutex countdown pour cohérence des paramètres
  if (countdownMutex == NULL || xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire countdown mutex - using defaults");
    xSemaphoreGive(preferencesMutex);
    return;
  }
  
  bool success = false;
  do {
    if (!preferences.begin("countdown", true)) {
      Serial.println("Failed to begin preferences for reading");
      break;
    }
    
    // Paramètres du countdown
    countdownYear = preferences.getInt("cd_Year", 2025);
    countdownMonth = preferences.getInt("cd_Month", 12);
    countdownDay = preferences.getInt("cd_Day", 31);
    countdownHour = preferences.getInt("cd_Hour", 23);
    countdownMinute = preferences.getInt("cd_Minute", 59);
    countdownSecond = preferences.getInt("cd_Second", 0);
    
    // Paramètres d'affichage
    countdownTextSize = preferences.getInt("textSize", 2);
    countdownColorR = preferences.getInt("colorR", 0);
    countdownColorG = preferences.getInt("colorG", 255);
    countdownColorB = preferences.getInt("colorB", 0);
    fontIndex = preferences.getInt("fontIndex", 0);
    
    String savedTitle = preferences.getString("cd_Title", "COUNTDOWN");
    strcpy(countdownTitle, savedTitle.c_str());
    
    preferences.end();
    success = true;
    
  } while(false);
  
  // Libérer le mutex
  xSemaphoreGive(preferencesMutex);
  xSemaphoreGive(countdownMutex);
  
  if (success) {
    Serial.println("Settings loaded successfully");
  } else {
    Serial.println("Using default settings");
  }
  
  // Mise à jour de la couleur
  countdownColor = display.color565(countdownColorR, countdownColorG, countdownColorB);
  
  // Mise à jour de la date cible
  countdownTarget = DateTime(countdownYear, countdownMonth, countdownDay, 
                           countdownHour, countdownMinute, countdownSecond);
}

// Sauvegarde des paramètres dans la mémoire flash (version thread-safe)
void saveSettings() {
  // Utiliser un timeout pour éviter les blocages
  if (preferencesMutex == NULL || xSemaphoreTake(preferencesMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire preferences mutex - settings not saved");
    return;
  }
  if (countdownMutex == NULL || xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire countdown mutex - settings not saved");
    xSemaphoreGive(preferencesMutex);
    return;
  }
  
  // Utiliser un try-catch style pour s'assurer que le mutex est libéré
  bool success = false;
  do {
    if (!preferences.begin("countdown", false)) {
      Serial.println("Failed to begin preferences");
      break;
    }
    
    // Paramètres du countdown
    preferences.putInt("cd_Year", countdownYear);
    preferences.putInt("cd_Month", countdownMonth);
    preferences.putInt("cd_Day", countdownDay);
    preferences.putInt("cd_Hour", countdownHour);
    preferences.putInt("cd_Minute", countdownMinute);
    preferences.putInt("cd_Second", countdownSecond);
    
    // Paramètres d'affichage
    preferences.putInt("textSize", countdownTextSize);
    preferences.putInt("colorR", countdownColorR);
    preferences.putInt("colorG", countdownColorG);
    preferences.putInt("colorB", countdownColorB);
    preferences.putInt("fontIndex", fontIndex);
    
    preferences.putString("cd_Title", String(countdownTitle));
    
    preferences.end();
    success = true;
    
  } while(false);
  
  // Libérer le mutex
  xSemaphoreGive(preferencesMutex);
  xSemaphoreGive(countdownMutex);
  
  if (success) {
    Serial.println("Settings saved successfully");
    // Mise à jour de la couleur
    countdownColor = display.color565(countdownColorR, countdownColorG, countdownColorB);
    
    // Mise à jour de la date cible
    countdownTarget = DateTime(countdownYear, countdownMonth, countdownDay, 
                             countdownHour, countdownMinute, countdownSecond);
  } else {
    Serial.println("Failed to save settings");
  }
}

// Connexion WiFi
void connecting_To_WiFi() {
  Serial.println("\n-------------WIFI mode (STA async)");
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false); // éviter écritures flash lentes
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  uint32_t startAttempt = millis();
  const uint32_t timeoutMs = 8000; // timeout réduit
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < timeoutMs) {
    WIFI_STEP_DELAY(50);
    Serial.print('.');
    // Donner du temps au scheduler (éviter WDT)
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected in %lu ms\n", (unsigned long)(millis() - startAttempt));
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connect timeout -> fallback AP");
    useStationMode = false;
  }
  Serial.println("-------------");
}

// Configuration du point d'accès
void set_ESP32_Access_Point() {
  Serial.println("\n-------------");
  Serial.println("WIFI mode : AP");
  WiFi.mode(WIFI_AP);
  Serial.println("-------------");

  Serial.println("Configuring Access Point...");
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress local_ip(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  Serial.print("SSID : "); Serial.println(ap_ssid);
  Serial.print("AP IP : "); Serial.println(WiFi.softAPIP());
}

// Gestionnaire de la page principale
void handleRoot() {
  server.send(200, "text/html", MAIN_page);
}

// Gestionnaire des paramètres actuels en JSON
void handleGetSettings() {
  String json = "{";
  json += "\"title\":\"" + String(countdownTitle) + "\",";
  json += "\"year\":" + String(countdownYear) + ",";
  json += "\"month\":" + String(countdownMonth) + ",";
  json += "\"day\":" + String(countdownDay) + ",";
  json += "\"hour\":" + String(countdownHour) + ",";
  json += "\"minute\":" + String(countdownMinute) + ",";
  json += "\"second\":" + String(countdownSecond) + ",";
  json += "\"textSize\":" + String(countdownTextSize) + ",";
  json += "\"colorR\":" + String(countdownColorR) + ",";
  json += "\"colorG\":" + String(countdownColorG) + ",";
  json += "\"colorB\":" + String(countdownColorB) + ",";
  json += "\"fontIndex\":" + String(fontIndex);
  json += "}";
  
  server.send(200, "application/json", json);
}

// Gestionnaire des paramètres
void handleSettings() {
  Serial.println("\n-------------Settings");

  // Récupérer les valeurs de la requête
  String title = server.hasArg("title") ? server.arg("title") : String("");
  String dateStr = server.hasArg("date") ? server.arg("date") : String("");
  String timeStr = server.hasArg("time") ? server.arg("time") : String("");

  if (dateStr.length() < 10 || timeStr.length() < 5) {
    server.send(400, "text/plain", "Parametres invalides");
    return;
  }
  
  // Parser la date (format YYYY-MM-DD)
  // Protéger modifications
  if (countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(200))) {
    countdownYear = dateStr.substring(0, 4).toInt();
    countdownMonth = dateStr.substring(5, 7).toInt();
    countdownDay = dateStr.substring(8, 10).toInt();
  
  // Parser l'heure (format HH:MM:SS)
  countdownHour = timeStr.substring(0, 2).toInt();
  countdownMinute = timeStr.substring(3, 5).toInt();
  countdownSecond = timeStr.length() > 5 ? timeStr.substring(6, 8).toInt() : 0;
  
  // Récupérer les autres paramètres
  countdownTextSize = server.hasArg("textSize") ? server.arg("textSize").toInt() : countdownTextSize;
  countdownColorR = server.hasArg("colorR") ? server.arg("colorR").toInt() : countdownColorR;
  countdownColorG = server.hasArg("colorG") ? server.arg("colorG").toInt() : countdownColorG;
  countdownColorB = server.hasArg("colorB") ? server.arg("colorB").toInt() : countdownColorB;
  fontIndex = server.hasArg("fontIndex") ? server.arg("fontIndex").toInt() : fontIndex;
  
  // Limiter les valeurs
  if (countdownTextSize < 1) countdownTextSize = 1;
  if (countdownTextSize > 3) countdownTextSize = 3;
  
  if (fontIndex < 0) fontIndex = 0;
  if (fontIndex > 3) fontIndex = 3;
  
  // Mettre à jour le titre
    if (title.length() > 0) {
      utf8SafeCopyTruncate(title, countdownTitle, sizeof(countdownTitle), 50);
    }
  
  // Valider la date
  if (countdownMonth < 1) countdownMonth = 1;
  if (countdownMonth > 12) countdownMonth = 12;
  if (countdownDay < 1) countdownDay = 1;
  if (countdownDay > 31) countdownDay = 31;
  
  // Valider l'heure
    if (countdownHour > 23) countdownHour = 23;
    if (countdownMinute > 59) countdownMinute = 59;
    if (countdownSecond > 59) countdownSecond = 59;

    // Recalculer couleur immédiate (affichage plus réactif)
    countdownColor = display.color565(countdownColorR, countdownColorG, countdownColorB);
    // Mettre à jour cible localement pour affichage avant sauvegarde
    countdownTarget = DateTime(countdownYear, countdownMonth, countdownDay, 
                               countdownHour, countdownMinute, countdownSecond);
    xSemaphoreGive(countdownMutex);
  } else {
    Serial.println("Could not get countdown mutex in handleSettings");
  }
  
  // Sauvegarder les paramètres (demander la sauvegarde plutôt que de la faire directement)
  saveRequested = true;
  saveRequestTime = millis();
  
  Serial.println("Settings updated:");
  Serial.printf("Target: %d-%02d-%02d %02d:%02d:%02d\n", 
                countdownYear, countdownMonth, countdownDay,
                countdownHour, countdownMinute, countdownSecond);
  Serial.printf("Title: %s\n", countdownTitle);
  Serial.printf("Text Size: %d, Font: %d\n", countdownTextSize, fontIndex);
  Serial.printf("Color (RGB): %d,%d,%d\n", countdownColorR, countdownColorG, countdownColorB);
  
  // Répondre avec une redirection vers la page principale
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// Gestionnaire de reset
void handleReset() {
  // Vérification clé supprimée

  // Réinitialiser aux valeurs par défaut
  if (countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(200))) {
    countdownYear = 2025;
    countdownMonth = 12;
    countdownDay = 31;
    countdownHour = 23;
    countdownMinute = 59;
    countdownSecond = 0;
    strcpy(countdownTitle, "COUNTDOWN");
    countdownTextSize = 2;
    countdownColorR = 0;
    countdownColorG = 255;
    countdownColorB = 0;
    fontIndex = 0;
    countdownColor = display.color565(countdownColorR, countdownColorG, countdownColorB);
    countdownTarget = DateTime(countdownYear, countdownMonth, countdownDay, 
                               countdownHour, countdownMinute, countdownSecond);
    xSemaphoreGive(countdownMutex);
  }
  
  // Sauvegarder les paramètres (demander la sauvegarde plutôt que de la faire directement)
  saveRequested = true;
  saveRequestTime = millis();
  
  Serial.println("Settings reset to defaults");
  
  // Répondre avec une redirection vers la page principale
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// Configuration et démarrage du serveur
void prepare_and_start_The_Server() {
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/settings", HTTP_POST, handleSettings);
  server.on("/getSettings", handleGetSettings);
  server.on("/reset", HTTP_POST, handleReset);
  server.onNotFound([]() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); });
  server.begin();
  Serial.println("HTTP server started (fast)");
  if (useStationMode) {
    Serial.print("URL: http://"); Serial.println(WiFi.localIP());
  } else {
    Serial.print("AP URL: http://"); Serial.println(WiFi.softAPIP());
  }
}

// Fonction de callback pour le timer d'affichage
void IRAM_ATTR display_updater() {
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Activation/désactivation du timer d'affichage
void display_update_enable(bool is_enable) {
  if (is_enable) {
    if (timer == nullptr) {
      timer = timerBegin(0, 80, true);
      if (timer != nullptr) {
        timerAttachInterrupt(timer, &display_updater, true);
        timerAlarmWrite(timer, 4000, true);
        timerAlarmEnable(timer);
        Serial.println("Display timer enabled");
      } else {
        Serial.println("Failed to initialize display timer");
      }
    } else {
      Serial.println("Display timer already enabled");
    }
  } else {
    if (timer != nullptr) {
      timerAlarmDisable(timer);
      timerDetachInterrupt(timer);
      timerEnd(timer);
      timer = nullptr;
      Serial.println("Display timer disabled");
    }
  }
}

void setup() {
  // Éviter grosse pause initiale; laisser hardware se stabiliser rapidement
  BOOT_DELAY(100);
  Serial.begin(115200);
  Serial.println("\n=== ESP32 P10 RGB FULLSCREEN COUNTDOWN ===");
  Serial.printf("Configuration: %dx%d panels (%dx%d total resolution)\n", 
                MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);
  
  // Création des mutex
  displayMutex = xSemaphoreCreateMutex();
  countdownMutex = xSemaphoreCreateMutex();
  preferencesMutex = xSemaphoreCreateMutex();
  
  if (displayMutex == NULL || countdownMutex == NULL || preferencesMutex == NULL) {
    Serial.println("Error creating mutexes");
    while(1);
  }
  
  // NE PAS activer le timer ici - attendre après le WiFi et les tâches
  
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
  BOOT_DELAY(20);
  
  // Luminosité adaptée au nombre de panneaux
  int brightness = 150;
  if (MATRIX_PANELS_X > 2) brightness = 100;
  if (MATRIX_PANELS_X > 4) brightness = 80;
  if (MATRIX_PANELS_X > 6) brightness = 60;
  
  display.setBrightness(brightness);
  Serial.printf("Brightness set to: %d\n", brightness);
  
  display.setTextWrap(false);
  display.setRotation(0);
  
  // Chargement des paramètres
  loadSettings();
  
  // Splash écran réduit
  display.clearDisplay();
  display.setTextColor(countdownColor);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("BOOT");
  display.setCursor(0, 8);
  display.print("COUNTDOWN");
  BOOT_DELAY(BOOT_SPLASH_MS);
  display.clearDisplay();
  
  // Configuration WiFi initiale
  if (useStationMode) {
    connecting_To_WiFi();
    if (!useStationMode) {
      set_ESP32_Access_Point();
    }
  } else {
    set_ESP32_Access_Point();
  }

  // Démarrage du serveur web
  prepare_and_start_The_Server();
  
  Serial.println("Creating FreeRTOS tasks (fast boot)...");
  
  // Création des tâches FreeRTOS avec une meilleure distribution
  BaseType_t result;
  
  // Tâche d'affichage sur le Core 1 (isolé du WiFi)
  result = xTaskCreatePinnedToCore(
    DisplayTask,
    "DisplayTask",
    TASK_DISPLAY_STACK,
    NULL,
    TASK_DISPLAY_PRIORITY,
    &displayTaskHandle,
    1  // Core 1
  );
  if (result != pdPASS) Serial.println("Failed to create Display task");
  
  // Tâche combinée réseau + serveur web Core 0
  result = xTaskCreatePinnedToCore(
    NetWebTask,
    "NetWebTask",
    TASK_NETWORK_STACK,
    NULL,
    TASK_NETWORK_PRIORITY,
    &networkTaskHandle,
    0
  );
  if (result != pdPASS) Serial.println("Failed to create NetWeb task");
  
  // Tâche de calcul sur le Core 0
  result = xTaskCreatePinnedToCore(
    CountdownTask,
    "CountdownTask",
    TASK_COUNTDOWN_STACK,
    NULL,
    TASK_COUNTDOWN_PRIORITY,
    &countdownTaskHandle,
    0  // Core 0
  );
  if (result != pdPASS) Serial.println("Failed to create Countdown task");
  
  Serial.println("FreeRTOS tasks created successfully");
  Serial.println("Starting fullscreen countdown...");
  
  // ACTIVATION DU TIMER D'AFFICHAGE EN DERNIÈRE ÉTAPE
  Serial.println("Activating display timer earlier...");
  display_update_enable(true);
  BOOT_DELAY(50); // Stabilisation rapide
}

// Tâche d'affichage
void DisplayTask(void * parameter) {
  // Attendre que le système soit prêt
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  Serial.println("Display task started on core " + String(xPortGetCoreID()));
  
  int prevSeconds = -1, prevMinutes = -1, prevHours = -1, prevDays = -1;
  bool prevExpired = false;
  for(;;) {
    if(displayMutex != NULL && xSemaphoreTake(displayMutex, pdMS_TO_TICKS(10))) {
      int days=0, hours=0, minutes=0, seconds=0;
      if(countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(10))) {
        updateCountdown(days, hours, minutes, seconds);
        updateDisplayFormat(days, hours, minutes, seconds);
        prevExpired = countdownExpired; // snapshot
        xSemaphoreGive(countdownMutex);
      }
      bool secondChanged = (seconds != prevSeconds) || (minutes != prevMinutes) || (hours != prevHours) || (days != prevDays);
      bool needDraw = secondChanged || blinkLastSeconds || countdownExpired != prevExpired;
      if (needDraw) {
        displayFullscreenCountdown(days, hours, minutes, seconds);
        prevSeconds = seconds; prevMinutes = minutes; prevHours = hours; prevDays = days; prevExpired = countdownExpired;
      }
      xSemaphoreGive(displayMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(blinkLastSeconds ? 50 : 150)); // Moins de rafraîchissements hors clignotement
  }
}

// Tâche combinée réseau + web
void NetWebTask(void * parameter) {
  vTaskDelay(pdMS_TO_TICKS(1200));
  Serial.println("NetWeb task started on core " + String(xPortGetCoreID()));
  uint32_t lastReconnectCheck = 0;
  for(;;) {
    dnsServer.processNextRequest();
    server.handleClient();
    if (useStationMode) {
      uint32_t now = millis();
      if (WiFi.status() != WL_CONNECTED && now - lastReconnectCheck > 5000) {
        lastReconnectCheck = now;
        Serial.println("[NetWeb] WiFi lost -> reconnect");
        WiFi.reconnect();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// Tâche de gestion du compte à rebours
void CountdownTask(void * parameter) {
  // Attendre que le RTC soit prêt
  vTaskDelay(pdMS_TO_TICKS(1500));
  
  Serial.println("Countdown task started on core " + String(xPortGetCoreID()));
  
  for(;;) {
    // Vérifier s'il faut sauvegarder les paramètres (avec délai de sécurité)
  if (saveRequested && (millis() - saveRequestTime) > 1500) {  // Débounce 1.5s après dernière modif
      saveRequested = false;
      // Attendre un peu plus pour s'assurer que le contexte web est terminé
      vTaskDelay(pdMS_TO_TICKS(50));
      saveSettings();
      Serial.println("Settings saved from CountdownTask");
    }
    
    if(countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(100))) {
      DateTime now = rtc.now();
      if (now.isValid()) {  // Vérification de la validité de la date/heure
        if (now >= countdownTarget) {
          countdownExpired = true;
        }
      } else {
        Serial.println("RTC read error!");
      }
      xSemaphoreGive(countdownMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Ancienne NetworkTask fusionnée dans NetWebTask

void loop() {
  // Pause pour éviter les problèmes de watchdog
  // Les tâches FreeRTOS gèrent tout le travail
  vTaskDelay(pdMS_TO_TICKS(1000));
}
  