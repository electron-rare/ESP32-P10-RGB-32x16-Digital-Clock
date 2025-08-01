# ESP32 P10 RGB 32x16 Digital Clock - PlatformIO Version

## Description

Horloge numérique utilisant un panneau P10 RGB 32x16 avec ESP32, module RTC DS3231 et interface web de configuration.

Version adaptée pour PlatformIO à partir du code Arduino IDE original.

## ✨ Fonctionnalités

### 🕒 Horloge Principale
- **Affichage de l'heure** avec deux points clignotants
- **Module RTC DS3231** pour une précision élevée
- **Centrage automatique** adapté au nombre de panneaux

### 📝 Texte Défilant
- **Messages personnalisables** (150 caractères max)
- **Vitesse ajustable** (10-100)
- **Affichage de la date** automatique
- **Rotation automatique** entre date, texte et countdown

### ⏱️ Countdown (Nouveau!)
- **Compte à rebours** vers une date/heure cible
- **Format adaptatif** : jours, heures, minutes, secondes
- **Titre personnalisable** pour l'événement
- **Couleur dédiée** avec indication d'expiration
- **Activation/désactivation** via interface web

### 🎨 Personnalisation
- **2 modes d'affichage** : couleurs fixes ou changeantes
- **Couleurs RGB** personnalisables pour chaque élément
- **Luminosité ajustable** (0-255)
- **Interface web complète** pour la configuration

### 🔗 Cascade de Panneaux
- **Support multi-panneaux** (1x1 à 8x1, 2x2)
- **Affichage élargi** pour textes longs
- **Configuration automatique** de la luminosité
- **Tests dédiés** pour chaque configuration

### 🌐 Connectivité
- **WiFi ou Point d'Accès** automatique
- **Interface web responsive** pour configuration
- **Sauvegarde persistante** en mémoire flash
- **Sécurisé** avec clé d'accès

## Matériel Requis

- **ESP32 DEVKIT V1**
- **Panneau P10 RGB 32x16 HUB75** (Scan 1/8)
  - Référence utilisée : P10-2727-8S-32X16-A
  - ICs : RUC7258D, DP5125D, 74HC245KA
- **Module RTC DS3231**
- **Alimentation 5V** (importante pour le panneau P10)

## Brochage

### Connexions P10 HUB75
```
HUB75 Pin | ESP32 Pin
----------|----------
R1        | GPIO 2
G1        | GPIO 15
B1        | GPIO 4
GND       | GND
R2        | GPIO 16
G2        | GPIO 17
B2        | GPIO 5
GND       | GND
A         | GPIO 19
B         | GPIO 23
C         | GPIO 18
GND       | GND
CLK       | GPIO 14
STB       | GPIO 32
OE        | GPIO 33
GND       | GND
```

**Note :** Ces connexions peuvent varier selon votre panneau P10. Consultez le code et ajustez si nécessaire.

### Connexions RTC DS3231
```
DS3231 | ESP32
-------|-------
VCC    | 3.3V
GND    | GND
SDA    | GPIO 21
SCL    | GPIO 22
```

## Installation et Configuration

### 1. Installation PlatformIO

Si vous n'avez pas encore PlatformIO :
```bash
# Via VS Code
# Installez l'extension PlatformIO IDE

# Via ligne de commande
pip install platformio
```

### 2. Configuration du projet

1. **Clonez ou téléchargez** ce projet
2. **Ouvrez le dossier** dans VS Code avec PlatformIO
3. **Modifiez les paramètres WiFi** dans `src/main.cpp` :

```cpp
// Configuration WiFi - Modifiez selon vos besoins
const char* ssid = "VOTRE_WIFI_SSID";
const char* password = "VOTRE_MOT_DE_PASSE_WIFI";
```

### 3. Compilation et téléversement

```bash
# Compilation
pio run

# Téléversement
pio run --target upload

# Monitoring série
pio device monitor
```

Ou utilisez les boutons PlatformIO dans VS Code.

## Structure du Projet

```
├── platformio.ini          # Configuration PlatformIO
├── src/
│   └── main.cpp            # Code principal de l'horloge
├── include/
│   └── PageIndex.h         # Interface web HTML
├── examples/
│   ├── rtc_test.cpp        # Test du module RTC
│   ├── p10_test.cpp        # Test du panneau P10
│   ├── countdown_test.cpp  # Test du countdown
│   ├── cascade_test.cpp    # Test des panneaux multiples
│   └── nvs_erase.cpp       # Effacement mémoire NVS
└── README.md               # Ce fichier
```

## Utilisation

### Premier démarrage

1. **Téléversez le code** sur l'ESP32
2. **Ouvrez le moniteur série** (115200 baud)
3. L'ESP32 va essayer de se connecter à votre WiFi
4. Si la connexion échoue, il créera un point d'accès WiFi

### Configuration via interface web

1. **Connectez-vous au réseau WiFi** :
   - Si connecté à votre WiFi : utilisez l'IP affichée dans le moniteur série
   - Sinon connectez-vous au point d'accès "ESP32_Clock" (mot de passe : "esp32clock")

2. **Ouvrez votre navigateur** et allez à l'adresse IP

3. **Utilisez la clé** : `p10rgbesp32ws`

4. **Configurez** :
   - Date et heure
   - Mode d'affichage (1 = couleurs manuelles, 2 = couleurs automatiques)
   - Luminosité (0-255)
   - Couleurs (en mode 1)
   - Texte défilant
   - Vitesse de défilement
   - **Countdown** : événement, date cible, couleur

### ⏱️ Configuration du Countdown

Le countdown permet d'afficher un compte à rebours vers un événement :

1. **Activez le countdown** : Cochez la case "Active"
2. **Nommez l'événement** : Exemple "NOUVEL AN", "ANNIVERSAIRE"
3. **Définissez la date cible** : Jour, Mois, Année
4. **Définissez l'heure cible** : Heure, Minute, Seconde (24h)
5. **Choisissez la couleur** : RGB personnalisable
6. **Sauvegardez** avec "Set Countdown"

**Formats d'affichage :**
- Avec jours : `NOUVEL AN: 15d 08h 30m 45s`
- Sans jours : `NOUVEL AN: 08h 30m 45s`
- Dernière heure : `NOUVEL AN: 30m 45s`
- Expiré : `NOUVEL AN - EXPIRED!` (en rouge)

Voir le [Guide Countdown Complet](COUNTDOWN_GUIDE.md) pour plus de détails.

### Configuration de l'heure RTC

Vous pouvez aussi configurer l'heure directement via le moniteur série :
```
SET,2024,8,1,14,30,0
```
Format : `SET,année,mois,jour,heure,minute,seconde`

## Programmes d'exemple et Tests

### 🧪 Tests avec PlatformIO

Le projet inclut plusieurs environnements de test préconfigurés :

```bash
# Test du module RTC DS3231
pio run -e rtc_test --target upload

# Test du panneau P10 (couleurs, texte)
pio run -e p10_test --target upload

# Test du countdown (5 min automatique)
pio run -e countdown_test --target upload

# Test cascade (panneaux multiples)
pio run -e test_cascade --target upload

# Effacement mémoire NVS
pio run -e nvs_erase --target upload
```

### 📱 Tâches VS Code

Si vous utilisez VS Code avec PlatformIO, les tâches suivantes sont disponibles :
- **PlatformIO: Test RTC Module**
- **PlatformIO: Test P10 Matrix**  
- **PlatformIO: Test Countdown**
- **PlatformIO: Test Cascade (2 panels)**
- **PlatformIO: Erase NVS Memory**

### Test du module RTC
```bash
# Affiche la date/heure actuelle
# Utile pour vérifier la connexion RTC
```

### Test du panneau P10
```bash
# Test complet des couleurs et de l'affichage
# Utile pour vérifier les connexions HUB75
```

### Test du countdown
```bash
# Configure automatiquement un countdown de 5 minutes
# Affiche les différents formats d'affichage
# Test d'expiration automatique
```

### Effacement mémoire NVS
```bash
# Réinitialise toutes les préférences sauvegardées
# Utile pour un "reset factory" complet
```

## Modes d'affichage

### Mode 1 - Couleurs manuelles
- Couleurs fixes définissables via l'interface web
- Couleur de l'horloge personnalisable
- Couleur de la date personnalisable  
- Couleur du texte personnalisable

### Mode 2 - Couleurs automatiques
- Changement automatique des couleurs
- Cycle entre : Rouge, Vert, Bleu, Jaune, Cyan, Magenta, Blanc

## Paramètres configurables

- **Date et heure** : via interface web ou moniteur série
- **Luminosité** : 0-255
- **Mode d'affichage** : 1 ou 2
- **Couleurs RGB** : 0-255 pour chaque composante (mode 1)
- **Texte défilant** : jusqu'à 150 caractères
- **Vitesse de défilement** : 10-100 (plus bas = plus rapide)

## Dépannage

### Problèmes d'affichage
- Vérifiez l'alimentation 5V (importante !)
- Vérifiez les connexions HUB75
- Ajustez `PxMATRIX_SPI_FREQUENCY` si vous voyez du bruit

### Problèmes WiFi
- Vérifiez le SSID et le mot de passe
- L'ESP32 basculera en mode point d'accès si la connexion échoue

### Problèmes RTC
- Vérifiez les connexions I2C (SDA/SCL)
- Testez avec le programme `examples/rtc_test.cpp`

### Réinitialisation
- Utilisez `examples/nvs_erase.cpp` pour effacer toutes les préférences

## Configuration avancée

### Modification de la fréquence SPI
```cpp
// Dans src/main.cpp, modifiez si vous avez du bruit sur l'affichage
#define PxMATRIX_SPI_FREQUENCY 10000000  // Valeurs possibles: 20000000, 15000000, 10000000, 8000000
```

### Modification des pins
```cpp
// Modifiez ces valeurs dans src/main.cpp si votre câblage est différent
#define P_LAT 5
#define P_A   19
#define P_B   23
#define P_C   18
#define P_OE  4
```

## Bibliothèques utilisées

- **PxMatrix** : Contrôle du panneau P10 RGB
- **RTClib** : Interface avec le module DS3231
- **Preferences** : Sauvegarde des paramètres dans la flash
- **WiFi** : Connectivité réseau
- **WebServer** : Interface web de configuration

## Support

Pour obtenir de l'aide :
1. Vérifiez ce README
2. Consultez les programmes d'exemple
3. Vérifiez les connexions et l'alimentation
4. Consultez la documentation des bibliothèques utilisées

## Crédit

Projet original adapté pour PlatformIO. Code source basé sur les exemples et tutoriels de la communauté ESP32/Arduino.

Bibliothèques utilisées :
- PxMatrix par 2dom (Dominic Buchstaller)
- RTClib par Adafruit
- Adafruit GFX Library
- Adafruit BusIO
