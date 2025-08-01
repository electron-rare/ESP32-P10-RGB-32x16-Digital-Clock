# Guide de Démarrage Rapide - ESP32 P10 Digital Clock

## 🚀 Démarrage en 5 minutes

### Étape 1 : Préparation
1. **Installez PlatformIO** dans VS Code
2. **Ouvrez ce dossier** dans VS Code
3. **Connectez votre ESP32** en USB

### Étape 2 : Configuration WiFi
Modifiez dans `src/main.cpp` lignes ~100 :
```cpp
const char* ssid = "VOTRE_WIFI";
const char* password = "VOTRE_MOT_DE_PASSE";
```

### Étape 3 : Câblage (IMPORTANT !)

#### Panneau P10 → ESP32
```
HUB75 → ESP32
R1 → GPIO2
G1 → GPIO15  
B1 → GPIO4
R2 → GPIO16
G2 → GPIO17
B2 → GPIO5
A → GPIO19
B → GPIO23
C → GPIO18
CLK → GPIO14
STB → GPIO32
OE → GPIO33
GND → GND (plusieurs connexions)
```

#### RTC DS3231 → ESP32
```
VCC → 3.3V
GND → GND
SDA → GPIO21
SCL → GPIO22
```

### Étape 4 : Test rapide
1. **Test du panneau P10** :
   ```bash
   pio run -e p10_test --target upload
   ```

2. **Test du RTC** :
   ```bash
   pio run -e rtc_test --target upload
   ```

### Étape 5 : Projet principal
```bash
pio run -e main --target upload
pio device monitor
```

## 🌐 Configuration Web

1. **Trouvez l'IP** dans le moniteur série
2. **Ouvrez votre navigateur** → http://IP_AFFICHEE
3. **Utilisez la clé** : `p10rgbesp32ws`
4. **Configurez** date, heure, couleurs, texte

## 🔧 Commandes Utiles

```bash
# Compilation principale
pio run -e main

# Upload et monitoring
pio run -e main -t upload && pio device monitor

# Test des composants
pio run -e rtc_test -t upload    # Test RTC
pio run -e p10_test -t upload    # Test P10
pio run -e nvs_erase -t upload   # Reset mémoire

# Nettoyage
pio run -t clean
```

## ⚡ Commandes VS Code
- **Ctrl+Shift+P** → "Tasks: Run Task"
- Choisissez parmi :
  - PlatformIO: Build Main Project
  - PlatformIO: Upload Main Project  
  - PlatformIO: Test RTC Module
  - PlatformIO: Test P10 Matrix
  - PlatformIO: Monitor Serial

## 🚨 Problèmes Courants

### L'affichage ne fonctionne pas
- ✅ Vérifiez l'**alimentation 5V** (crucial !)
- ✅ Vérifiez **toutes les connexions**
- ✅ Testez avec `pio run -e p10_test -t upload`

### WiFi ne se connecte pas  
- ✅ Vérifiez SSID/mot de passe
- ✅ L'ESP32 créera un point d'accès "ESP32_Clock"

### RTC ne fonctionne pas
- ✅ Vérifiez connexions I2C (SDA/SCL)
- ✅ Testez avec `pio run -e rtc_test -t upload`

### Paramètres perdus
- ✅ Effacez la mémoire : `pio run -e nvs_erase -t upload`

## 📱 Interface Web - Fonctionnalités

- **Date/Heure** : Configuration complète
- **Mode 1** : Couleurs manuelles (RGB)
- **Mode 2** : Changement automatique de couleurs
- **Luminosité** : 0-255
- **Texte défilant** : Message personnalisé
- **Vitesse** : Réglage du défilement

## 💡 Configuration Avancée

### Changement des pins
Modifiez dans `src/main.cpp` :
```cpp
#define P_LAT 5    // Pin Latch
#define P_A   19   // Pin A
#define P_B   23   // Pin B
#define P_C   18   // Pin C  
#define P_OE  4    // Pin Output Enable
```

### Réduction du bruit d'affichage
```cpp
#define PxMATRIX_SPI_FREQUENCY 8000000  // Réduire si bruit
```

## 📋 Checklist de Test

- [ ] ESP32 se connecte au WiFi
- [ ] Interface web accessible
- [ ] Heure s'affiche correctement
- [ ] Date défile en bas
- [ ] Texte personnalisé fonctionne
- [ ] Changement de couleurs (mode 2)
- [ ] Réglage luminosité
- [ ] Sauvegarde des paramètres

## 🎯 Prêt à utiliser !

Une fois tout testé, votre horloge affichera :
- **Heure** en haut (HH:MM avec : clignotant)
- **Date et texte** défilant en bas
- **Couleurs** selon le mode choisi
- **Configuration** via navigateur web

Clé d'accès web : **p10rgbesp32ws**
