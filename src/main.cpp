/**
 * ESP32 P10 RGB 32x16 Digital Clock with Web Server
 * Compatible with PlatformIO
 * 
 * Hardware Requirements:
 * - ESP32 DEVKIT V1
 * - P10 RGB 32x16 HUB75 Panel
 * - DS3231 RTC Module
 * - 5V Power Supply
 * 
 * Libraries:
 * - Adafruit BusIO
 * - Adafruit GFX Library
 * - PxMatrix Library
 * - RTClib
 */

// Defines pour la fréquence SPI (réduire si du bruit apparaît sur l'affichage)
#define PxMATRIX_SPI_FREQUENCY 10000000

// Inclusion des bibliothèques
#include <Arduino.h>
#include <stdint.h>
#include <PxMatrix.h>
#include <RTClib.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <nvs_flash.h>
#include "PageIndex.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/timer.h>

// Prototypes des tâches FreeRTOS
void DisplayTask(void *pvParameters);
void WebServerTask(void *pvParameters);
void WiFiTask(void *pvParameters);

// Prototypes des gestionnaires web
void handleRoot();
void handleSettings();
void handleCaptivePortal();
void handleNotFound();

// Pins pour la matrice LED
#define P_LAT 5
#define P_A   19
#define P_B   23
#define P_C   18
#define P_OE  4

// Configuration des panneaux - définie par les build flags ou valeurs par défaut

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

// Temps d'affichage (plus élevé = plus lumineux, mais attention aux crashs)
uint8_t display_draw_time = 30; // 30-70 est généralement correct

// Objet matrice avec dimensions totales calculées
PxMATRIX display(TOTAL_WIDTH, TOTAL_HEIGHT, P_LAT, P_OE, P_A, P_B, P_C);

// Couleurs prédéfinies
uint16_t myRED      = display.color565(255, 0, 0);
uint16_t myGREEN    = display.color565(0, 255, 0);
uint16_t myBLUE     = display.color565(0, 0, 255);
uint16_t myYELLOW   = display.color565(255, 255, 0);
uint16_t myCYAN     = display.color565(0, 255, 255);
uint16_t myFUCHSIA  = display.color565(255, 0, 255);
uint16_t myWHITE    = display.color565(255, 255, 255);
uint16_t myBLACK    = display.color565(0, 0, 0);

uint16_t myCOLOR_ARRAY[7] = {myRED, myGREEN, myBLUE, myYELLOW, myCYAN, myFUCHSIA, myWHITE};
int cnt_Color = 0;
int myCOLOR_ARRAY_Length = sizeof(myCOLOR_ARRAY) / sizeof(myCOLOR_ARRAY[0]);

// Variables pour le texte défilant
unsigned long prevMill_Scroll_Text = 0;
int scrolling_Y_Pos = 0;
long scrolling_X_Pos;
long scrolling_X_Pos_CT;
uint16_t scrolling_Text_Color;
uint16_t text_Color;
char text_Scrolling_Text[151];
uint16_t text_Length_In_Pixel;
bool set_up_Scrolling_Text_Length = true;
bool start_Scroll_Text = false;
int scrolling_text_Display_Order = 0;
bool reset_Scrolling_Text = false;

// Variables de temps
unsigned long prevMill_Update_Time = 0;
const long interval_Update_Time = 1000;
unsigned long prevMill_Show_Clock = 0;
const long interval_Show_Clock = 500;

// Variables pour la date et l'heure
char daysOfTheWeek[7][10] = {"LUNDI", "MARDI", "MERCREDI", "JEUDI", "VENDREDI", "SAMEDI", "DIMANCHE"};
char chr_t_Minute[3];
uint8_t minute_Val, last_minute_Val;
char chr_t_Hour[3];
char day_and_date_Text[25];
bool blink_Colon = false;
uint16_t clock_Color;
uint16_t day_and_date_Text_Color;

// Variables de configuration
int d_Year;
uint8_t d_Month, d_Day;
uint8_t t_Hour, t_Minute, t_Second;
uint8_t input_Display_Mode = 1;
uint8_t input_Brightness = 125;
uint8_t input_Scrolling_Speed = 45;
int Color_Clock_R = 255, Color_Clock_G = 0, Color_Clock_B = 0;
int Color_Date_R = 0, Color_Date_G = 255, Color_Date_B = 0;
int Color_Text_R = 0, Color_Text_G = 0, Color_Text_B = 255;
char input_Scrolling_Text[151] = "ESP32 P10 RGB Digital Clock with PlatformIO";

// Variables pour le countdown
bool countdown_Active = false;
int countdown_Year = 2025;
int countdown_Month = 12;
int countdown_Day = 31;
int countdown_Hour = 23;
int countdown_Minute = 59;
int countdown_Second = 59;
char countdown_Title[51] = "NEW YEAR";
char countdown_Text[101];
bool countdown_Expired = false;
int Color_Countdown_R = 255, Color_Countdown_G = 165, Color_Countdown_B = 0; // Orange par défaut

// Configuration WiFi - Modifiez selon vos besoins
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Configuration Point d'accès (si pas de WiFi)
const char* ap_ssid = "HOKA_Clock";
const char* ap_password = "hoka";

// Clé de sécurité pour l'interface web
#define KEY_TXT "hoka"
  // display_update_enable(true); // NE PAS activer ici
// Mode de fonctionnement WiFi
bool useStationMode = false; // true = se connecter au WiFi, false = créer un point d'accès

// Objets RTC et Preferences
RTC_DS3231 rtc;
Preferences preferences;

// Serveur web et DNS pour portail captif
WebServer server(80);
DNSServer dnsServer;

// Configuration du portail captif
const byte DNS_PORT = 53;
const IPAddress apIP(192, 168, 1, 1);

// Gestionnaire d'interruption pour l'affichage
void IRAM_ATTR display_updater() {
  //if (timer != NULL) {
    portENTER_CRITICAL_ISR(&timerMux);
    display.display(display_draw_time);
    portEXIT_CRITICAL_ISR(&timerMux);
  //}
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

// Connexion WiFi
void connecting_To_WiFi() {
  Serial.println("\n-------------WIFI mode");
  Serial.println("WIFI mode : STA");
  WiFi.mode(WIFI_STA);
  Serial.println("-------------");
  delay(1000);

  Serial.println("\n-------------Connection");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  int connecting_process_timed_out = 40; // 20 secondes timeout
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if(connecting_process_timed_out > 0) connecting_process_timed_out--;
    if(connecting_process_timed_out == 0) {
      Serial.println("\nFailed to connect to WiFi. Switching to AP mode.");
      useStationMode = false;
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("Successfully connected to : ");
    Serial.println(ssid);
    Serial.print("IP address : ");
    Serial.println(WiFi.localIP());
  }
  Serial.println("-------------");
}

// Configuration du point d'accès avec portail captif
void set_ESP32_Access_Point() {
  Serial.println("\n-------------");
  Serial.println("WIFI mode : AP avec Portail Captif");
  WiFi.mode(WIFI_AP);
  Serial.println("-------------");
  delay(1000);

  Serial.println("\n-------------");
  Serial.println("Setting up ESP32 to be an Access Point with Captive Portal.");
  WiFi.softAP(ap_ssid, ap_password);
  delay(1000);
  
  // Configuration IP avec l'adresse définie pour le portail captif
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(apIP, gateway, subnet);
  
  // Démarrage du serveur DNS pour le portail captif
  dnsServer.start(DNS_PORT, "*", apIP);
  
  Serial.println("-------------");
  Serial.print("SSID name : ");
  Serial.println(ap_ssid);
  Serial.print("IP address : ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Portail captif activé - toutes les requêtes DNS seront redirigées");
  delay(1000);
}

// Fonction de calcul et formatage du countdown
void updateCountdown() {
  if (!countdown_Active) return;
  
  DateTime now = rtc.now();
  DateTime target(countdown_Year, countdown_Month, countdown_Day, countdown_Hour, countdown_Minute, countdown_Second);
  
  // Vérifier si le countdown est expiré
  if (now >= target) {
    countdown_Expired = true;
    strcpy(countdown_Text, countdown_Title);
    strcat(countdown_Text, " - EXPIRED!");
    return;
  }
  
  countdown_Expired = false;
  
  // Calculer la différence
  TimeSpan diff = target - now;
  long totalSeconds = diff.totalseconds();
  
  int days = totalSeconds / 86400;
  int hours = (totalSeconds % 86400) / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;
  
  // Formater le texte selon la durée restante
  if (days > 0) {
    sprintf(countdown_Text, "%s: %dd %02dh %02dm %02ds", countdown_Title, days, hours, minutes, seconds);
  } else if (hours > 0) {
    sprintf(countdown_Text, "%s: %02dh %02dm %02ds", countdown_Title, hours, minutes, seconds);
  } else {
    sprintf(countdown_Text, "%s: %02dm %02ds", countdown_Title, minutes, seconds);
  }
}

// Fonction pour obtenir la largeur du texte en pixels
uint16_t getTextWidth(const char* text) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// Fonction pour dessiner les deux points de l'heure
void drawColon(int16_t x, int16_t y, uint16_t colonColor) {
  display.drawPixel(x, y, colonColor);
  display.drawPixel(x+1, y, colonColor);
  display.drawPixel(x, y+1, colonColor);
  display.drawPixel(x+1, y+1, colonColor);

  display.drawPixel(x, y+3, colonColor);
  display.drawPixel(x+1, y+3, colonColor);
  display.drawPixel(x, y+4, colonColor);
  display.drawPixel(x+1, y+4, colonColor);
}

// Fonction de texte défilant adaptée aux panneaux multiples
void run_Scrolling_Text(uint8_t st_Y_Pos, byte st_Speed, char * st_Text, uint16_t st_Color) {
  if (start_Scroll_Text == true && set_up_Scrolling_Text_Length == true) {
    if (strlen(st_Text) > 0) {
      text_Length_In_Pixel = getTextWidth(st_Text);
      scrolling_X_Pos = TOTAL_WIDTH; // Utiliser la largeur totale
      set_up_Scrolling_Text_Length = false;
    } else {
      start_Scroll_Text = false;
      return;
    }
  }

  unsigned long currentMillis_Scroll_Text = millis();
  if (currentMillis_Scroll_Text - prevMill_Scroll_Text >= st_Speed) {
    prevMill_Scroll_Text = currentMillis_Scroll_Text;

    scrolling_X_Pos--;
    if (scrolling_X_Pos < -(TOTAL_WIDTH + text_Length_In_Pixel)) { // Utiliser la largeur totale
      set_up_Scrolling_Text_Length = true;
      start_Scroll_Text = false;
      return;
    }

    scrolling_X_Pos_CT = scrolling_X_Pos + 1;
    
    display.setTextColor(myBLACK);
    display.setCursor(scrolling_X_Pos_CT, st_Y_Pos);
    display.print(st_Text);
    
    display.setTextColor(st_Color);
    display.setCursor(scrolling_X_Pos, st_Y_Pos);
    display.print(st_Text);
  }
}

// Récupération de l'heure
void get_Time() {
  DateTime now = rtc.now();
  minute_Val = now.minute();
  sprintf(chr_t_Hour, "%02d", now.hour());
  sprintf(chr_t_Minute, "%02d", now.minute());
}

// Récupération de la date
void get_Date() {
  DateTime now = rtc.now();
  sprintf(day_and_date_Text, "%s, %02d-%02d-%d", 
          daysOfTheWeek[now.dayOfTheWeek()], 
          now.day(), now.month(), now.year());
}

// Chargement des paramètres depuis la mémoire flash
void loadSettings() {
  preferences.begin("mySettings", true);
  
  input_Display_Mode = preferences.getInt("input_DM", 1);
  input_Brightness = preferences.getInt("input_BRT", 125);
  input_Scrolling_Speed = preferences.getInt("input_SS", 45);
  Color_Clock_R = preferences.getInt("CC_R", 255);
  Color_Clock_G = preferences.getInt("CC_G", 0);
  Color_Clock_B = preferences.getInt("CC_B", 0);
  Color_Date_R = preferences.getInt("DC_R", 0);
  Color_Date_G = preferences.getInt("DC_G", 255);
  Color_Date_B = preferences.getInt("DC_B", 0);
  Color_Text_R = preferences.getInt("CT_R", 0);
  Color_Text_G = preferences.getInt("CT_G", 0);
  Color_Text_B = preferences.getInt("CT_B", 255);
  
  // Paramètres du countdown
  countdown_Active = preferences.getBool("cd_Active", false);
  countdown_Year = preferences.getInt("cd_Year", 2025);
  countdown_Month = preferences.getInt("cd_Month", 12);
  countdown_Day = preferences.getInt("cd_Day", 31);
  countdown_Hour = preferences.getInt("cd_Hour", 23);
  countdown_Minute = preferences.getInt("cd_Minute", 59);
  countdown_Second = preferences.getInt("cd_Second", 59);
  Color_Countdown_R = preferences.getInt("CD_R", 255);
  Color_Countdown_G = preferences.getInt("CD_G", 165);
  Color_Countdown_B = preferences.getInt("CD_B", 0);
  
  String savedText = preferences.getString("scrollText", "ESP32 P10 RGB Digital Clock with PlatformIO");
  strcpy(input_Scrolling_Text, savedText.c_str());
  
  String savedCountdownTitle = preferences.getString("cd_Title", "NEW YEAR");
  strcpy(countdown_Title, savedCountdownTitle.c_str());
  
  preferences.end();
  
  // Application des paramètres
  display.setBrightness(input_Brightness);
  if (input_Display_Mode == 1) {
    clock_Color = display.color565(Color_Clock_R, Color_Clock_G, Color_Clock_B);
    day_and_date_Text_Color = display.color565(Color_Date_R, Color_Date_G, Color_Date_B);
    text_Color = display.color565(Color_Text_R, Color_Text_G, Color_Text_B);
  }
}

// Gestionnaire pour le portail captif - redirige toutes les requêtes non reconnues
void handleCaptivePortal() {
  // Vérifier si c'est une requête pour la page principale de l'interface
  String hostHeader = server.hostHeader();
  
  // Si l'utilisateur accède directement à l'IP de l'ESP32, afficher la page normale
  if (hostHeader == WiFi.softAPIP().toString() || hostHeader == "192.168.1.1") {
    handleRoot();
    return;
  }
  
  // Pour toutes les autres requêtes (portail captif), rediriger vers l'interface
  String redirectUrl = "http://" + WiFi.softAPIP().toString();
  server.sendHeader("Location", redirectUrl, true);
  server.send(302, "text/plain", "");
}

// Gestionnaire pour toutes les requêtes non définies (NotFound)
void handleNotFound() {
  if (!useStationMode) {
    // En mode AP, traiter comme une requête de portail captif
    handleCaptivePortal();
  } else {
    // En mode Station, retourner une erreur 404 normale
    server.send(404, "text/plain", "Page non trouvée");
  }
}

// Gestionnaire de la page principale
void handleRoot() {
  server.send(200, "text/html", MAIN_page);
}

// Gestionnaire des paramètres
void handleSettings() {
  String incoming_Settings = server.arg("key");
  
  Serial.println("\n-------------Settings");
  Serial.print("Key : ");
  Serial.println(incoming_Settings);

  // Validation de clé supprimée - accès libre
  incoming_Settings = server.arg("sta");

  // Définir la date et l'heure
  if (incoming_Settings == "setTimeDate") {
    d_Year = server.arg("d_Year").toInt();
    d_Month = server.arg("d_Month").toInt();
    d_Day = server.arg("d_Day").toInt();
    t_Hour = server.arg("t_Hour").toInt();
    t_Minute = server.arg("t_Minute").toInt();
    t_Second = server.arg("t_Second").toInt();

    Serial.println("Set Time and Date.");
    Serial.printf("DateTime : %02d-%02d-%d %02d:%02d:%02d\n", d_Day, d_Month, d_Year, t_Hour, t_Minute, t_Second);

    rtc.adjust(DateTime(d_Year, d_Month, d_Day, t_Hour, t_Minute, t_Second));
    Serial.println("Setting completed.");
  }

  // Définir le mode d'affichage
  else if (incoming_Settings == "setDisplayMode") {
    input_Display_Mode = server.arg("input_Display_Mode").toInt();
    
    Serial.print("Set Display Mode : ");
    Serial.println(input_Display_Mode);

    // Arrêt sécurisé du timer avant modification des préférences
    display_update_enable(false);
    delay(50); // Attendre que les tâches se terminent
    
    preferences.begin("mySettings", false);
    preferences.putInt("input_DM", input_Display_Mode);
    preferences.end();
    
    delay(50); // Délai avant redémarrage
    display_update_enable(true);
    
    if (input_Display_Mode == 1) {
      clock_Color = display.color565(Color_Clock_R, Color_Clock_G, Color_Clock_B);
      day_and_date_Text_Color = display.color565(Color_Date_R, Color_Date_G, Color_Date_B);
      text_Color = display.color565(Color_Text_R, Color_Text_G, Color_Text_B);
    }
    
    display.clearDisplay();
    reset_Scrolling_Text = true;
    scrolling_text_Display_Order = 0;
  }

  // Définir la luminosité
  else if (incoming_Settings == "setBrightness") {
    input_Brightness = server.arg("input_Brightness").toInt();
    if (input_Brightness > 255) input_Brightness = 255;
    if (input_Brightness < 0) input_Brightness = 0;
    
    Serial.print("Set Brightness : ");
    Serial.println(input_Brightness);

    // Arrêt sécurisé du timer avant modification des préférences
    display_update_enable(false);
    delay(50);
    
    preferences.begin("mySettings", false);
    preferences.putInt("input_BRT", input_Brightness);
    preferences.end();
    
    delay(50);
    display_update_enable(true);
    display.setBrightness(input_Brightness);
  }

  // Définir la vitesse de défilement
  else if (incoming_Settings == "setScrollingSpeed") {
    input_Scrolling_Speed = server.arg("input_Scrolling_Speed").toInt();
    if (input_Scrolling_Speed > 100) input_Scrolling_Speed = 100;
    if (input_Scrolling_Speed < 10) input_Scrolling_Speed = 10;
    
    Serial.print("Set Scrolling Speed : ");
    Serial.println(input_Scrolling_Speed);

    preferences.begin("mySettings", false);
    preferences.putInt("input_SS", input_Scrolling_Speed);
    preferences.end();
  }

  // Définir la couleur de l'horloge
  else if (incoming_Settings == "setColorClock") {
    if (input_Display_Mode == 2) {
      server.send(200, "text/plain", "+ERR_DM");
      Serial.println("-------------");
      return;
    }
    
    Color_Clock_R = server.arg("Color_Clock_R").toInt();
    Color_Clock_G = server.arg("Color_Clock_G").toInt();
    Color_Clock_B = server.arg("Color_Clock_B").toInt();
    
    Serial.printf("Set Clock Color (RGB) : %d,%d,%d\n", Color_Clock_R, Color_Clock_G, Color_Clock_B);

    // Arrêt sécurisé du timer avant modification des préférences
    display_update_enable(false);
    delay(50);
    
    preferences.begin("mySettings", false);
    preferences.putInt("CC_R", Color_Clock_R);
    preferences.putInt("CC_G", Color_Clock_G);
    preferences.putInt("CC_B", Color_Clock_B);
    preferences.end();
    
    delay(50);
    display_update_enable(true);
    clock_Color = display.color565(Color_Clock_R, Color_Clock_G, Color_Clock_B);
  }

  // Définir la couleur de la date
  else if (incoming_Settings == "setColorDate") {
    if (input_Display_Mode == 2) {
      server.send(200, "text/plain", "+ERR_DM");
      Serial.println("-------------");
      return;
    }
    
    Color_Date_R = server.arg("Color_Date_R").toInt();
    Color_Date_G = server.arg("Color_Date_G").toInt();
    Color_Date_B = server.arg("Color_Date_B").toInt();
    
    Serial.printf("Set Date Color (RGB) : %d,%d,%d\n", Color_Date_R, Color_Date_G, Color_Date_B);

    // Arrêt sécurisé du timer avant modification des préférences
    display_update_enable(false);
    delay(50);
    
    preferences.begin("mySettings", false);
    preferences.putInt("DC_R", Color_Date_R);
    preferences.putInt("DC_G", Color_Date_G);
    preferences.putInt("DC_B", Color_Date_B);
    preferences.end();
    
    delay(50);
    display_update_enable(true);
    day_and_date_Text_Color = display.color565(Color_Date_R, Color_Date_G, Color_Date_B);
  }

  // Définir la couleur du texte
  else if (incoming_Settings == "setColorText") {
    if (input_Display_Mode == 2) {
      server.send(200, "text/plain", "+ERR_DM");
      Serial.println("-------------");
      return;
    }
    
    Color_Text_R = server.arg("Color_Text_R").toInt();
    Color_Text_G = server.arg("Color_Text_G").toInt();
    Color_Text_B = server.arg("Color_Text_B").toInt();
    
    Serial.printf("Set Text Color (RGB) : %d,%d,%d\n", Color_Text_R, Color_Text_G, Color_Text_B);

    // Arrêt sécurisé du timer avant modification des préférences
    display_update_enable(false);
    delay(50);
    
    preferences.begin("mySettings", false);
    preferences.putInt("CT_R", Color_Text_R);
    preferences.putInt("CT_G", Color_Text_G);
    preferences.putInt("CT_B", Color_Text_B);
    preferences.end();
    
    delay(50);
    display_update_enable(true);
    text_Color = display.color565(Color_Text_R, Color_Text_G, Color_Text_B);
  }

  // Définir le texte défilant
  else if (incoming_Settings == "setScrollingText") {
    String scrollText = server.arg("input_Scrolling_Text");
    if (scrollText.length() > 150) scrollText = scrollText.substring(0, 150);
    strcpy(input_Scrolling_Text, scrollText.c_str());
    
    Serial.print("Set Scrolling Text : ");
    Serial.println(input_Scrolling_Text);

    preferences.begin("mySettings", false);
    preferences.putString("scrollText", scrollText);
    preferences.end();
    
    reset_Scrolling_Text = true;
    scrolling_text_Display_Order = 0;
  }

  // Configurer le countdown
  else if (incoming_Settings == "setCountdown") {
    countdown_Active = server.arg("countdown_Active") == "true";
    countdown_Year = server.arg("countdown_Year").toInt();
  countdown_Month = server.arg("countdown_Month").toInt();
    countdown_Day = server.arg("countdown_Day").toInt();
    countdown_Hour = server.arg("countdown_Hour").toInt();
    countdown_Minute = server.arg("countdown_Minute").toInt();
    countdown_Second = server.arg("countdown_Second").toInt();
    
    String countdownTitle = server.arg("countdown_Title");
    if (countdownTitle.length() > 50) countdownTitle = countdownTitle.substring(0, 50);
    strcpy(countdown_Title, countdownTitle.c_str());
    
    Serial.println("Set Countdown:");
    Serial.printf("Active: %s\n", countdown_Active ? "true" : "false");
    Serial.printf("Target: %02d-%02d-%d %02d:%02d:%02d\n", 
                  countdown_Day, countdown_Month, countdown_Year, 
                  countdown_Hour, countdown_Minute, countdown_Second);
    Serial.printf("Title: %s\n", countdown_Title);

    preferences.begin("mySettings", false);
    preferences.putBool("cd_Active", countdown_Active);
    preferences.putInt("cd_Year", countdown_Year);
    preferences.putInt("cd_Month", countdown_Month);
    preferences.putInt("cd_Day", countdown_Day);
    preferences.putInt("cd_Hour", countdown_Hour);
    preferences.putInt("cd_Minute", countdown_Minute);
    preferences.putInt("cd_Second", countdown_Second);
    preferences.putString("cd_Title", countdownTitle);
    preferences.end();
    
    countdown_Expired = false;
    reset_Scrolling_Text = true;
    scrolling_text_Display_Order = 0;
  }

  // Définir la couleur du countdown
  else if (incoming_Settings == "setColorCountdown") {
    if (input_Display_Mode == 2) {
      server.send(200, "text/plain", "+ERR_DM");
      Serial.println("-------------");
      return;
    }
    
    Color_Countdown_R = server.arg("Color_Countdown_R").toInt();
    Color_Countdown_G = server.arg("Color_Countdown_G").toInt();
    Color_Countdown_B = server.arg("Color_Countdown_B").toInt();
    
    Serial.printf("Set Countdown Color (RGB) : %d,%d,%d\n", Color_Countdown_R, Color_Countdown_G, Color_Countdown_B);

    // Arrêt sécurisé du timer avant modification des préférences
    display_update_enable(false);
    delay(50);
    
    preferences.begin("mySettings", false);
    preferences.putInt("CD_R", Color_Countdown_R);
    preferences.putInt("CD_G", Color_Countdown_G);
    preferences.putInt("CD_B", Color_Countdown_B);
    preferences.end();
    
    delay(50);
    display_update_enable(true);
  }

  // Reset du système
  else if (incoming_Settings == "resetSystem") {
    Serial.println("System Reset requested");
    server.send(200, "text/plain", "+OK");
    delay(1000);
    ESP.restart();
  }

  server.send(200, "text/plain", "+OK");
  Serial.println("-------------");
}

// Configuration et démarrage du serveur
void prepare_and_start_The_Server() {
  // Routes principales
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  
  // Routes communes pour le portail captif
  server.on("/generate_204", handleRoot);  // Android
  server.on("/fwlink", handleRoot);        // Microsoft
  server.on("/hotspot-detect.html", handleRoot); // Apple
  
  // Gestionnaire pour toutes les autres requêtes (portail captif)
  server.onNotFound(handleNotFound);
  
  delay(500);

  server.begin();
  Serial.println("\nHTTP server started");
  
  if (useStationMode) {
    Serial.print("Open http://");
    Serial.print(WiFi.localIP());
    Serial.println(" in your browser");
  } else {
    Serial.print("Connect to WiFi network: ");
    Serial.println(ap_ssid);
    Serial.print("Captive Portal active - navigate to any website");
    Serial.print(" or open http://");
    Serial.print(WiFi.softAPIP());
    Serial.println(" directly");
  }
  
  Serial.println("No authentication required - Open access");
  delay(500);
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("\n=== ESP32 P10 RGB Digital Clock ===");
  Serial.println("Version: PlatformIO Compatible with Cascade Support");
  
  // Affichage de la configuration des panneaux
  Serial.println("\n--- Configuration Panneaux ---");
  Serial.printf("Panneaux X: %d\n", MATRIX_PANELS_X);
  Serial.printf("Panneaux Y: %d\n", MATRIX_PANELS_Y);
  Serial.printf("Taille panneau: %dx%d pixels\n", MATRIX_WIDTH, MATRIX_HEIGHT);
  Serial.printf("Taille totale: %dx%d pixels\n", TOTAL_WIDTH, TOTAL_HEIGHT);
  Serial.printf("Nombre total panneaux: %d\n", MATRIX_PANELS_X * MATRIX_PANELS_Y);
  
  // Ajustement automatique de la luminosité et du draw_time selon le nombre de panneaux
  int auto_brightness = 125;
  uint8_t auto_draw_time = 30;
  
  if (MATRIX_PANELS_X > 2) {
    auto_brightness = 100;
    auto_draw_time = 25;
  }
  if (MATRIX_PANELS_X > 4) {
    auto_brightness = 80;
    auto_draw_time = 20;
  }
  if (MATRIX_PANELS_X > 6) {
    auto_brightness = 60;
    auto_draw_time = 15;
  }
  
  input_Brightness = auto_brightness;
  display_draw_time = auto_draw_time;
  
  Serial.printf("Luminosité auto-ajustée: %d\n", auto_brightness);
  Serial.printf("Draw time ajusté: %d\n", auto_draw_time);
  Serial.println("------------------------------");

  // Initialisation du RTC
  Serial.println("\n------------");
  Serial.println("Starting DS3231 RTC module...");
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1) delay(10);
  }
  Serial.println("DS3231 RTC module started successfully");
  Serial.println("------------");

  // Initialisation de l'affichage
    // Initialisation de l'affichage avec configuration P10 optimisée
  display.begin(4); // 1/8 scan pour P10
  display.setScanPattern(ZAGZIG);
  display.setMuxPattern(BINARY); 
  const int muxdelay = 10; // Délai de multiplexage
  display.setMuxDelay(muxdelay, muxdelay, muxdelay, muxdelay, muxdelay);
  delay(100);

  // NE PAS activer le timer ici - attendre après le WiFi
  display.clearDisplay();
  delay(500);

  // Chargement des paramètres
  loadSettings();

  // Appliquer la luminosité ajustée si pas de sauvegarde
  display.setBrightness(input_Brightness);

  // Test d'affichage des couleurs avec message adapté - SANS timer
  Serial.println("Testing display colors...");
  
  // Test de bordures pour vérifier l'alignement (panneaux multiples)
  if (MATRIX_PANELS_X > 1 || MATRIX_PANELS_Y > 1) {
    Serial.println("Testing panel alignment...");
    
    // Bordure extérieure
    display.drawRect(0, 0, TOTAL_WIDTH, TOTAL_HEIGHT, myWHITE);
    delay(1000);
    
    // Lignes de séparation entre panneaux
    for (int i = 1; i < MATRIX_PANELS_X; i++) {
      int x = i * MATRIX_WIDTH;
      display.drawLine(x, 0, x, TOTAL_HEIGHT - 1, myRED);
    }
    for (int i = 1; i < MATRIX_PANELS_Y; i++) {
      int y = i * MATRIX_HEIGHT;
      display.drawLine(0, y, TOTAL_WIDTH - 1, y, myGREEN);
    }
    delay(2000);
    display.clearDisplay();
  }
  
  // Test des couleurs - mode manuel (sans timer)
  display.fillScreen(myRED);
  delay(1000);
  display.fillScreen(myGREEN);
  delay(1000);
  display.fillScreen(myBLUE);
  delay(1000);
  display.fillScreen(myWHITE);
  delay(1000);

  display.clearDisplay();
  delay(500);

  display.setTextWrap(false);
  display.setTextSize(1);
  display.setRotation(0);

  // Affichage du message de démarrage adapté
  display.setTextColor(myWHITE);
  
  // Calculer la position centrée pour le texte
  String startMsg = "ESP32 CLOCK";
  if (MATRIX_PANELS_X > 1) {
    startMsg = String(MATRIX_PANELS_X) + "x" + String(MATRIX_PANELS_Y) + " P10 CLOCK";
  }
  
  int textWidth = startMsg.length() * 6; // Approximation
  int startX = (TOTAL_WIDTH - textWidth) / 2;
  if (startX < 0) startX = 0;
  
  display.setCursor(startX, 0);
  display.print(startMsg);
  
  if (TOTAL_HEIGHT > 16) {
    display.setCursor(startX, 16);
    display.print("CASCADE MODE");
  } else {
    display.setCursor(startX, 8);
    display.print("READY");
  }
  
  delay(3000);
  display.clearDisplay();

  // Configuration WiFi
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

  Serial.println("\nSetup completed. System ready!");
  // Message de fin adapté à la configuration
  if (MATRIX_PANELS_X > 1) {
    Serial.printf("Running with %dx%d panels cascade (%dx%d total resolution)\n", 
                  MATRIX_PANELS_X, MATRIX_PANELS_Y, TOTAL_WIDTH, TOTAL_HEIGHT);
  }

  // ACTIVATION DU TIMER D'AFFICHAGE APRÈS TOUT LE RESTE
  Serial.println("Activating display timer...");
  display_update_enable(true);
  delay(200); // Attendre que le timer soit stable

  // --- FreeRTOS : création des tâches principales EN DERNIÈRE ÉTAPE ---
  // Attendre un peu que tout soit stable avant de créer les tâches
  delay(100);
  
  // Tâche d'affichage (priorité 2)
  xTaskCreate(DisplayTask, "DisplayTask", 4096, NULL, 2, NULL);
  // Tâche serveur web (priorité 1)
  xTaskCreate(WebServerTask, "WebServerTask", 4096, NULL, 1, NULL);
  // Tâche WiFi (priorité 1, extensible)
  xTaskCreate(WiFiTask, "WiFiTask", 2048, NULL, 1, NULL);
  
  Serial.println("FreeRTOS tasks created successfully!");
}

// --- FreeRTOS : Tâche d'affichage principale ---
void DisplayTask(void *pvParameters) {
  // Attendre un peu que le système soit complètement initialisé
  vTaskDelay(pdMS_TO_TICKS(100));
  
  for (;;) {
    // Mise à jour de l'heure et du countdown
    unsigned long currentMillis_Update_Time = millis();
    if (currentMillis_Update_Time - prevMill_Update_Time >= interval_Update_Time) {
      prevMill_Update_Time = currentMillis_Update_Time;
      get_Time();
      blink_Colon = !blink_Colon;
      if (countdown_Active) {
        updateCountdown();
      }
    }

    // Affichage de l'horloge
    unsigned long currentMillis_Show_Clock = millis();
    if (currentMillis_Show_Clock - prevMill_Show_Clock >= interval_Show_Clock) {
      prevMill_Show_Clock = currentMillis_Show_Clock;
      display.setTextSize(1);
      // Couleur selon le mode
      if (input_Display_Mode == 1) {
        clock_Color = display.color565(Color_Clock_R, Color_Clock_G, Color_Clock_B);
      } else {
        clock_Color = myCOLOR_ARRAY[cnt_Color];
      }
      int clock_width = 30;
      int clock_x = (TOTAL_WIDTH - clock_width) / 2;
      if (clock_x < 1) clock_x = 1;
      if (last_minute_Val != minute_Val) display.fillRect(clock_x, 0, 11, 7, myBLACK);
      display.setTextColor(clock_Color);
      display.setCursor(clock_x, 0);
      display.print(chr_t_Hour);
      int colon_x = clock_x + 14;
      if (blink_Colon) {
        drawColon(colon_x, 1, clock_Color);
      } else {
        drawColon(colon_x, 1, myBLACK);
      }
      if (last_minute_Val != minute_Val) display.fillRect(clock_x + 19, 0, 11, 7, myBLACK);
      display.setTextColor(clock_Color);
      display.setCursor(clock_x + 19, 0);
      display.print(chr_t_Minute);
      last_minute_Val = minute_Val;
    }

    // Gestion du texte défilant
    if (reset_Scrolling_Text) {
      start_Scroll_Text = false;
      set_up_Scrolling_Text_Length = true;
      reset_Scrolling_Text = false;
    }
    if (start_Scroll_Text == false) {
      scrolling_text_Display_Order++;
      int maxOrder = countdown_Active ? 3 : 2;
      if (input_Display_Mode == 2) {
        maxOrder++;
      }
      if (scrolling_text_Display_Order > maxOrder) scrolling_text_Display_Order = 1;
      if (scrolling_text_Display_Order == 1) {
        get_Date();
        display.setTextSize(1);
        scrolling_Y_Pos = 8;
        if (input_Display_Mode == 1) {
          scrolling_Text_Color = display.color565(Color_Date_R, Color_Date_G, Color_Date_B);
        } else {
          int next_cnt_Color = (cnt_Color + 1) % myCOLOR_ARRAY_Length;
          scrolling_Text_Color = myCOLOR_ARRAY[next_cnt_Color];
        }
        strcpy(text_Scrolling_Text, day_and_date_Text);
      }
      if (scrolling_text_Display_Order == 2) {
        display.setTextSize(1);
        scrolling_Y_Pos = 8;
        if (input_Display_Mode == 1) {
          scrolling_Text_Color = display.color565(Color_Text_R, Color_Text_G, Color_Text_B);
        } else {
          int next_cnt_Color = (cnt_Color + 2) % myCOLOR_ARRAY_Length;
          scrolling_Text_Color = myCOLOR_ARRAY[next_cnt_Color];
        }
        strcpy(text_Scrolling_Text, input_Scrolling_Text);
      }
      if (scrolling_text_Display_Order == 3 && countdown_Active) {
        display.setTextSize(1);
        scrolling_Y_Pos = 8;
        if (input_Display_Mode == 1) {
          if (countdown_Expired) {
            scrolling_Text_Color = myRED;
          } else {
            scrolling_Text_Color = display.color565(Color_Countdown_R, Color_Countdown_G, Color_Countdown_B);
          }
        } else {
          int next_cnt_Color = (cnt_Color + 3) % myCOLOR_ARRAY_Length;
          scrolling_Text_Color = myCOLOR_ARRAY[next_cnt_Color];
        }
        strcpy(text_Scrolling_Text, countdown_Text);
      }
      int colorChangeOrder = countdown_Active ? 4 : 3;
      if (scrolling_text_Display_Order == colorChangeOrder && input_Display_Mode == 2) {
        cnt_Color = (cnt_Color + 1) % myCOLOR_ARRAY_Length;
        strcpy(text_Scrolling_Text, "");
      }
      start_Scroll_Text = true;
    }
    if (start_Scroll_Text) {
      run_Scrolling_Text(scrolling_Y_Pos, input_Scrolling_Speed, text_Scrolling_Text, scrolling_Text_Color);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // FreeRTOS : délai approprié de 10ms
  }
}

// --- FreeRTOS : Tâche serveur web ---
void WebServerTask(void *pvParameters) {
  // Attendre un peu que le système soit complètement initialisé
  vTaskDelay(pdMS_TO_TICKS(200));
  
  for (;;) {
    server.handleClient();
    
    // Gestion du serveur DNS pour le portail captif (uniquement en mode AP)
    if (!useStationMode) {
      dnsServer.processNextRequest();
    }
    
    vTaskDelay(pdMS_TO_TICKS(5)); // FreeRTOS : délai approprié de 5ms
  }
}

// --- FreeRTOS : Tâche WiFi (optionnelle, extensible) ---
void WiFiTask(void *pvParameters) {
  // Attendre un peu que le système soit complètement initialisé
  vTaskDelay(pdMS_TO_TICKS(300));
  
  for (;;) {
    // Ici, vous pouvez ajouter la logique de surveillance ou de reconnexion WiFi
    vTaskDelay(pdMS_TO_TICKS(1000)); // FreeRTOS : délai approprié de 1 seconde
  }
}

// Fonction loop() vide requise par le framework Arduino
// Toute la logique est maintenant gérée par les tâches FreeRTOS
void loop() {
  // Fonction vide - toute la logique est dans les tâches FreeRTOS
  delay(1000); // Utiliser delay() Arduino au lieu de vTaskDelay
}
