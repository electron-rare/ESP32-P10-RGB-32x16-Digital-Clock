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
const char* ap_ssid = "ESP32_Countdown";
const char* ap_password = "countdown123";

// Clé de sécurité pour l'interface web
#define KEY_TXT "countdown2025"

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

// Prototypes des tâches
void DisplayTask(void * parameter);
void WebServerTask(void * parameter);
void CountdownTask(void * parameter);
void NetworkTask(void * parameter);

// Variables pour le countdown
DateTime countdownTarget;
bool countdownExpired = false;
bool settingsLoaded = false; // Indicateur que les paramètres sont chargés
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
int fontIndex = 0; // 0=Default, 1=Sans, 2=Sans Bold, 3=Mono (pour le countdown)
int endTextFontIndex = 0; // Police pour le texte de fin
uint16_t countdownColor;

// Format d'affichage (0=jours, 1=heures, 2=minutes, 3=secondes uniquement)
int displayFormat = 0;

// HTML Page
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 P10 Countdown - Configuration</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css" rel="stylesheet">
    <style>
        :root {
            --primary-color: #6366f1;
            --primary-dark: #4f46e5;
            --secondary-color: #10b981;
            --accent-color: #f59e0b;
            --danger-color: #ef4444;
            --background: #f8fafc;
            --surface: #ffffff;
            --surface-dark: #f1f5f9;
            --text-primary: #1e293b;
            --text-secondary: #64748b;
            --border: #e2e8f0;
            --shadow: 0 4px 6px -1px rgb(0 0 0 / 0.1), 0 2px 4px -2px rgb(0 0 0 / 0.1);
            --shadow-lg: 0 10px 15px -3px rgb(0 0 0 / 0.1), 0 4px 6px -4px rgb(0 0 0 / 0.1);
            --border-radius: 12px;
            --transition: all 0.2s ease-in-out;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            color: var(--text-primary);
            line-height: 1.6;
        }

        .header {
            text-align: center;
            padding: 2rem 1rem;
            color: white;
        }

        .header h1 {
            font-size: 2.5rem;
            font-weight: 700;
            margin-bottom: 0.5rem;
            text-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }

        .header p {
            font-size: 1.1rem;
            opacity: 0.9;
            font-weight: 300;
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 0 1rem 2rem;
        }

        .card {
            background: var(--surface);
            border-radius: var(--border-radius);
            box-shadow: var(--shadow-lg);
            overflow: hidden;
            margin-bottom: 1.5rem;
            border: 1px solid var(--border);
        }

        .card-header {
            background: var(--surface-dark);
            padding: 1.5rem;
            border-bottom: 1px solid var(--border);
        }

        .card-header h2 {
            font-size: 1.25rem;
            font-weight: 600;
            color: var(--text-primary);
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        .card-content {
            padding: 1.5rem;
        }

        .countdown-preview {
            background: linear-gradient(135deg, #1e293b 0%, #334155 100%);
            border-radius: var(--border-radius);
            padding: 2rem;
            text-align: center;
            margin-bottom: 1.5rem;
            position: relative;
            overflow: hidden;
        }

        .countdown-preview::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: radial-gradient(circle at 30% 20%, rgba(99, 102, 241, 0.3) 0%, transparent 50%);
            pointer-events: none;
        }

        .countdown-display {
            font-family: 'Courier New', monospace;
            font-size: 2.5rem;
            font-weight: bold;
            color: #00ff00;
            text-shadow: 0 0 10px rgba(0, 255, 0, 0.5);
            position: relative;
            z-index: 1;
        }

        .countdown-title {
            font-size: 1rem;
            color: rgba(255, 255, 255, 0.8);
            margin-top: 0.5rem;
            position: relative;
            z-index: 1;
        }

        .form-grid {
            display: grid;
            gap: 1rem;
        }

        .form-group {
            margin-bottom: 1.5rem;
        }

        .form-row {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 1rem;
        }

        @media (max-width: 640px) {
            .form-row {
                grid-template-columns: 1fr;
            }
        }

        .form-label {
            display: block;
            font-weight: 500;
            color: var(--text-primary);
            margin-bottom: 0.5rem;
            font-size: 0.875rem;
        }

        .form-input, .form-select {
            width: 100%;
            padding: 0.75rem;
            border: 2px solid var(--border);
            border-radius: 8px;
            background: var(--surface);
            color: var(--text-primary);
            font-size: 0.875rem;
            transition: var(--transition);
        }

        .form-input:focus, .form-select:focus {
            outline: none;
            border-color: var(--primary-color);
            box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.1);
        }

        .color-picker-group {
            display: flex;
            align-items: center;
            gap: 1rem;
        }

        .color-input {
            width: 60px;
            height: 40px;
            border: 2px solid var(--border);
            border-radius: 8px;
            cursor: pointer;
            transition: var(--transition);
        }

        .color-input:hover {
            transform: scale(1.05);
        }

        .color-preview {
            width: 40px;
            height: 40px;
            border-radius: 8px;
            border: 2px solid var(--border);
            transition: var(--transition);
        }

        .color-presets {
            display: flex;
            gap: 0.5rem;
            margin-top: 0.5rem;
            flex-wrap: wrap;
        }

        .color-preset {
            width: 32px;
            height: 32px;
            border-radius: 6px;
            border: 2px solid var(--border);
            cursor: pointer;
            transition: var(--transition);
        }

        .color-preset:hover {
            transform: scale(1.1);
            border-color: var(--primary-color);
        }

        .button-group {
            display: flex;
            gap: 1rem;
            margin-top: 2rem;
        }

        .btn {
            flex: 1;
            padding: 0.875rem 1.5rem;
            border: none;
            border-radius: 8px;
            font-weight: 600;
            font-size: 0.875rem;
            cursor: pointer;
            transition: var(--transition);
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 0.5rem;
            text-decoration: none;
        }

        .btn-primary {
            background: var(--primary-color);
            color: white;
        }

        .btn-primary:hover {
            background: var(--primary-dark);
            transform: translateY(-1px);
            box-shadow: var(--shadow);
        }

        .btn-secondary {
            background: var(--surface);
            color: var(--text-primary);
            border: 2px solid var(--border);
        }

        .btn-secondary:hover {
            background: var(--surface-dark);
            border-color: var(--primary-color);
        }

        .btn-danger {
            background: var(--danger-color);
            color: white;
        }

        .btn-danger:hover {
            background: #dc2626;
            transform: translateY(-1px);
        }

        .btn-sm {
            padding: 0.5rem 1rem;
            font-size: 0.8rem;
        }

        .section-buttons {
            margin-top: 1.5rem;
            padding-top: 1rem;
            border-top: 1px solid var(--border);
            display: flex;
            justify-content: flex-end;
        }

        .status-indicator {
            display: inline-flex;
            align-items: center;
            gap: 0.5rem;
            padding: 0.5rem 1rem;
            border-radius: 20px;
            font-size: 0.875rem;
            font-weight: 500;
            margin-bottom: 1rem;
        }

        .status-connected {
            background: rgba(16, 185, 129, 0.1);
            color: var(--secondary-color);
        }

        .footer {
            text-align: center;
            padding: 2rem 1rem;
            color: rgba(255, 255, 255, 0.8);
            font-size: 0.875rem;
        }

        .info-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 1rem;
            margin-bottom: 1.5rem;
        }

        .info-card {
            background: var(--surface-dark);
            padding: 1rem;
            border-radius: 8px;
            text-align: center;
        }

        .info-value {
            font-size: 1.5rem;
            font-weight: 700;
            color: var(--primary-color);
        }

        .info-label {
            font-size: 0.75rem;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }

        @media (max-width: 640px) {
            .header h1 {
                font-size: 2rem;
            }
            
            .countdown-display {
                font-size: 1.8rem;
            }
            
            .button-group {
                flex-direction: column;
            }
            
            .container {
                padding: 0 0.5rem 1rem;
            }
        }

        .loading {
            opacity: 0.6;
            pointer-events: none;
        }

        .fade-in {
            animation: fadeIn 0.5s ease-in-out;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }
    </style>
</head>
<body>
    <div class="header">
        <h1><i class="fas fa-clock"></i> ESP32 P10 Countdown</h1>
        <p>Configuration du compte à rebours LED</p>
    </div>

    <div class="container">
        <div class="status-indicator status-connected">
            <i class="fas fa-wifi"></i>
            Connecté - IP: 192.168.1.1
        </div>

        <!-- Formulaire de configuration -->
        <form id="settingsForm" action="/settings" method="POST">
            <!-- Configuration de la date et heure -->
            <div class="card fade-in">
                <div class="card-header">
                    <h2><i class="fas fa-calendar-alt"></i> Date et Heure Cible</h2>
                </div>
                <div class="card-content">
                    <div class="form-group">
                        <label class="form-label" for="title">Titre du Compte à Rebours</label>
                        <input type="text" id="title" name="title" class="form-input" maxlength="50" placeholder="Entrez le titre..." required>
                    </div>
                    
                    <div class="form-row">
                        <div class="form-group">
                            <label class="form-label" for="date">Date</label>
                            <input type="date" id="date" name="date" class="form-input" required>
                        </div>
                        <div class="form-group">
                            <label class="form-label" for="time">Heure</label>
                            <input type="time" id="time" name="time" class="form-input" step="1" required>
                        </div>
                    </div>
                    
                    <!-- Bouton pour cette section -->
                    <div class="section-buttons">
                        <button type="button" class="btn btn-primary btn-sm" onclick="saveDateTime()">
                            <i class="fas fa-save"></i> Enregistrer Date & Heure
                        </button>
                    </div>
                </div>
            </div>

            <!-- Configuration de l'apparence -->
            <div class="card fade-in">
                <div class="card-header">
                    <h2><i class="fas fa-palette"></i> Apparence</h2>
                </div>
                <div class="card-content">
                    <div class="form-row">
                        <div class="form-group">
                            <label class="form-label" for="fontIndex">Police du Countdown</label>
                            <select id="fontIndex" name="fontIndex" class="form-select">
                                <option value="0">Standard</option>
                                <option value="1">Sans Serif</option>
                                <option value="2">Sans Serif Bold</option>
                                <option value="3">Monospace</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label class="form-label" for="endTextFontIndex">Police du Texte de Fin</label>
                            <select id="endTextFontIndex" name="endTextFontIndex" class="form-select">
                                <option value="0">Standard</option>
                                <option value="1">Sans Serif</option>
                                <option value="2">Sans Serif Bold</option>
                                <option value="3">Monospace</option>
                            </select>
                        </div>
                    </div>
                    <div class="form-row">
                        <div class="form-group">
                            <label class="form-label" for="textSize">Taille du texte</label>
                            <select id="textSize" name="textSize" class="form-select">
                                <option value="1">Petite</option>
                                <option value="2">Moyenne</option>
                                <option value="3">Grande</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <!-- Espace pour équilibrer la mise en page -->
                        </div>
                    </div>
                    
                    <div class="form-group">
                        <label class="form-label" for="colorPicker">Couleur d'affichage</label>
                        <div class="color-picker-group">
                            <input type="color" id="colorPicker" class="color-input" value="#00ff00">
                            <div class="color-preview" id="colorPreview"></div>
                            <span id="colorValue">#00FF00</span>
                        </div>
                        
                        <div class="color-presets">
                            <div class="color-preset" style="background-color: #00ff00;" data-color="#00ff00" title="Vert"></div>
                            <div class="color-preset" style="background-color: #ff0000;" data-color="#ff0000" title="Rouge"></div>
                            <div class="color-preset" style="background-color: #0000ff;" data-color="#0000ff" title="Bleu"></div>
                            <div class="color-preset" style="background-color: #ffff00;" data-color="#ffff00" title="Jaune"></div>
                            <div class="color-preset" style="background-color: #ff00ff;" data-color="#ff00ff" title="Magenta"></div>
                            <div class="color-preset" style="background-color: #00ffff;" data-color="#00ffff" title="Cyan"></div>
                            <div class="color-preset" style="background-color: #ffffff;" data-color="#ffffff" title="Blanc"></div>
                            <div class="color-preset" style="background-color: #ffa500;" data-color="#ffa500" title="Orange"></div>
                        </div>
                        
                        <input type="hidden" id="colorR" name="colorR" value="0">
                        <input type="hidden" id="colorG" name="colorG" value="255">
                        <input type="hidden" id="colorB" name="colorB" value="0">
                    </div>
                    
                    <!-- Bouton pour cette section -->
                    <div class="section-buttons">
                        <button type="button" class="btn btn-primary btn-sm" onclick="saveAppearance()">
                            <i class="fas fa-save"></i> Enregistrer Apparence
                        </button>
                    </div>
                </div>
            </div>

            <!-- Boutons d'action -->
            <div class="button-group">
                <button type="submit" class="btn btn-primary">
                    <i class="fas fa-save"></i> Enregistrer
                </button>
                <button type="button" class="btn btn-secondary" onclick="loadCurrentSettings()">
                    <i class="fas fa-refresh"></i> Actualiser
                </button>
                <button type="button" class="btn btn-danger" onclick="resetCountdown()">
                    <i class="fas fa-undo"></i> Réinitialiser
                </button>
            </div>
        </form>
        
        <!-- Aperçu du compte à rebours - Déplacé en bas -->
        <div class="card fade-in">
            <div class="card-header">
                <h2><i class="fas fa-eye"></i> Aperçu en temps réel</h2>
            </div>
            <div class="card-content">
                <div class="countdown-preview">
                    <div class="countdown-display" id="countdown-display">12:34:56</div>
                    <div class="countdown-title" id="countdown-title">COUNTDOWN</div>
                </div>
                
                <div class="info-grid">
                    <div class="info-card">
                        <div class="info-value" id="days-left">--</div>
                        <div class="info-label">Jours restants</div>
                    </div>
                    <div class="info-card">
                        <div class="info-value" id="hours-left">--</div>
                        <div class="info-label">Heures restantes</div>
                    </div>
                    <div class="info-card">
                        <div class="info-value" id="minutes-left">--</div>
                        <div class="info-label">Minutes restantes</div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <div class="footer">
        <p><i class="fas fa-microchip"></i> ESP32 P10 RGB Countdown &copy; 2025</p>
        <p>Firmware version 2.0 - Interface Web moderne</p>
    </div>

    <script>
        let countdownInterval;
        let targetDate;

        // Fonctions utilitaires pour les couleurs
        function hexToRgb(hex) {
            hex = hex.replace('#', '');
            const r = parseInt(hex.substring(0, 2), 16);
            const g = parseInt(hex.substring(2, 4), 16);
            const b = parseInt(hex.substring(4, 6), 16);
            return {r, g, b};
        }

        function rgbToHex(r, g, b) {
            return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
        }

        function updateColorPreview(color) {
            const rgb = hexToRgb(color);
            document.getElementById('colorR').value = rgb.r;
            document.getElementById('colorG').value = rgb.g;
            document.getElementById('colorB').value = rgb.b;
            document.getElementById('colorPreview').style.backgroundColor = color;
            document.getElementById('countdown-display').style.color = color;
            document.getElementById('colorValue').textContent = color.toUpperCase();
        }

        // Gestionnaires d'événements pour les couleurs
        document.getElementById('colorPicker').addEventListener('input', function() {
            updateColorPreview(this.value);
        });

        // Couleurs prédéfinies
        document.querySelectorAll('.color-preset').forEach(preset => {
            preset.addEventListener('click', function() {
                const color = this.dataset.color;
                document.getElementById('colorPicker').value = color;
                updateColorPreview(color);
            });
        });

        // Mise à jour de la taille du texte
        document.getElementById('textSize').addEventListener('change', function() {
            const sizes = {'1': '1.8rem', '2': '2.5rem', '3': '3.2rem'};
            document.getElementById('countdown-display').style.fontSize = sizes[this.value];
        });

        // Mise à jour du titre
        document.getElementById('title').addEventListener('input', function() {
            document.getElementById('countdown-title').textContent = this.value || 'COUNTDOWN';
        });

        // Mise à jour de la date/heure
        function updateTargetDate() {
            const date = document.getElementById('date').value;
            const time = document.getElementById('time').value;
            if (date && time) {
                targetDate = new Date(date + 'T' + time);
                updateCountdownPreview();
            }
        }

        document.getElementById('date').addEventListener('change', updateTargetDate);
        document.getElementById('time').addEventListener('change', updateTargetDate);

        // Prévisualisation du compte à rebours en temps réel
        function updateCountdownPreview() {
            if (!targetDate) return;

            const now = new Date();
            const diff = targetDate - now;

            if (diff <= 0) {
                document.getElementById('countdown-display').textContent = '00:00:00';
                document.getElementById('days-left').textContent = '0';
                document.getElementById('hours-left').textContent = '0';
                document.getElementById('minutes-left').textContent = '0';
                return;
            }

            const days = Math.floor(diff / (1000 * 60 * 60 * 24));
            const hours = Math.floor((diff % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60));
            const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
            const seconds = Math.floor((diff % (1000 * 60)) / 1000);

            // Format d'affichage principal
            if (days > 0) {
                document.getElementById('countdown-display').textContent = 
                    `${days}j ${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
            } else {
                document.getElementById('countdown-display').textContent = 
                    `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
            }

            // Informations détaillées
            document.getElementById('days-left').textContent = days;
            document.getElementById('hours-left').textContent = Math.floor(diff / (1000 * 60 * 60));
            document.getElementById('minutes-left').textContent = Math.floor(diff / (1000 * 60));
        }

        // Démarrer la mise à jour du compte à rebours
        function startCountdownPreview() {
            if (countdownInterval) clearInterval(countdownInterval);
            countdownInterval = setInterval(updateCountdownPreview, 1000);
            updateCountdownPreview();
        }

        // Charger les paramètres actuels
        function loadCurrentSettings() {
            document.body.classList.add('loading');
            
            fetch('/getSettings')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('title').value = data.title;
                    
                    const dateStr = `${data.year}-${String(data.month).padStart(2, '0')}-${String(data.day).padStart(2, '0')}`;
                    document.getElementById('date').value = dateStr;
                    
                    const timeStr = `${String(data.hour).padStart(2, '0')}:${String(data.minute).padStart(2, '0')}:${String(data.second).padStart(2, '0')}`;
                    document.getElementById('time').value = timeStr;
                    
                    document.getElementById('fontIndex').value = data.fontIndex;
                    document.getElementById('endTextFontIndex').value = data.endTextFontIndex;
                    document.getElementById('textSize').value = data.textSize;
                    
                    const color = rgbToHex(data.colorR, data.colorG, data.colorB);
                    document.getElementById('colorPicker').value = color;
                    updateColorPreview(color);
                    
                    // Mettre à jour le titre affiché
                    document.getElementById('countdown-title').textContent = data.title;
                    
                    // Mettre à jour la taille
                    const sizes = {'1': '1.8rem', '2': '2.5rem', '3': '3.2rem'};
                    document.getElementById('countdown-display').style.fontSize = sizes[data.textSize];
                    
                    updateTargetDate();
                    startCountdownPreview();
                })
                .catch(error => {
                    console.error('Erreur lors du chargement:', error);
                    // Valeurs par défaut
                    const tomorrow = new Date();
                    tomorrow.setDate(tomorrow.getDate() + 1);
                    document.getElementById('date').value = tomorrow.toISOString().split('T')[0];
                    document.getElementById('time').value = '23:59:00';
                    updateTargetDate();
                    startCountdownPreview();
                })
                .finally(() => {
                    document.body.classList.remove('loading');
                });
        }

        // Fonction pour sauvegarder seulement la date et l'heure
        function saveDateTime() {
            const formData = new FormData();
            formData.append('title', document.getElementById('title').value);
            formData.append('date', document.getElementById('date').value);
            formData.append('time', document.getElementById('time').value);
            
            // Ajouter les autres valeurs actuelles pour éviter de les perdre
            formData.append('fontIndex', document.getElementById('fontIndex').value);
            formData.append('endTextFontIndex', document.getElementById('endTextFontIndex').value);
            formData.append('textSize', document.getElementById('textSize').value);
            formData.append('colorR', document.getElementById('colorR').value);
            formData.append('colorG', document.getElementById('colorG').value);
            formData.append('colorB', document.getElementById('colorB').value);
            
            saveSectionData(formData, 'Date & Heure');
        }

        // Fonction pour sauvegarder seulement l'apparence
        function saveAppearance() {
            const formData = new FormData();
            formData.append('fontIndex', document.getElementById('fontIndex').value);
            formData.append('endTextFontIndex', document.getElementById('endTextFontIndex').value);
            formData.append('textSize', document.getElementById('textSize').value);
            formData.append('colorR', document.getElementById('colorR').value);
            formData.append('colorG', document.getElementById('colorG').value);
            formData.append('colorB', document.getElementById('colorB').value);
            
            // Ajouter les autres valeurs actuelles pour éviter de les perdre
            formData.append('title', document.getElementById('title').value);
            formData.append('date', document.getElementById('date').value);
            formData.append('time', document.getElementById('time').value);
            
            saveSectionData(formData, 'Apparence');
        }

        // Fonction utilitaire pour sauvegarder une section
        function saveSectionData(formData, sectionName) {
            fetch('/settings', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.ok) {
                    showNotification(`${sectionName} sauvegardée avec succès !`, 'success');
                    loadCurrentSettings(); // Recharger pour synchroniser l'aperçu
                } else {
                    showNotification(`Erreur lors de la sauvegarde de ${sectionName}`, 'error');
                }
            })
            .catch(error => {
                console.error('Erreur:', error);
                showNotification(`Erreur lors de la sauvegarde de ${sectionName}`, 'error');
            });
        }

        // Fonction pour afficher les notifications
        function showNotification(message, type) {
            const notification = document.createElement('div');
            notification.className = `notification ${type}`;
            notification.innerHTML = `
                <i class="fas fa-${type === 'success' ? 'check' : 'exclamation-triangle'}"></i>
                ${message}
            `;
            
            // Ajouter le style si pas encore défini
            if (!document.querySelector('style[data-notifications]')) {
                const style = document.createElement('style');
                style.setAttribute('data-notifications', 'true');
                style.textContent = `
                    .notification {
                        position: fixed;
                        top: 20px;
                        right: 20px;
                        padding: 1rem;
                        border-radius: 8px;
                        color: white;
                        font-weight: 500;
                        z-index: 1000;
                        display: flex;
                        align-items: center;
                        gap: 0.5rem;
                        animation: slideIn 0.3s ease;
                    }
                    .notification.success { background: var(--secondary-color); }
                    .notification.error { background: var(--danger-color); }
                    @keyframes slideIn {
                        from { transform: translateX(100%); opacity: 0; }
                        to { transform: translateX(0); opacity: 1; }
                    }
                `;
                document.head.appendChild(style);
            }
            
            document.body.appendChild(notification);
            
            setTimeout(() => {
                notification.style.animation = 'slideIn 0.3s ease reverse';
                setTimeout(() => notification.remove(), 300);
            }, 3000);
        }

        // Fonction de réinitialisation
        function resetCountdown() {
            if (confirm('Voulez-vous vraiment réinitialiser le compte à rebours aux valeurs par défaut ?')) {
                document.getElementById('settingsForm').action = '/reset';
                document.getElementById('settingsForm').submit();
            }
        }

        // Soumission du formulaire avec feedback
        document.getElementById('settingsForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const submitBtn = this.querySelector('button[type="submit"]');
            const originalText = submitBtn.innerHTML;
            submitBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Enregistrement...';
            submitBtn.disabled = true;
            
            const formData = new FormData(this);
            
            fetch('/settings', {
                method: 'POST',
                body: formData
            })
            .then(response => {
                if (response.ok) {
                    submitBtn.innerHTML = '<i class="fas fa-check"></i> Enregistré !';
                    setTimeout(() => {
                        submitBtn.innerHTML = originalText;
                        submitBtn.disabled = false;
                    }, 2000);
                } else {
                    throw new Error('Erreur de sauvegarde');
                }
            })
            .catch(error => {
                console.error('Erreur:', error);
                submitBtn.innerHTML = '<i class="fas fa-times"></i> Erreur';
                setTimeout(() => {
                    submitBtn.innerHTML = originalText;
                    submitBtn.disabled = false;
                }, 2000);
            });
        });

        // Initialisation au chargement
        window.addEventListener('load', function() {
            loadCurrentSettings();
            
            // Animation d'entrée des cartes
            const cards = document.querySelectorAll('.card');
            cards.forEach((card, index) => {
                setTimeout(() => {
                    card.style.opacity = '0';
                    card.style.transform = 'translateY(20px)';
                    card.style.transition = 'all 0.5s ease-in-out';
                    setTimeout(() => {
                        card.style.opacity = '1';
                        card.style.transform = 'translateY(0)';
                    }, 100);
                }, index * 200);
            });
        });
    </script>
</body>
</html>
)rawliteral";

// Note: La fonction display_update_enable() a été intégrée dans setup()
// pour une meilleure gestion avec FreeRTOS

// Calcul du temps restant
void updateCountdown(int &days, int &hours, int &minutes, int &seconds) {
  // Ne pas calculer le countdown si les paramètres ne sont pas encore chargés
  if (!settingsLoaded) {
    days = hours = minutes = seconds = 0;
    countdownExpired = false;
    return;
  }
  
  DateTime now = rtc.now();
  
  // Vérifier si le countdown est expiré
  if (now >= countdownTarget) {
    countdownExpired = true;
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
  display.clearDisplay();

  // Choisir la couleur
  uint16_t displayColor = countdownColor;

  // Si on est dans les 10 dernières secondes et qu'il faut clignoter
  if (blinkLastSeconds) {
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
      lastBlinkTime = currentTime;
      blinkState = !blinkState;
    }
    if (!blinkState) {
      displayColor = myBLACK;
    } else {
      displayColor = myRED;
    }
  }

  // Si le countdown est expiré
  if (countdownExpired) {
    display.setFont();
    int maxSize = 1;
    // Afficher le titre défini par l'utilisateur
    const char* endMsg = countdownTitle;
    
    // Appliquer la police sélectionnée pour le texte de fin
    const GFXfont* selectedFont = getFontByIndex(endTextFontIndex);
    display.setFont(selectedFont);
    
    // Recherche intelligente de la taille optimale pour le texte de fin
    for (int s = 1; s <= 8; s++) { // Augmenter la plage de recherche
      display.setTextSize(s);
      int16_t x1, y1;
      uint16_t w, h;
      display.getTextBounds(endMsg, 0, 0, &x1, &y1, &w, &h);
      
      // Marges de sécurité plus petites
      int marginX = 2;
      int marginY = 1;
      
      if (w <= (TOTAL_WIDTH - marginX) && h <= (TOTAL_HEIGHT - marginY)) {
        maxSize = s;
      } else {
        break;
      }
    }
    
    display.setTextSize(maxSize);
    
    // Calcul précis des dimensions finales
    int16_t x1, y1;
    uint16_t finalW, finalH;
    display.getTextBounds(endMsg, 0, 0, &x1, &y1, &finalW, &finalH);
    
    // Centrage parfait
    int textX = (TOTAL_WIDTH - finalW) / 2;
    int textY = (TOTAL_HEIGHT - finalH) / 2 + finalH;
    
    display.setCursor(textX, textY);
    display.setTextColor(myRED);
    display.print(endMsg);
    return;
  }

  char countdownText[20];
  int16_t textWidth, textX, textY;
  int maxSize = 1;

  // Déterminer le texte à afficher
  switch (displayFormat) {
    case 0:
      sprintf(countdownText, "%dd %02d:%02d", days, hours, minutes);
      break;
    case 1:
      sprintf(countdownText, "%02d:%02d:%02d", hours, minutes, seconds);
      break;
    case 2:
      sprintf(countdownText, "%02d:%02d", minutes, seconds);
      break;
    case 3:
      sprintf(countdownText, "%02d", seconds);
      break;
  }

  // Recherche de la taille maximale qui rentre - Algorithme amélioré
  // Appliquer la police sélectionnée par l'utilisateur
  const GFXfont* selectedFont = getFontByIndex(fontIndex);
  display.setFont(selectedFont);
  
  // Recherche intelligente de la taille optimale
  maxSize = 1;
  for (int s = 1; s <= 8; s++) { // Augmenter la plage de recherche
    display.setTextSize(s);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(countdownText, 0, 0, &x1, &y1, &w, &h);
    
    // Marges de sécurité plus petites pour maximiser la taille
    int marginX = 2; // Marge horizontale minimale
    int marginY = 1; // Marge verticale minimale
    
    if (w <= (TOTAL_WIDTH - marginX) && h <= (TOTAL_HEIGHT - marginY)) {
      maxSize = s;
    } else {
      break; // Arrêter dès qu'on dépasse
    }
  }
  
  display.setTextSize(maxSize);
  
  // Calcul précis des dimensions finales
  int16_t x1, y1;
  uint16_t finalW, finalH;
  display.getTextBounds(countdownText, 0, 0, &x1, &y1, &finalW, &finalH);
  
  // Centrage parfait
  textX = (TOTAL_WIDTH - finalW) / 2;
  textY = (TOTAL_HEIGHT - finalH) / 2 + finalH; // Position de base pour GFX fonts
  
  display.setTextColor(displayColor);
  display.setCursor(textX, textY);
  display.print(countdownText);

  // Le titre n'est plus affiché pendant le compte à rebours
}

// Chargement des paramètres depuis la mémoire flash (version thread-safe)
void loadSettings() {
  // Utiliser un timeout pour éviter les blocages
  if (preferencesMutex == NULL || xSemaphoreTake(preferencesMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire preferences mutex - using defaults");
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
    endTextFontIndex = preferences.getInt("endTextFontIndex", 0);
    
    String savedTitle = preferences.getString("cd_Title", "COUNTDOWN");
    strcpy(countdownTitle, savedTitle.c_str());
    
    preferences.end();
    success = true;
    
  } while(false);
  
  // Libérer le mutex
  xSemaphoreGive(preferencesMutex);
  
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
  
  // Marquer que les paramètres sont chargés
  settingsLoaded = true;
  
  Serial.println("Settings loaded successfully");
}

// Sauvegarde des paramètres dans la mémoire flash (version thread-safe)
void saveSettings() {
  // Utiliser un timeout pour éviter les blocages
  if (preferencesMutex == NULL || xSemaphoreTake(preferencesMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    Serial.println("Failed to acquire preferences mutex - settings not saved");
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
    preferences.putInt("endTextFontIndex", endTextFontIndex);
    
    preferences.putString("cd_Title", String(countdownTitle));
    
    preferences.end();
    success = true;
    
  } while(false);
  
  // Libérer le mutex
  xSemaphoreGive(preferencesMutex);
  
  if (success) {
    Serial.println("Settings saved successfully");
    // Mise à jour de la couleur
    countdownColor = display.color565(countdownColorR, countdownColorG, countdownColorB);
    
    // Mise à jour de la date cible
    countdownTarget = DateTime(countdownYear, countdownMonth, countdownDay, 
                             countdownHour, countdownMinute, countdownSecond);
                             
    // S'assurer que les paramètres sont marqués comme chargés
    settingsLoaded = true;
  } else {
    Serial.println("Failed to save settings");
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

// Configuration du point d'accès
void set_ESP32_Access_Point() {
  Serial.println("\n-------------");
  Serial.println("WIFI mode : AP");
  WiFi.mode(WIFI_AP);
  Serial.println("-------------");
  delay(1000);

  Serial.println("\n-------------");
  Serial.println("Setting up ESP32 to be an Access Point.");
  WiFi.softAP(ap_ssid, ap_password);
  delay(1000);
  
  IPAddress local_ip(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(local_ip, gateway, subnet);
  Serial.println("-------------");
  Serial.print("SSID name : ");
  Serial.println(ap_ssid);
  Serial.print("IP address : ");
  Serial.println(WiFi.softAPIP());
  delay(1000);
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
  json += "\"endTextFontIndex\":" + String(endTextFontIndex);
  json += "}";
  
  server.send(200, "application/json", json);
}

// Gestionnaire des paramètres
void handleSettings() {
  Serial.println("\n-------------Settings");

  // Récupérer les valeurs de la requête
  String title = server.arg("title");
  String dateStr = server.arg("date");
  String timeStr = server.arg("time");
  
  // Parser la date (format YYYY-MM-DD)
  countdownYear = dateStr.substring(0, 4).toInt();
  countdownMonth = dateStr.substring(5, 7).toInt();
  countdownDay = dateStr.substring(8, 10).toInt();
  
  // Parser l'heure (format HH:MM:SS)
  countdownHour = timeStr.substring(0, 2).toInt();
  countdownMinute = timeStr.substring(3, 5).toInt();
  countdownSecond = timeStr.length() > 5 ? timeStr.substring(6, 8).toInt() : 0;
  
  // Récupérer les autres paramètres
  countdownTextSize = server.arg("textSize").toInt();
  countdownColorR = server.arg("colorR").toInt();
  countdownColorG = server.arg("colorG").toInt();
  countdownColorB = server.arg("colorB").toInt();
  fontIndex = server.arg("fontIndex").toInt();
  endTextFontIndex = server.arg("endTextFontIndex").toInt();
  
  // Limiter les valeurs
  if (countdownTextSize < 1) countdownTextSize = 1;
  if (countdownTextSize > 3) countdownTextSize = 3;
  
  if (fontIndex < 0) fontIndex = 0;
  if (fontIndex > 3) fontIndex = 3;
  
  if (endTextFontIndex < 0) endTextFontIndex = 0;
  if (endTextFontIndex > 3) endTextFontIndex = 3;
  
  // Mettre à jour le titre
  if (title.length() > 50) title = title.substring(0, 50);
  strcpy(countdownTitle, title.c_str());
  
  // Valider la date
  if (countdownMonth < 1) countdownMonth = 1;
  if (countdownMonth > 12) countdownMonth = 12;
  if (countdownDay < 1) countdownDay = 1;
  if (countdownDay > 31) countdownDay = 31;
  
  // Valider l'heure
  if (countdownHour > 23) countdownHour = 23;
  if (countdownMinute > 59) countdownMinute = 59;
  if (countdownSecond > 59) countdownSecond = 59;
  
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
  // Démarrage du serveur DNS pour le portail captif
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  server.on("/", handleRoot);
  server.on("/settings", HTTP_POST, handleSettings);
  server.on("/getSettings", handleGetSettings);
  server.on("/reset", HTTP_POST, handleReset);
  // Rediriger toutes les autres requêtes HTTP vers la page principale (portail captif)
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
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
    Serial.print("Then open http://");
    Serial.print(WiFi.softAPIP());
    Serial.println(" in your browser");
  }
  
  Serial.println("Use key: " KEY_TXT);
  delay(500);
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
  delay(1000);
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
  delay(100);
  
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
  
  // Message de démarrage
  display.clearDisplay();
  display.setTextColor(countdownColor);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("COUNTDOWN");
  
  display.setCursor(0, 8);
  display.print("INTERFACE WEB");
  
  delay(2000);
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
  
  Serial.println("Creating FreeRTOS tasks...");
  
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
  
  // Tâches réseau sur le Core 0 (avec le WiFi)
  result = xTaskCreatePinnedToCore(
    NetworkTask,
    "NetworkTask", 
    TASK_NETWORK_STACK,
    NULL,
    TASK_NETWORK_PRIORITY,
    &networkTaskHandle,
    0  // Core 0
  );
  if (result != pdPASS) Serial.println("Failed to create Network task");
  
  result = xTaskCreatePinnedToCore(
    WebServerTask,
    "WebServerTask",
    TASK_WEBSERVER_STACK,
    NULL,
    TASK_WEBSERVER_PRIORITY,
    &webServerTaskHandle,
    0  // Core 0
  );
  if (result != pdPASS) Serial.println("Failed to create WebServer task");
  
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
  Serial.println("Activating display timer...");
  display_update_enable(true);
  delay(200); // Attendre que le timer soit stable
}

// Tâche d'affichage
void DisplayTask(void * parameter) {
  // Attendre que le système soit prêt
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  Serial.println("Display task started on core " + String(xPortGetCoreID()));
  
  // Attendre que les paramètres soient chargés
  while (!settingsLoaded) {
    Serial.println("Waiting for settings to load...");
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  
  Serial.println("Settings loaded, starting display updates");
  
  for(;;) {
    // Tentative d'acquisition du mutex avec un timeout court
    if(displayMutex != NULL && xSemaphoreTake(displayMutex, pdMS_TO_TICKS(10))) {
      int days, hours, minutes, seconds;
      
      // Protection contre les erreurs de calcul
      if(countdownMutex != NULL && xSemaphoreTake(countdownMutex, pdMS_TO_TICKS(10))) {
        updateCountdown(days, hours, minutes, seconds);
        updateDisplayFormat(days, hours, minutes, seconds);
        xSemaphoreGive(countdownMutex);
        
        displayFullscreenCountdown(days, hours, minutes, seconds);
      } else {
        // Si on ne peut pas obtenir le mutex, afficher un état par défaut
        displayFullscreenCountdown(0, 0, 0, 0);
      }
      
      xSemaphoreGive(displayMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Rafraîchissement toutes les 50ms
  }
}

// Tâche du serveur web
void WebServerTask(void * parameter) {
  // Attendre que le WiFi soit configuré
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  Serial.println("WebServer task started on core " + String(xPortGetCoreID()));
  
  for(;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(1)); // Délai minimal
  }
}

// Tâche de gestion du compte à rebours
void CountdownTask(void * parameter) {
  // Attendre que le RTC soit prêt
  vTaskDelay(pdMS_TO_TICKS(1500));
  
  Serial.println("Countdown task started on core " + String(xPortGetCoreID()));
  
  for(;;) {
    // Vérifier s'il faut sauvegarder les paramètres (avec délai de sécurité)
    if (saveRequested && (millis() - saveRequestTime) > 100) {  // Attendre 100ms après la requête
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

// Tâche de gestion réseau
void NetworkTask(void * parameter) {
  // Attendre que le système soit stabilisé
  vTaskDelay(pdMS_TO_TICKS(500));
  
  Serial.println("Network task started on core " + String(xPortGetCoreID()));
  
  for(;;) {
    dnsServer.processNextRequest();
    
    // Vérification et gestion de la connexion WiFi
    if(useStationMode) {
      if(WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost, trying to reconnect...");
        WiFi.reconnect();
        vTaskDelay(pdMS_TO_TICKS(5000)); // Attendre 5 secondes avant la prochaine tentative
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void loop() {
  // Pause pour éviter les problèmes de watchdog
  // Les tâches FreeRTOS gèrent tout le travail
  vTaskDelay(pdMS_TO_TICKS(1000));
}
  