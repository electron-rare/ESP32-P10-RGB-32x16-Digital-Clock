# Configuration du Portail Captif pour ESP32 P10 RGB Digital Clock

## Modifications apportées

Ce document décrit les modifications apportées pour transformer l'interface web de l'horloge ESP32 en portail captif.

### 1. Modifications du fichier `platformio.ini`

Ajout de la dépendance `DNSServer` dans l'environnement principal :

```ini
[env:main]
src_filter = +<main.cpp>
lib_deps = 
    adafruit/Adafruit BusIO@^1.16.1
    adafruit/Adafruit GFX Library@^1.11.9
    adafruit/RTClib@^2.1.4
    https://github.com/2dom/PxMatrix.git
    DNSServer  # <-- NOUVELLE DÉPENDANCE
```

### 2. Modifications du fichier `src/main.cpp`

#### 2.1 Inclusion de la bibliothèque DNSServer

```cpp
#include <DNSServer.h>
```

#### 2.2 Déclaration des objets pour le portail captif

```cpp
// Serveur web et DNS pour portail captif
WebServer server(80);
DNSServer dnsServer;

// Configuration du portail captif
const byte DNS_PORT = 53;
const IPAddress apIP(192, 168, 1, 1);
```

#### 2.3 Modification de la fonction `set_ESP32_Access_Point()`

- Ajout du démarrage du serveur DNS
- Configuration pour rediriger toutes les requêtes DNS vers l'ESP32

```cpp
// Démarrage du serveur DNS pour le portail captif
dnsServer.start(DNS_PORT, "*", apIP);
```

#### 2.4 Nouvelles fonctions de gestion des requêtes

- `handleCaptivePortal()` : Gère les redirections du portail captif
- `handleNotFound()` : Traite toutes les requêtes non définies

#### 2.5 Configuration des routes spéciales

Ajout de routes communes utilisées par les systèmes d'exploitation pour détecter les portails captifs :

```cpp
server.on("/generate_204", handleRoot);      // Android
server.on("/fwlink", handleRoot);            // Microsoft  
server.on("/hotspot-detect.html", handleRoot); // Apple
server.onNotFound(handleNotFound);           // Toutes autres requêtes
```

#### 2.6 Gestion du DNS dans la tâche WebServer

Ajout du traitement des requêtes DNS dans la boucle principale :

```cpp
// Gestion du serveur DNS pour le portail captif (uniquement en mode AP)
if (!useStationMode) {
    dnsServer.processNextRequest();
}
```

## Fonctionnement du portail captif

### 1. Comment ça marche ?

1. **Point d'accès WiFi** : L'ESP32 crée un réseau WiFi nommé "HOKA_Clock"
2. **Serveur DNS** : Toutes les requêtes DNS sont redirigées vers l'IP de l'ESP32 (192.168.1.1)
3. **Interception web** : Toutes les tentatives de navigation sont capturées
4. **Redirection** : L'utilisateur est automatiquement redirigé vers l'interface de l'horloge

### 2. Expérience utilisateur

1. L'utilisateur se connecte au WiFi "HOKA_Clock" (mot de passe : "hoka")
2. Quand il essaie de naviguer sur n'importe quel site web, il est automatiquement redirigé vers l'interface de l'horloge
3. Il peut alors configurer l'horloge directement via l'interface web

### 3. Compatibilité

Le portail captif est compatible avec :
- **Android** : Détection automatique et affichage de la notification de connexion
- **iOS/macOS** : Ouverture automatique du navigateur captif
- **Windows** : Redirection automatique lors de la première navigation

## Configuration recommandée

### Variables importantes :
- **SSID** : `HOKA_Clock` (peut être modifié dans `ap_ssid`)
- **Mot de passe** : `hoka` (peut être modifié dans `ap_password`)
- **IP** : `192.168.1.1` (définie dans `apIP`)
- **Clé d'accès** : `hoka` (définie dans `KEY_TXT`)

### Mode de fonctionnement :
- **Mode Station** (`useStationMode = true`) : Se connecte à un WiFi existant
- **Mode Point d'Accès** (`useStationMode = false`) : Crée un portail captif

## Avantages du portail captif

1. **Facilité d'accès** : Pas besoin de connaître l'adresse IP
2. **Expérience intuitive** : Ouverture automatique de l'interface
3. **Compatibilité universelle** : Fonctionne sur tous les appareils
4. **Configuration simple** : L'utilisateur est guidé automatiquement

## Compilation et déploiement

Pour compiler et déployer le projet avec le portail captif :

```bash
# Compilation
pio run -e main

# Upload vers l'ESP32
pio run -e main --target upload

# Monitoring série (optionnel)
pio device monitor
```

## Dépannage

### Problèmes courants :

1. **Le portail ne s'ouvre pas automatiquement**
   - Vérifiez que le mode AP est activé (`useStationMode = false`)
   - Testez l'accès direct via http://192.168.1.1

2. **Erreurs de compilation**
   - Vérifiez que la dépendance `DNSServer` est bien ajoutée
   - Assurez-vous d'utiliser l'environnement `main`

3. **Redirection en boucle**
   - Vérifiez la fonction `handleCaptivePortal()`
   - Testez avec différents navigateurs

## Notes de développement

- Le portail captif n'est actif qu'en mode Point d'Accès
- En mode Station, le comportement reste inchangé
- La gestion DNS ajoute une charge minime au processeur
- Compatible avec l'architecture FreeRTOS existante
