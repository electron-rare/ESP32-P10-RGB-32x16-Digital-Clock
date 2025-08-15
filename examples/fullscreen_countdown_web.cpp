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
// Paramètres de clignotement configurables (pour les 10 dernières secondes)
bool blinkEnabled = true;       // 1 = clignote sur la fin du compte à rebours
int blinkIntervalMs = 500;      // Intervalle de clignotement en ms (modifiable via Web UI)
int blinkWindowSeconds = 10;    // Nombre de dernières secondes pendant lesquelles le clignotement est actif

// Marquee (défilement) pour le texte final si trop long
volatile bool marqueeActive = false; // indicateur global pour la tâche d'affichage
// Paramètres configurables et état du défilement
bool marqueeEnabled = true;              // activation auto si texte trop long
int marqueeIntervalMs = 40;              // intervalle ms entre déplacements (1 px)
int marqueeGap = 24;                     // espace en pixels avant répétition
int marqueeMode = 0;                     // 0=Auto (si overflow, scroll gauche), 1=Toujours gauche, 2=Aller-Retour, 3=Une fois
int marqueeReturnIntervalMs = 60;        // vitesse différente pour le retour (aller-retour)
int marqueeBouncePauseLeftMs = 400;      // pause extrémité gauche (ms)
int marqueeBouncePauseRightMs = 400;     // pause extrémité droite (ms)
int marqueeOneShotDelayMs = 800;         // délai centré avant départ (ms)
bool marqueeOneShotStopCenter = true;    // recadrer au centre à la fin
int marqueeOneShotRestartSec = 0;        // redémarrage automatique (0=pas de restart)
// Accélération progressive
bool marqueeAccelEnabled = false;
int marqueeAccelStartIntervalMs = 80;    // intervalle initial
int marqueeAccelEndIntervalMs = 30;      // intervalle final
int marqueeAccelDurationMs = 3000;       // durée interpolation sur un cycle (ms)
static int marqueeTextWidth = 0;         // largeur pixels du texte courant
static int marqueeOffset = 0;            // position X courante
static unsigned long lastMarqueeStep = 0; // dernière étape
static int marqueeDirection = -1;        // pour mode aller-retour
static bool marqueeOneShotDone = false;  // pour mode une fois
static bool marqueeInPause = false;      // pause extrémités bounce
static unsigned long marqueePauseUntil = 0; // fin pause
static bool marqueeOneShotCenterPhase = false; // phase centrée initiale
static unsigned long marqueeOneShotStart = 0;  // début phase centrée
static unsigned long marqueeOneShotRestartAt = 0; // moment de relance
static unsigned long marqueeCycleStartMs = 0;   // début cycle pour accélération

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
int displayBrightness = -1; // -1 = auto (calculé selon nombre de panneaux)

// Format d'affichage (0=jours, 1=heures, 2=minutes, 3=secondes uniquement)
int displayFormat = 0;

// HTML Page
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="fr"><head>
<meta charset="utf-8" />
<title>ESP32 P10 Countdown</title>
<meta name="viewport" content="width=device-width,initial-scale=1" />
<style>
:root { --accent:#009688; --accent-hover:#017a6f; --danger:#d32f2f; --bg:#101418; --panel:#1c242b; --panel-alt:#232e36; --text:#e5ecec; --muted:#7d8b91; --ok:#4caf50; --warn:#ffc107; font-size:16px; }
* { box-sizing:border-box; }
body { margin:0; font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif; background:var(--bg); color:var(--text); -webkit-font-smoothing:antialiased; }
h1 { font-size:1.35rem; margin:0 0 1rem; text-align:center; letter-spacing:.5px; }
h2 { font-size:.95rem; margin:1.5rem 0 .75rem; text-transform:uppercase; letter-spacing:.08em; color:var(--muted); }
a { color:var(--accent); }
.wrap { max-width:760px; margin:0 auto; padding:clamp(12px,2vw,32px); }
.grid { display:grid; gap:14px; }
.card { background:linear-gradient(145deg,var(--panel) 0%,var(--panel-alt) 100%); padding:18px 20px 22px; border-radius:14px; position:relative; box-shadow:0 2px 4px rgba(0,0,0,.3),0 8px 32px -8px rgba(0,0,0,.45); overflow:hidden; }
form .field { display:flex; flex-direction:column; gap:4px; }
label { font-size:.70rem; font-weight:600; letter-spacing:.06em; text-transform:uppercase; color:var(--muted); }
input:not([type=color]), select { background:#0d1418; border:1px solid #233038; border-radius:8px; padding:10px 11px; font:inherit; color:var(--text); outline:none; transition:.2s border-color,.2s background; }
input:focus, select:focus { border-color:var(--accent); background:#111b20; }
input[type=color] { border:none; background:transparent; width:54px; height:42px; padding:0; cursor:pointer; }
.inline { display:flex; gap:10px; }
.inline > * { flex:1; }
.actions { display:flex; flex-wrap:wrap; gap:10px; margin-top:8px; }
button { --b:var(--accent); flex:1; cursor:pointer; border:none; border-radius:10px; padding:12px 18px; font:600 .9rem/1 system-ui; letter-spacing:.04em; background:linear-gradient(135deg,var(--b) 0%, #00b39e 100%); color:#fff; box-shadow:0 3px 10px -2px rgba(0,0,0,.5); transition:.25s transform,.25s filter; }
button:hover { transform:translateY(-2px); filter:brightness(1.08); }
button:active { transform:translateY(0); filter:brightness(.92); }
button.secondary { --b:#2d3a41; background:linear-gradient(135deg,#2d3a41,#37464f); }
button.danger { --b:var(--danger); background:linear-gradient(135deg,#d13737,#ef5350); }
.preview-panel { text-align:center; padding:18px 12px 24px; border-radius:16px; background:#06090b; position:relative; }
#countdown-preview { font-family:monospace; font-weight:600; font-size: clamp(28px,10vw,72px); line-height:1; color:#00ff73; letter-spacing:.04em; text-shadow:0 0 6px #00ff73, 0 0 14px rgba(0,255,115,.55); transition:.25s color,.25s text-shadow; word-break:break-word; }
#titlePreview { display:block; font-size:.75rem; font-weight:500; letter-spacing:.12em; margin-top:10px; color:var(--muted); text-transform:uppercase; }
.blink { animation:blink .9s steps(2,start) infinite; }
@keyframes blink { to { visibility:hidden; } }
.swatches { display:flex; flex-wrap:wrap; gap:6px; margin-top:6px; }
.swatch { width:30px; height:30px; border-radius:6px; cursor:pointer; position:relative; box-shadow:0 0 0 2px #141c22, 0 0 0 4px rgba(0,0,0,.3); transition:.2s transform,.2s box-shadow; }
.swatch:hover { transform:scale(1.1); box-shadow:0 0 0 2px #1f2d35,0 0 0 5px rgba(0,0,0,.55); }
.swatch.active { outline:2px solid #fff; }
.quick { display:flex; flex-wrap:wrap; gap:6px; margin-top:4px; }
.quick button { flex:1 0 120px; font-size:.65rem; padding:8px 10px; background:#25323a; }
.inline-note { font-size:.65rem; color:var(--muted); margin:6px 0 2px; }
.save-indicator { position:absolute; top:10px; right:14px; font-size:.65rem; letter-spacing:.1em; display:flex; align-items:center; gap:6px; color:var(--muted); }
.save-indicator.active { color:var(--accent); }
.dot { width:9px; height:9px; border-radius:50%; background:var(--muted); position:relative; overflow:hidden; }
.save-indicator.active .dot { background:var(--accent); box-shadow:0 0 0 4px rgba(0,150,136,.25); }
.toast { position:fixed; top:14px; left:50%; transform:translateX(-50%) translateY(-20px); background:#182125; color:var(--text); padding:10px 18px; border-radius:999px; font-size:.7rem; letter-spacing:.08em; opacity:0; pointer-events:none; transition:.35s; box-shadow:0 4px 22px -6px rgba(0,0,0,.6); }
.toast.show { opacity:1; transform:translateX(-50%) translateY(0); }
.row-2 { display:grid; gap:12px; grid-template-columns:repeat(auto-fit,minmax(120px,1fr)); }
footer { margin:40px 0 10px; text-align:center; font-size:.6rem; letter-spacing:.15em; color:var(--muted); }
@media (max-width:680px){ .row-2 { grid-template-columns:repeat(auto-fit,minmax(100px,1fr)); } }
</style>
</head><body>
<div class="wrap grid" style="gap:18px;">
  <div class="card preview-panel" id="previewCard">
    <div class="save-indicator" id="saveState"><span class="dot"></span><span id="saveLabel">PRÊT</span></div>
    <div id="countdown-preview">--:--:--</div>
    <span id="titlePreview"></span>
    <div class="inline-note" id="expireNote" style="display:none;">Le compte à rebours est terminé.</div>
    <div class="quick" aria-label="Ajustements rapides">
      <button type="button" data-add="60">+1 min</button>
      <button type="button" data-add="300">+5 min</button>
      <button type="button" data-add="3600">+1 h</button>
      <button type="button" data-add="86400">+1 jour</button>
      <button type="button" data-add="-300">-5 min</button>
      <button type="button" data-set="EOD">Fin de journée</button>
    </div>
  </div>
  <div class="card" id="formCard">
    <h1>Configuration du Compte à Rebours</h1>
    <form action="/settings" method="POST" id="settingsForm" autocomplete="off">
      <div class="grid" style="gap:18px;">
        <div class="field">
          <label for="title">Titre</label>
          <input id="title" name="title" maxlength="50" required placeholder="Mon Événement" />
        </div>
        <div class="row-2">
          <div class="field"><label for="date">Date</label><input type="date" id="date" name="date" required /></div>
          <div class="field"><label for="time">Heure (hh:mm:ss)</label><input type="time" id="time" name="time" step="1" required /></div>
        </div>
        <div class="row-2">
          <div class="field"><label for="fontIndex">Police</label>
            <select id="fontIndex" name="fontIndex">
              <option value="0">Standard</option>
              <option value="1">Sans Serif</option>
              <option value="2">Sans Serif Bold</option>
              <option value="3">Monospace</option>
            </select>
          </div>
          <div class="field"><label for="textSize">Taille</label>
            <select id="textSize" name="textSize">
              <option value="1">Petite</option>
              <option value="2" selected>Moyenne</option>
              <option value="3">Grande</option>
            </select>
          </div>
          <div class="field" style="grid-column:1/-1;">
            <label>Couleur</label>
            <div class="inline" style="align-items:center;">
              <input type="color" id="colorPicker" value="#00ff00" aria-label="Couleur personnalisée" />
              <div style="display:flex; flex:1; flex-wrap:wrap; gap:6px;" class="swatches" id="swatches" aria-label="Couleurs prédéfinies"></div>
            </div>
            <input type="hidden" id="colorR" name="colorR" value="0" />
            <input type="hidden" id="colorG" name="colorG" value="255" />
            <input type="hidden" id="colorB" name="colorB" value="0" />
          </div>
        </div>
      </div>
      <div class="actions">
        <button type="submit" id="btnSave">Enregistrer</button>
        <button type="button" class="secondary" id="btnNow">Maintenant</button>
        <button type="button" class="secondary" id="btnSyncTime" title="Synchroniser le RTC avec l'heure locale du navigateur">Sync Heure</button>
        <button type="button" class="danger" id="btnReset">Réinitialiser</button>
      </div>
      <h2>Clignotement</h2>
      <div class="row-2">
        <div class="field">
          <label for="blinkEnabled">Activer Clignotement</label>
          <select id="blinkEnabled" name="blinkEnabled">
            <option value="1" selected>Oui</option>
            <option value="0">Non</option>
          </select>
        </div>
        <div class="field">
          <label for="blinkInterval">Intervalle (ms)</label>
          <input type="number" id="blinkInterval" name="blinkInterval" min="50" max="5000" step="50" value="500" />
        </div>
        <div class="field" style="grid-column:1/-1;">
          <label for="blinkWindow">Dernières secondes (fenêtre)</label>
          <input type="number" id="blinkWindow" name="blinkWindow" min="1" max="3600" step="1" value="10" />
        </div>
      </div>
      <h2>Défilement</h2>
      <div class="row-2">
        <div class="field">
          <label for="marqueeEnabled">Activer Défilement</label>
          <select id="marqueeEnabled" name="marqueeEnabled">
            <option value="1" selected>Oui</option>
            <option value="0">Non</option>
          </select>
        </div>
        <div class="field">
          <label for="marqueeInterval">Vitesse (ms/pixel)</label>
          <input type="number" id="marqueeInterval" name="marqueeInterval" min="5" max="500" step="5" value="40" />
        </div>
        <div class="field">
          <label for="marqueeGap">Espace (px)</label>
          <input type="number" id="marqueeGap" name="marqueeGap" min="4" max="256" step="2" value="24" />
        </div>
        <div class="field" style="grid-column:1/-1;">
          <label for="marqueeMode">Mode</label>
          <select id="marqueeMode" name="marqueeMode">
            <option value="0" selected>Auto (si dépasse)</option>
            <option value="1">Toujours (gauche)</option>
            <option value="2">Aller-Retour</option>
            <option value="3">Une seule fois</option>
          </select>
          <div class="inline-note">Aller-Retour et Une seule fois nécessitent un texte plus large que l'afficheur (sauf Toujours).</div>
        </div>
        <div class="field">
          <label for="marqueeReturnInterval">Vitesse Retour (ms/pixel)</label>
          <input type="number" id="marqueeReturnInterval" name="marqueeReturnInterval" min="5" max="500" step="5" value="60" />
        </div>
        <div class="field">
          <label for="marqueeBouncePauseLeft">Pause Gauche (ms)</label>
          <input type="number" id="marqueeBouncePauseLeft" name="marqueeBouncePauseLeft" min="0" max="3000" step="50" value="400" />
        </div>
        <div class="field">
          <label for="marqueeBouncePauseRight">Pause Droite (ms)</label>
          <input type="number" id="marqueeBouncePauseRight" name="marqueeBouncePauseRight" min="0" max="3000" step="50" value="400" />
        </div>
        <div class="field">
          <label for="marqueeOneShotDelay">Délai Centré One-Shot (ms)</label>
          <input type="number" id="marqueeOneShotDelay" name="marqueeOneShotDelay" min="0" max="5000" step="100" value="800" />
        </div>
        <div class="field">
          <label for="marqueeOneShotStopCenter">One-Shot Fin Centrée</label>
          <select id="marqueeOneShotStopCenter" name="marqueeOneShotStopCenter">
            <option value="1" selected>Oui</option>
            <option value="0">Non</option>
          </select>
        </div>
        <div class="field">
          <label for="marqueeOneShotRestart">Restart One-Shot (s)</label>
          <input type="number" id="marqueeOneShotRestart" name="marqueeOneShotRestart" min="0" max="86400" step="1" value="0" />
        </div>
        <div class="field" style="grid-column:1/-1;">
          <label>Accélération Progressive</label>
          <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:flex-end;">
            <select id="marqueeAccelEnabled" name="marqueeAccelEnabled">
              <option value="0" selected>Off</option>
              <option value="1">On</option>
            </select>
            <label style="font-size:12px;">Début(ms)<input type="number" id="marqueeAccelStart" name="marqueeAccelStart" min="5" max="500" step="5" value="80" style="width:80px;margin-left:4px;" /></label>
            <label style="font-size:12px;">Fin(ms)<input type="number" id="marqueeAccelEnd" name="marqueeAccelEnd" min="5" max="500" step="5" value="30" style="width:80px;margin-left:4px;" /></label>
            <label style="font-size:12px;">Durée(ms)<input type="number" id="marqueeAccelDuration" name="marqueeAccelDuration" min="100" max="20000" step="100" value="3000" style="width:90px;margin-left:4px;" /></label>
          </div>
          <div class="inline-note">Interpolation linéaire entre vitesse début et fin à chaque cycle (aller, retour ou boucle complete).</div>
        </div>
      </div>
      <h2>Luminosité</h2>
      <div class="row-2">
        <div class="field">
          <label for="brightnessMode">Mode</label>
          <select id="brightnessMode">
            <option value="auto" selected>Auto</option>
            <option value="manual">Manuel</option>
          </select>
        </div>
        <div class="field" style="grid-column:1/-1;">
          <label for="brightness">Valeur (0-255)</label>
          <input type="range" id="brightness" name="brightness" min="0" max="255" value="150" />
          <div style="font-size:.65rem; color:var(--muted);">Auto = -1 (adapté au nombre de panneaux)</div>
          <input type="hidden" id="brightnessHidden" name="brightness" value="-1" />
        </div>
      </div>
      <div class="inline-note">Les modifications sont appliquées immédiatement sur l'afficheur après envoi.</div>
    </form>
  </div>
  <footer>ESP32 P10 RGB Fullscreen Countdown • 2025</footer>
</div>
<div class="toast" id="toast"></div>
<script>
// === Utilitaires Couleur ===
const hex = n => n.toString(16).padStart(2,'0');
function hexToRgb(h){h=h.replace('#','');return {r:parseInt(h.substr(0,2),16),g:parseInt(h.substr(2,2),16),b:parseInt(h.substr(4,2),16)};}
function rgbToHex(r,g,b){return '#'+hex(r)+hex(g)+hex(b);} 

// === Éléments DOM ===
const el = id => document.getElementById(id);
const preview = el('countdown-preview');
const titlePreview = el('titlePreview');
const expireNote = el('expireNote');
const form = el('settingsForm');
const saveState = el('saveState');
const saveLabel = el('saveLabel');
const toast = el('toast');
const swatchesBox = el('swatches');

// === Swatches prédéfinies ===
const SWATCHES = ['#00ff00','#ffffff','#ff0000','#00bcd4','#ffeb3b','#ff9800','#ff00ff','#2196f3','#9c27b0','#4caf50'];
SWATCHES.forEach(c=>{const d=document.createElement('div');d.className='swatch';d.style.background=c;d.dataset.color=c;d.title=c;d.addEventListener('click',()=>{el('colorPicker').value=c;updateColor(c,true);});swatchesBox.appendChild(d);});

// === Variables de logique ===
let target = null; // Date target JS
let expired=false; let blink=false; let lastBlink=0;
let lastServerSync = 0; let offsetMs = 0; // Diff client/serveur (si future sync ajoutée)

function showToast(msg,ok=true){toast.textContent=msg;toast.classList.add('show');toast.style.background= ok?'#1e2c31':'#452222'; setTimeout(()=>toast.classList.remove('show'),2600);} 

function setSaving(active,label){if(active){saveState.classList.add('active');saveLabel.textContent=label||'SAUVEGARDE';}else{saveState.classList.remove('active');saveLabel.textContent=label||'PRÊT';}}

function updateColor(val,fromSwatch=false){const {r,g,b}=hexToRgb(val); el('colorR').value=r; el('colorG').value=g; el('colorB').value=b; preview.style.color=val; preview.style.textShadow=`0 0 6px ${val},0 0 16px ${val}80`; document.querySelectorAll('.swatch').forEach(s=>s.classList.toggle('active',s.dataset.color===val)); if(fromSwatch){showToast('Couleur appliquée');}}

function parseFormDateTime(){ const d=el('date').value; const t=el('time').value || '00:00:00'; if(!d) return null; const parts = d.split('-').map(Number); const time=t.split(':').map(Number); return new Date(parts[0],parts[1]-1,parts[2],time[0]||0,time[1]||0,time[2]||0); }

function refreshTarget(){ target = parseFormDateTime(); }

function updatePreviewLoop(ts){ if(!target){ requestAnimationFrame(updatePreviewLoop); return; } const now = new Date(Date.now()+offsetMs); let diff = (target - now)/1000; if(diff<=0){diff=0; if(!expired){expired=true; preview.classList.remove('blink'); preview.textContent=el('title').value || 'TERMINÉ'; expireNote.style.display='block'; showToast('Compte à rebours terminé',false);} }
 if(!expired){ expireNote.style.display='none'; let d=Math.floor(diff/86400); let h=Math.floor(diff%86400/3600); let m=Math.floor(diff%3600/60); let s=Math.floor(diff%60); // Choix format adaptatif
 let txt=''; if(d>0) txt=`${d}d ${h.toString().padStart(2,'0')}:${m.toString().padStart(2,'0')}`; else if(h>0) txt=`${h.toString().padStart(2,'0')}:${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`; else if(m>0) txt=`${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`; else txt=s.toString().padStart(2,'0'); preview.textContent=txt; // blinking last N seconds
 const bw = parseInt(el('blinkWindow').value)||10; if(d===0 && h===0 && m===0 && s<=bw){ if(ts-lastBlink>450){ lastBlink=ts; blink=!blink; preview.classList.toggle('blink',blink);} } else { preview.classList.remove('blink'); }
 }
 requestAnimationFrame(updatePreviewLoop); }
requestAnimationFrame(updatePreviewLoop);

function syncFieldsToPreview(){ titlePreview.textContent = (el('title').value||'').toUpperCase(); refreshTarget(); }

['title','date','time','fontIndex','textSize','blinkEnabled','blinkInterval','blinkWindow','marqueeEnabled','marqueeInterval','marqueeGap','marqueeMode','marqueeReturnInterval','marqueeBouncePauseLeft','marqueeBouncePauseRight','marqueeOneShotDelay','marqueeOneShotStopCenter','marqueeOneShotRestart','marqueeAccelEnabled','marqueeAccelStart','marqueeAccelEnd','marqueeAccelDuration','brightness','brightnessMode'].forEach(id=> el(id).addEventListener('input',()=>{ if(id==='brightnessMode'){ if(el('brightnessMode').value==='auto'){ el('brightnessHidden').value='-1'; } else { el('brightnessHidden').value=el('brightness').value; } } if(id==='brightness'){ if(el('brightnessMode').value==='manual'){ el('brightnessHidden').value=el('brightness').value; } } syncFieldsToPreview(); setSaving(true,'MODIFIÉ'); }));
el('colorPicker').addEventListener('input',e=>{ updateColor(e.target.value); setSaving(true,'MODIFIÉ'); });

// Ajustements rapides
document.querySelectorAll('.quick button[data-add], .quick button[data-set]').forEach(btn=>btn.addEventListener('click',()=>{
  if(!target) refreshTarget(); if(btn.dataset.set==='EOD'){ const n=new Date(); n.setHours(23,59,59,0); target=n; } else { const add=parseInt(btn.dataset.add,10); target = new Date((target?target:new Date()).getTime()+add*1000); }
  el('date').value = target.toISOString().split('T')[0]; const t = target.toTimeString().split(' ')[0]; el('time').value = t; syncFieldsToPreview(); showToast('Nouvelle cible '+ t); setSaving(true,'MODIFIÉ'); }));

// Bouton maintenant
el('btnNow').addEventListener('click',()=>{ const n=new Date(); el('date').value=n.toISOString().split('T')[0]; el('time').value=n.toTimeString().split(' ')[0]; syncFieldsToPreview(); showToast('Synchro à maintenant'); setSaving(true,'MODIFIÉ'); });

// Synchronisation de l'heure RTC côté ESP32 avec l'heure locale du navigateur
el('btnSyncTime').addEventListener('click',()=>{
  const now = new Date();
  const epoch = now.getTime(); // ms
  const tz = now.getTimezoneOffset(); // minutes (UTC = local + offset)
  showToast('Envoi heure locale...');
  fetch(`/syncTime?epoch=${epoch}&tz=${tz}`)
    .then(r=>r.json())
    .then(j=>{ if(j.status==='OK'){ showToast('RTC synchronisé'); setSaving(false,'PRÊT'); } else { showToast('Erreur sync',false);} })
    .catch(()=>showToast('Erreur réseau sync',false));
});

// Reset
el('btnReset').addEventListener('click',()=>{ if(!confirm('Réinitialiser les paramètres ?')) return; form.action='/reset'; form.submit(); });

// Soumission
form.addEventListener('submit',()=>{ setSaving(true,'ENVOI...'); el('btnSave').disabled=true; setTimeout(()=>{ setSaving(true,'SAUVEGARDE'); },150); });

// Chargement initial depuis l'ESP32
window.addEventListener('load',()=>{
  const now=new Date(); const tomorrow=new Date(now.getTime()+86400000); el('date').value=tomorrow.toISOString().split('T')[0]; el('time').value='23:59:59';
  fetch('/getSettings').then(r=>r.json()).then(d=>{ el('title').value=d.title; const ds=`${d.year}-${String(d.month).padStart(2,'0')}-${String(d.day).padStart(2,'0')}`; el('date').value=ds; const ts=`${String(d.hour).padStart(2,'0')}:${String(d.minute).padStart(2,'0')}:${String(d.second).padStart(2,'0')}`; el('time').value=ts; el('fontIndex').value=d.fontIndex; el('textSize').value=d.textSize; if(d.blinkEnabled!==undefined){ el('blinkEnabled').value = d.blinkEnabled ? '1' : '0'; } if(d.blinkIntervalMs!==undefined){ el('blinkInterval').value = d.blinkIntervalMs; } if(d.blinkWindow!==undefined){ el('blinkWindow').value = d.blinkWindow; } if(d.marqueeEnabled!==undefined){ el('marqueeEnabled').value = d.marqueeEnabled ? '1':'0'; } if(d.marqueeIntervalMs!==undefined){ el('marqueeInterval').value = d.marqueeIntervalMs; } if(d.marqueeGap!==undefined){ el('marqueeGap').value = d.marqueeGap; } if(d.marqueeMode!==undefined){ el('marqueeMode').value = d.marqueeMode; } if(d.marqueeReturnIntervalMs!==undefined){ el('marqueeReturnInterval').value = d.marqueeReturnIntervalMs; } if(d.marqueeBouncePauseLeftMs!==undefined){ el('marqueeBouncePauseLeft').value = d.marqueeBouncePauseLeftMs; } if(d.marqueeBouncePauseRightMs!==undefined){ el('marqueeBouncePauseRight').value = d.marqueeBouncePauseRightMs; } if(d.marqueeOneShotDelayMs!==undefined){ el('marqueeOneShotDelay').value = d.marqueeOneShotDelayMs; } if(d.marqueeOneShotStopCenter!==undefined){ el('marqueeOneShotStopCenter').value = d.marqueeOneShotStopCenter? '1':'0'; } if(d.marqueeOneShotRestartSec!==undefined){ el('marqueeOneShotRestart').value = d.marqueeOneShotRestartSec; } if(d.marqueeAccelEnabled!==undefined){ el('marqueeAccelEnabled').value = d.marqueeAccelEnabled? '1':'0'; } if(d.marqueeAccelStartIntervalMs!==undefined){ el('marqueeAccelStart').value = d.marqueeAccelStartIntervalMs; } if(d.marqueeAccelEndIntervalMs!==undefined){ el('marqueeAccelEnd').value = d.marqueeAccelEndIntervalMs; } if(d.marqueeAccelDurationMs!==undefined){ el('marqueeAccelDuration').value = d.marqueeAccelDurationMs; } if(d.brightness!==undefined){ if(parseInt(d.brightness,10)===-1){ el('brightnessMode').value='auto'; el('brightnessHidden').value='-1'; } else { el('brightnessMode').value='manual'; el('brightness').value=d.brightness; el('brightnessHidden').value=d.brightness; } } const color=rgbToHex(d.colorR,d.colorG,d.colorB); el('colorPicker').value=color; updateColor(color); syncFieldsToPreview(); setSaving(false,'PRÊT'); showToast('Paramètres chargés'); }).catch(()=>{ showToast('Échec chargement paramètres',false); setSaving(false,'HORS LIGNE'); syncFieldsToPreview(); });
});
</script>
</body></html>
)rawliteral";
  // Ancien script legacy supprimé (désormais encapsulé dans MAIN_page)
// Note: La fonction display_update_enable() a été intégrée dans setup()
// pour une meilleure gestion avec FreeRTOS

// Calcul du temps restant
void updateCountdown(int &days, int &hours, int &minutes, int &seconds) {
  DateTime now = rtc.now();
  
  // Vérifier si le countdown est expiré
  if (now >= countdownTarget) {
    countdownExpired = true;
    days = hours = minutes = 0;
    seconds = 0;
    blinkLastSeconds = false; // Pas de clignotement une fois expiré
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
  
  // Activer le clignotement uniquement sur la fenêtre configurée des dernières secondes
  int bw = blinkWindowSeconds;
  if (bw < 1) bw = 1;
  blinkLastSeconds = (days == 0 && hours == 0 && minutes == 0 && seconds <= bw);
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
  static uint16_t cachedTextPixelWidth = 0; // pour calcul marquee

  // Recalculer systématiquement la couleur utilisateur (évite usage d'une valeur obsolète)
  int localR = countdownColorR;
  int localG = countdownColorG;
  int localB = countdownColorB;
  if (localR < 0) localR = 0; if (localR > 255) localR = 255;
  if (localG < 0) localG = 0; if (localG > 255) localG = 255;
  if (localB < 0) localB = 0; if (localB > 255) localB = 255;
  uint16_t userColor = display.color565(localR, localG, localB);

  // Clignotement configurable des 10 dernières secondes (si activé)
  uint16_t displayColor = userColor;
  if (!countdownExpired && blinkEnabled && blinkLastSeconds) {
    unsigned long currentTime = millis();
    int localInterval = blinkIntervalMs;
    if (localInterval < 50) localInterval = 50;       // bornes de sécurité
    if (localInterval > 5000) localInterval = 5000;
    if (currentTime - lastBlinkTime >= (unsigned long)localInterval) {
      lastBlinkTime = currentTime;
      blinkState = !blinkState;
    }
    displayColor = blinkState ? userColor : myBLACK;
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
    cachedTextPixelWidth = w; // conserver largeur
    cachedY = (TOTAL_HEIGHT - h) / 2 - y1;

    // Décider activation selon le mode
    marqueeTextWidth = w;
    marqueeActive = false;
    marqueeOneShotDone = (marqueeMode == 3) ? marqueeOneShotDone : false; // réinitialiser si changement de texte
    if (marqueeEnabled) {
      switch (marqueeMode) {
        case 0: // Auto (continuous gauche si dépasse)
          if (w > TOTAL_WIDTH) marqueeActive = true;
          break;
        case 1: // Toujours gauche
          marqueeActive = true;
          break;
        case 2: // Aller-Retour seulement si dépasse
          if (w > TOTAL_WIDTH) marqueeActive = true;
          break;
        case 3: // Une seule fois (si dépasse et pas déjà fini)
          if (w > TOTAL_WIDTH && !marqueeOneShotDone) marqueeActive = true;
          break;
      }
    }

    if (marqueeActive) {
      marqueeInPause = false;
      marqueePauseUntil = 0;
      if (marqueeMode == 2) { // bounce
        // On démarre à offset 0 (texte aligné à gauche visible) puis direction vers la gauche => rapidement sortira et reviendra
        marqueeOffset = 0;
        marqueeDirection = -1;
        // pause initiale gauche
        if (marqueeBouncePauseLeftMs > 0) { marqueeInPause = true; marqueePauseUntil = millis() + marqueeBouncePauseLeftMs; }
      } else if (marqueeMode == 1 || marqueeMode == 0) {
        marqueeOffset = TOTAL_WIDTH; // continuous depuis la droite
      } else if (marqueeMode == 3) { // one-shot centré d'abord
        marqueeOneShotCenterPhase = true;
        marqueeOneShotStart = millis();
        cachedX = (TOTAL_WIDTH - w) / 2 - x1; // centré
      }
      lastMarqueeStep = millis();
      marqueeCycleStartMs = millis();
    } else {
      cachedX = (TOTAL_WIDTH - w) / 2 - x1; // centré
    }

    // Si texte court (pas de marquee) on définit cachedX, sinon il sera dynamique
    if (!marqueeActive) {
      cachedX = (TOTAL_WIDTH - w) / 2 - x1;
    }
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

  // Gestion de l'avancement du marquee (hors section critique)
  if (marqueeActive) {
    unsigned long nowMs = millis();
    // ONE SHOT: phase centrée -> attendre délai puis lancer scroll
    if (marqueeMode == 3 && marqueeOneShotCenterPhase) {
      if (nowMs - marqueeOneShotStart >= (unsigned long)marqueeOneShotDelayMs) {
        marqueeOneShotCenterPhase = false;
        marqueeOffset = TOTAL_WIDTH; // début scroll
        lastMarqueeStep = nowMs;
        marqueeCycleStartMs = nowMs;
      } else {
        // ne rien faire pendant la phase centrée
      }
    } else if (marqueeMode == 3 && marqueeOneShotCenterPhase == false && marqueeOneShotDone) {
      // terminé : si restart demandé
      if (marqueeOneShotRestartSec > 0 && nowMs >= marqueeOneShotRestartAt && marqueeOneShotRestartAt != 0) {
        // relance cycle
        marqueeOneShotDone = false;
        marqueeOneShotCenterPhase = true;
        marqueeOneShotStart = nowMs;
        lastMarqueeStep = nowMs;
        marqueeCycleStartMs = nowMs;
      }
    } else {
      // BOUNCE: gestion pause
      if (marqueeMode == 2 && marqueeInPause) {
        if (nowMs >= marqueePauseUntil) {
          marqueeInPause = false;
          lastMarqueeStep = nowMs; // reset timer pour éviter saut
          marqueeCycleStartMs = nowMs; // nouveau cycle après pause
        }
      }
      int forwardInt = marqueeIntervalMs; if (forwardInt < 5) forwardInt = 5; if (forwardInt > 500) forwardInt = 500;
      int returnInt = marqueeReturnIntervalMs; if (returnInt < 5) returnInt = 5; if (returnInt > 500) returnInt = 500;

      // Accélération progressive
      if (marqueeAccelEnabled) {
        int startI = marqueeAccelStartIntervalMs; if (startI < 5) startI = 5; if (startI > 500) startI = 500;
        int endI = marqueeAccelEndIntervalMs; if (endI < 5) endI = 5; if (endI > 500) endI = 500;
        unsigned long elapsed = nowMs - marqueeCycleStartMs;
        float t = (marqueeAccelDurationMs <= 0) ? 1.0f : (float)elapsed / (float)marqueeAccelDurationMs;
        if (t > 1.0f) t = 1.0f;
        int interp = startI + (int)((endI - startI) * t);
        // Pour bounce: appliquer sur direction actuelle (séparément pour retour si différent)
        if (marqueeMode == 2) {
          if (marqueeDirection == -1) forwardInt = interp; else returnInt = interp; // direction -1 = vers gauche (forward logique), 1 = retour
        } else if (marqueeMode == 0 || marqueeMode == 1 || marqueeMode == 3) {
          forwardInt = interp;
        }
      }
      int effectiveInt = forwardInt;
      if (marqueeMode == 2 && marqueeDirection == 1) effectiveInt = returnInt; // retour
      if (!marqueeInPause && nowMs - lastMarqueeStep >= (unsigned long)effectiveInt) {
        lastMarqueeStep = nowMs;
        if (marqueeMode == 2) { // bounce
          marqueeOffset += marqueeDirection; // -1 gauche, +1 droite
          int minX = TOTAL_WIDTH - marqueeTextWidth; // négatif
          if (marqueeOffset <= minX) { marqueeOffset = minX; marqueeDirection = 1; if (marqueeBouncePauseRightMs>0){ marqueeInPause=true; marqueePauseUntil=nowMs+marqueeBouncePauseRightMs; } marqueeCycleStartMs = nowMs; }
          if (marqueeOffset >= 0) { marqueeOffset = 0; marqueeDirection = -1; if (marqueeBouncePauseLeftMs>0){ marqueeInPause=true; marqueePauseUntil=nowMs+marqueeBouncePauseLeftMs; } marqueeCycleStartMs = nowMs; }
        } else if (marqueeMode == 3) { // one-shot scrolling phase
          if (!marqueeOneShotDone && !marqueeOneShotCenterPhase) {
            marqueeOffset--; // vers la gauche
            if (marqueeOffset + marqueeTextWidth < 0) {
              marqueeActive = false; marqueeOneShotDone = true;
              if (marqueeOneShotStopCenter) {
                // recadrer
                cachedX = (TOTAL_WIDTH - marqueeTextWidth) / 2;
              }
              if (marqueeOneShotRestartSec > 0) {
                marqueeOneShotRestartAt = nowMs + (unsigned long)marqueeOneShotRestartSec * 1000UL;
              }
            }
          }
        } else { // continuous modes
          marqueeOffset--;
          int localGap = marqueeGap; if (localGap < 4) localGap = 4; if (localGap > 256) localGap = 256;
          if (marqueeOffset + marqueeTextWidth < 0) {
            marqueeOffset = TOTAL_WIDTH + localGap; // boucle
            marqueeCycleStartMs = nowMs; // nouveau cycle -> reset accel
          }
        }
      }
    }
  }

  // Section critique minimale : effacement + écriture tampon
  portENTER_CRITICAL(&timerMux);
  display.clearDisplay();
  // Toujours la couleur choisie (même si expiré) conformément à la demande
  display.setTextColor(displayColor);
  if (marqueeActive) {
    if (marqueeMode == 2) { // bounce
      display.setCursor(marqueeOffset, cachedY);
      display.print(lastText);
    } else if (marqueeMode == 3) { // one-shot
      if (marqueeOneShotCenterPhase) {
        // déjà centré via cachedX/cachedY dans phase setup, mais on redessine centré
        display.setCursor((TOTAL_WIDTH - marqueeTextWidth)/2, cachedY);
        display.print(lastText);
      } else if (marqueeOneShotDone && marqueeOneShotStopCenter) {
        display.setCursor((TOTAL_WIDTH - marqueeTextWidth)/2, cachedY);
        display.print(lastText);
      } else {
        display.setCursor(marqueeOffset, cachedY);
        display.print(lastText);
      }
    } else {
      // continuous / always
      int drawX = marqueeOffset;
      display.setCursor(drawX, cachedY);
      display.print(lastText);
      int localGap = marqueeGap; if (localGap < 4) localGap = 4; if (localGap > 256) localGap = 256;
      int secondX = marqueeOffset + marqueeTextWidth + localGap;
      if (secondX < TOTAL_WIDTH) {
        display.setCursor(secondX, cachedY);
        display.print(lastText);
      }
    }
  } else {
    display.setCursor(cachedX, cachedY);
    display.print(lastText);
  }
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
  blinkEnabled = preferences.getBool("blinkEn", true);
  blinkIntervalMs = preferences.getInt("blinkInt", 500);
  blinkWindowSeconds = preferences.getInt("blinkWin", 10);
  marqueeEnabled = preferences.getBool("mqEn", true);
  marqueeIntervalMs = preferences.getInt("mqInt", 40);
  marqueeGap = preferences.getInt("mqGap", 24);
  marqueeMode = preferences.getInt("mqMode", 0);
  marqueeReturnIntervalMs = preferences.getInt("mqRetInt", 60);
  marqueeBouncePauseLeftMs = preferences.getInt("mqPL", 400);
  marqueeBouncePauseRightMs = preferences.getInt("mqPR", 400);
  marqueeOneShotDelayMs = preferences.getInt("mqOsDelay", 800);
  marqueeOneShotStopCenter = preferences.getBool("mqOsStopC", true);
  marqueeOneShotRestartSec = preferences.getInt("mqOsRst", 0);
  marqueeAccelEnabled = preferences.getBool("mqAccEn", false);
  marqueeAccelStartIntervalMs = preferences.getInt("mqAccSt", 80);
  marqueeAccelEndIntervalMs = preferences.getInt("mqAccEnd", 30);
  marqueeAccelDurationMs = preferences.getInt("mqAccDur", 3000);
  displayBrightness = preferences.getInt("bright", -1);
    
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
  // Appliquer brightness (auto si -1)
  int effectiveBrightness;
  if (displayBrightness < 0) {
    effectiveBrightness = 150;
    if (MATRIX_PANELS_X > 2) effectiveBrightness = 100;
    if (MATRIX_PANELS_X > 4) effectiveBrightness = 80;
    if (MATRIX_PANELS_X > 6) effectiveBrightness = 60;
  } else {
    effectiveBrightness = displayBrightness;
  }
  display.setBrightness(effectiveBrightness);
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
  preferences.putBool("blinkEn", blinkEnabled);
  preferences.putInt("blinkInt", blinkIntervalMs);
  preferences.putInt("blinkWin", blinkWindowSeconds);
  preferences.putBool("mqEn", marqueeEnabled);
  preferences.putInt("mqInt", marqueeIntervalMs);
  preferences.putInt("mqGap", marqueeGap);
  preferences.putInt("mqMode", marqueeMode);
  preferences.putInt("mqRetInt", marqueeReturnIntervalMs);
  preferences.putInt("mqPL", marqueeBouncePauseLeftMs);
  preferences.putInt("mqPR", marqueeBouncePauseRightMs);
  preferences.putInt("mqOsDelay", marqueeOneShotDelayMs);
  preferences.putBool("mqOsStopC", marqueeOneShotStopCenter);
  preferences.putInt("mqOsRst", marqueeOneShotRestartSec);
  preferences.putBool("mqAccEn", marqueeAccelEnabled);
  preferences.putInt("mqAccSt", marqueeAccelStartIntervalMs);
  preferences.putInt("mqAccEnd", marqueeAccelEndIntervalMs);
  preferences.putInt("mqAccDur", marqueeAccelDurationMs);
  preferences.putInt("bright", displayBrightness);
    
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
  json += "\"fontIndex\":" + String(fontIndex) + ",";
  json += "\"blinkEnabled\":" + String(blinkEnabled ? 1 : 0) + ",";
  json += "\"blinkIntervalMs\":" + String(blinkIntervalMs) + ",";
  json += "\"blinkWindow\":" + String(blinkWindowSeconds) + ",";
  json += "\"marqueeEnabled\":" + String(marqueeEnabled ? 1 : 0) + ",";
  json += "\"marqueeIntervalMs\":" + String(marqueeIntervalMs) + ",";
  json += "\"marqueeGap\":" + String(marqueeGap) + ",";
  json += "\"marqueeMode\":" + String(marqueeMode) + ",";
  json += "\"marqueeReturnIntervalMs\":" + String(marqueeReturnIntervalMs) + ",";
  json += "\"marqueeBouncePauseLeftMs\":" + String(marqueeBouncePauseLeftMs) + ",";
  json += "\"marqueeBouncePauseRightMs\":" + String(marqueeBouncePauseRightMs) + ",";
  json += "\"marqueeOneShotDelayMs\":" + String(marqueeOneShotDelayMs) + ",";
  json += "\"marqueeOneShotStopCenter\":" + String(marqueeOneShotStopCenter ? 1:0) + ",";
  json += "\"marqueeOneShotRestartSec\":" + String(marqueeOneShotRestartSec) + ",";
  json += "\"marqueeAccelEnabled\":" + String(marqueeAccelEnabled ? 1:0) + ",";
  json += "\"marqueeAccelStartIntervalMs\":" + String(marqueeAccelStartIntervalMs) + ",";
  json += "\"marqueeAccelEndIntervalMs\":" + String(marqueeAccelEndIntervalMs) + ",";
  json += "\"marqueeAccelDurationMs\":" + String(marqueeAccelDurationMs);
  json += ",\"brightness\":" + String(displayBrightness);
  json += "}";
  
  server.send(200, "application/json", json);
}

// Synchronisation de l'heure depuis le navigateur (client envoie son epoch ms + offset minutes)
void handleSyncTime() {
  if (!server.hasArg("epoch")) {
    server.send(400, "application/json", "{\"status\":\"ERR\",\"msg\":\"missing epoch\"}");
    return;
  }
  String epochStr = server.arg("epoch");
  String tzStr = server.hasArg("tz") ? server.arg("tz") : String("0");
  unsigned long long epochMs = strtoull(epochStr.c_str(), nullptr, 10);
  long tzMinutes = tzStr.toInt(); // JS getTimezoneOffset(): UTC = local + offset
  // epochMs est en UTC. Nous voulons régler le RTC sur l'heure locale perçue par l'utilisateur.
  time_t epochSec = (time_t)(epochMs / 1000ULL);
  // local = UTC - offsetMinutes*60 (car offset = UTC - local)
  time_t localSec = epochSec - (tzMinutes * 60L);
  DateTime localDT(localSec);
  // Protection simple : vérifier année raisonnable
  if (localDT.year() < 2020 || localDT.year() > 2099) {
    server.send(400, "application/json", "{\"status\":\"ERR\",\"msg\":\"invalid time\"}");
    return;
  }
  // Ajuster le RTC (mutex countdown pas nécessaire pour simple set, mais on peut briefer)
  rtc.adjust(localDT);
  // Recalculer le countdownTarget à partir des paramètres sauvegardés (utc inchangé) — laisser tel quel.
  // Réponse
  String resp = "{\"status\":\"OK\",\"set\":\"";
  resp += String(localDT.year()) + "-" + String(localDT.month()) + "-" + String(localDT.day()) + "T" + String(localDT.hour()) + ":" + String(localDT.minute()) + ":" + String(localDT.second()) + "\"}";
  server.send(200, "application/json", resp);
  Serial.printf("RTC synchronized to client local time: %04d-%02d-%02d %02d:%02d:%02d (tz offset %ld mn)\n", localDT.year(), localDT.month(), localDT.day(), localDT.hour(), localDT.minute(), localDT.second(), tzMinutes);
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
  if (server.hasArg("blinkEnabled")) {
    blinkEnabled = server.arg("blinkEnabled").toInt() != 0;
  }
  if (server.hasArg("blinkInterval")) {
    int bi = server.arg("blinkInterval").toInt();
    if (bi < 50) bi = 50; if (bi > 5000) bi = 5000; // bornes logiques
    blinkIntervalMs = bi;
  }
  if (server.hasArg("blinkWindow")) {
    int bw = server.arg("blinkWindow").toInt();
    if (bw < 1) bw = 1; if (bw > 3600) bw = 3600;
    blinkWindowSeconds = bw;
  }
  if (server.hasArg("marqueeEnabled")) {
    marqueeEnabled = server.arg("marqueeEnabled").toInt() != 0;
  }
  if (server.hasArg("marqueeInterval")) {
    int mi = server.arg("marqueeInterval").toInt();
    if (mi < 5) mi = 5; if (mi > 500) mi = 500;
    marqueeIntervalMs = mi;
  }
  if (server.hasArg("marqueeGap")) {
    int mg = server.arg("marqueeGap").toInt();
    if (mg < 4) mg = 4; if (mg > 256) mg = 256;
    marqueeGap = mg;
  }
  if (server.hasArg("marqueeMode")) {
    int mm = server.arg("marqueeMode").toInt();
    if (mm < 0) mm = 0; if (mm > 3) mm = 3;
    marqueeMode = mm;
    // réinitialiser états spécifiques
    marqueeOneShotDone = false;
  }
  if (server.hasArg("marqueeReturnInterval")) {
    int ri = server.arg("marqueeReturnInterval").toInt();
    if (ri < 5) ri = 5; if (ri > 500) ri = 500;
    marqueeReturnIntervalMs = ri;
  }
  if (server.hasArg("marqueeBouncePauseLeft")) {
    int bpL = server.arg("marqueeBouncePauseLeft").toInt();
    if (bpL < 0) bpL = 0; if (bpL > 5000) bpL = 5000;
    marqueeBouncePauseLeftMs = bpL;
  }
  if (server.hasArg("marqueeBouncePauseRight")) {
    int bpR = server.arg("marqueeBouncePauseRight").toInt();
    if (bpR < 0) bpR = 0; if (bpR > 5000) bpR = 5000;
    marqueeBouncePauseRightMs = bpR;
  }
  if (server.hasArg("marqueeOneShotDelay")) {
    int od = server.arg("marqueeOneShotDelay").toInt();
    if (od < 0) od = 0; if (od > 10000) od = 10000;
    marqueeOneShotDelayMs = od;
  }
  if (server.hasArg("marqueeOneShotStopCenter")) {
    marqueeOneShotStopCenter = server.arg("marqueeOneShotStopCenter") == "1";
  }
  if (server.hasArg("marqueeOneShotRestart")) {
    int rs = server.arg("marqueeOneShotRestart").toInt();
    if (rs < 0) rs = 0; if (rs > 86400) rs = 86400;
    marqueeOneShotRestartSec = rs;
  }
  if (server.hasArg("marqueeAccelEnabled")) {
    marqueeAccelEnabled = server.arg("marqueeAccelEnabled") == "1";
  }
  if (server.hasArg("marqueeAccelStart")) {
    int as = server.arg("marqueeAccelStart").toInt();
    if (as < 5) as = 5; if (as > 500) as = 500; marqueeAccelStartIntervalMs = as;
  }
  if (server.hasArg("marqueeAccelEnd")) {
    int ae = server.arg("marqueeAccelEnd").toInt();
    if (ae < 5) ae = 5; if (ae > 500) ae = 500; marqueeAccelEndIntervalMs = ae;
  }
  if (server.hasArg("marqueeAccelDuration")) {
    int ad = server.arg("marqueeAccelDuration").toInt();
    if (ad < 50) ad = 50; if (ad > 600000) ad = 600000; marqueeAccelDurationMs = ad;
  }
  if (server.hasArg("brightness")) {
    int b = server.arg("brightness").toInt();
    if (b < -1) b = -1; if (b > 255) b = 255;
    displayBrightness = b;
  }
  
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

  // Ajuster luminosité immédiatement
  int effectiveBrightness;
  if (displayBrightness < 0) {
    effectiveBrightness = 150;
    if (MATRIX_PANELS_X > 2) effectiveBrightness = 100;
    if (MATRIX_PANELS_X > 4) effectiveBrightness = 80;
    if (MATRIX_PANELS_X > 6) effectiveBrightness = 60;
  } else {
    effectiveBrightness = displayBrightness;
  }
  display.setBrightness(effectiveBrightness);
  
  Serial.println("Settings updated:");
  Serial.printf("Target: %d-%02d-%02d %02d:%02d:%02d\n", 
                countdownYear, countdownMonth, countdownDay,
                countdownHour, countdownMinute, countdownSecond);
  Serial.printf("Title: %s\n", countdownTitle);
  Serial.printf("Text Size: %d, Font: %d\n", countdownTextSize, fontIndex);
  Serial.printf("Color (RGB): %d,%d,%d\n", countdownColorR, countdownColorG, countdownColorB);
  Serial.printf("Blink: enabled=%d interval=%dms window=%ds\n", blinkEnabled, blinkIntervalMs, blinkWindowSeconds);
  Serial.printf("Marquee: en=%d mode=%d fwdInt=%d retInt=%d gap=%d LPause=%d RPause=%d accel=%d start=%d end=%d dur=%d oneDelay=%d oneStopC=%d oneRst=%d\n",
    marqueeEnabled, marqueeMode, marqueeIntervalMs, marqueeReturnIntervalMs, marqueeGap,
    marqueeBouncePauseLeftMs, marqueeBouncePauseRightMs, marqueeAccelEnabled?1:0,
    marqueeAccelStartIntervalMs, marqueeAccelEndIntervalMs, marqueeAccelDurationMs,
    marqueeOneShotDelayMs, marqueeOneShotStopCenter?1:0, marqueeOneShotRestartSec);
  Serial.printf("Brightness setting: %d ( -1 = auto )\n", displayBrightness);
  
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
  server.on("/syncTime", HTTP_GET, handleSyncTime);
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
  // Affichage initial (y compris si déjà expiré au démarrage)
  {
    int d=0,h=0,m=0,s=0;
    if (countdownMutex && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(50))) {
      updateCountdown(d,h,m,s);
      updateDisplayFormat(d,h,m,s);
      xSemaphoreGive(countdownMutex);
    }
    if (displayMutex && xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50))) {
      displayFullscreenCountdown(d,h,m,s);
      xSemaphoreGive(displayMutex);
    }
  }
  
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
  