/**
 * Test du Portail Captif ESP32
 * Ce fichier permet de tester uniquement la fonctionnalité de portail captif
 * sans l'affichage LED pour simplifier le débogage
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Configuration du point d'accès
const char* ap_ssid = "HOKA_Clock_Test";
const char* ap_password = "hoka";

// Configuration du portail captif
const byte DNS_PORT = 53;
const IPAddress apIP(192, 168, 1, 1);

// Objets serveur
WebServer server(80);
DNSServer dnsServer;

// Prototypes des fonctions
void handleRoot();
void handleCaptivePortal();
void handleNotFound();
void setupAccessPoint();
void setupWebServer();

// Page HTML simple pour les tests
const char* test_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Test Portail Captif - ESP32 Clock</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 50px; }
        .container { max-width: 600px; margin: 0 auto; }
        h1 { color: #2196F3; }
        .success { background: #4CAF50; color: white; padding: 20px; border-radius: 5px; }
        .info { background: #2196F3; color: white; padding: 15px; border-radius: 5px; margin: 20px 0; }
        button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; }
        button:hover { background: #45a049; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Portail Captif Actif !</h1>
        <div class="success">
            <h2>Felicitations !</h2>
            <p>Vous etes connecte au portail captif de l'ESP32 Digital Clock.</p>
        </div>
        <div class="info">
            <h3>Informations de connexion :</h3>
            <p><strong>SSID :</strong> HOKA_Clock_Test</p>
            <p><strong>IP ESP32 :</strong> 192.168.1.1</p>
            <p><strong>Status :</strong> Portail Captif Fonctionnel</p>
        </div>
        <p>Ce portail capture automatiquement toutes vos tentatives de navigation et vous redirige vers cette interface.</p>
        <br>
        <button onclick="location.reload()">Actualiser</button>
        <button onclick="testRedirection()">Tester la redirection</button>
    </div>
    
    <script>
        function testRedirection() {
            alert('En temps normal, cette fonction redirigerait vers l interface de configuration de l horloge.');
        }
        
        // Afficher l'URL actuelle pour debug
        document.addEventListener('DOMContentLoaded', function() {
            console.log('URL actuelle:', window.location.href);
            console.log('Host:', window.location.host);
        });
    </script>
</body>
</html>
)rawliteral";

// Gestionnaire pour le portail captif
void handleCaptivePortal() {
    String hostHeader = server.hostHeader();
    
    // Si l'utilisateur accède directement à l'IP de l'ESP32, afficher la page
    if (hostHeader == WiFi.softAPIP().toString() || hostHeader == "192.168.1.1") {
        server.send(200, "text/html", test_page);
        return;
    }
    
    // Pour toutes les autres requêtes, rediriger vers l'ESP32
    String redirectUrl = "http://" + WiFi.softAPIP().toString();
    server.sendHeader("Location", redirectUrl, true);
    server.send(302, "text/plain", "Redirection vers le portail captif");
}

// Gestionnaire pour la page principale
void handleRoot() {
    server.send(200, "text/html", test_page);
}

// Gestionnaire pour toutes les requêtes non définies
void handleNotFound() {
    Serial.println("Requête non trouvée - Redirection portail captif");
    Serial.println("URI: " + server.uri());
    Serial.println("Host: " + server.hostHeader());
    handleCaptivePortal();
}

// Configuration du point d'accès avec portail captif
void setupAccessPoint() {
    Serial.println("Configuration du Point d'Accès avec Portail Captif...");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    
    // Configuration IP
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);
    
    // Démarrage du serveur DNS
    dnsServer.start(DNS_PORT, "*", apIP);
    
    Serial.println("Point d'accès configuré :");
    Serial.print("SSID: ");
    Serial.println(ap_ssid);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Serveur DNS démarré pour portail captif");
}

// Configuration du serveur web
void setupWebServer() {
    // Routes principales
    server.on("/", handleRoot);
    
    // Routes spéciales pour portail captif
    server.on("/generate_204", handleRoot);        // Android
    server.on("/fwlink", handleRoot);              // Microsoft
    server.on("/hotspot-detect.html", handleRoot); // Apple
    server.on("/connecttest.txt", handleRoot);     // Microsoft
    server.on("/redirect", handleRoot);            // Générique
    
    // Gestionnaire pour toutes les autres requêtes
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("Serveur web démarré");
    Serial.println("Connectez-vous au WiFi et naviguez vers n'importe quel site pour tester le portail captif");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Test Portail Captif ESP32 ===");
    Serial.println("Ce test vérifie uniquement la fonctionnalité de portail captif");
    
    setupAccessPoint();
    setupWebServer();
    
    Serial.println("\n=== Instructions de test ===");
    Serial.println("1. Connectez-vous au WiFi: " + String(ap_ssid));
    Serial.println("2. Mot de passe: " + String(ap_password));
    Serial.println("3. Essayez de naviguer vers n'importe quel site web");
    Serial.println("4. Vous devriez être redirigé automatiquement vers la page de test");
    Serial.println("5. Ou accédez directement à http://192.168.1.1");
    Serial.println("\n=== Monitoring ===");
}

void loop() {
    // Gestion des requêtes web
    server.handleClient();
    
    // Gestion des requêtes DNS pour le portail captif
    dnsServer.processNextRequest();
    
    // Petit délai pour ne pas surcharger le processeur
    delay(1);
    
    // Debug : afficher le nombre de clients connectés toutes les 10 secondes
    static unsigned long lastClientCount = 0;
    if (millis() - lastClientCount > 10000) {
        Serial.print("Clients connectés: ");
        Serial.println(WiFi.softAPgetStationNum());
        lastClientCount = millis();
    }
}
