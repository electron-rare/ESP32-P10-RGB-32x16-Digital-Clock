# Schémas de Connexion - ESP32 P10 Digital Clock

## Vue d'ensemble du système

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Alimentation  │    │     ESP32       │    │   Panneau P10   │
│      5V         │    │   DEVKIT V1     │    │   RGB 32x16     │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ + ────────────────────│ VIN             │    │                 │
│ - ────────────────────│ GND             │    │                 │
└─────────────────┘    │                 │    │                 │
                       │                 │    │                 │
┌─────────────────┐    │                 │    │                 │
│   RTC DS3231    │    │                 │    │                 │
├─────────────────┤    │                 │    │                 │
│ VCC ──────────────────│ 3.3V            │    │                 │
│ GND ──────────────────│ GND             │    │                 │
│ SDA ──────────────────│ GPIO21          │    │                 │
│ SCL ──────────────────│ GPIO22          │    │                 │
└─────────────────┘    │                 │    │                 │
                       │ GPIO2  ─────────────────│ R1              │
                       │ GPIO15 ─────────────────│ G1              │
                       │ GPIO4  ─────────────────│ B1              │
                       │ GPIO16 ─────────────────│ R2              │
                       │ GPIO17 ─────────────────│ G2              │
                       │ GPIO5  ─────────────────│ B2              │
                       │ GPIO19 ─────────────────│ A               │
                       │ GPIO23 ─────────────────│ B               │
                       │ GPIO18 ─────────────────│ C               │
                       │ GPIO14 ─────────────────│ CLK             │
                       │ GPIO32 ─────────────────│ STB (LAT)       │
                       │ GPIO33 ─────────────────│ OE              │
                       │ GND    ─────────────────│ GND (multiple)  │
                       └─────────────────┘    └─────────────────┘
```

## Connecteur HUB75 - Vue détaillée

```
     HUB75 Connector (Vue de face)
    ┌─────────────────────────┐
    │  1  2  3  4  5  6  7  8 │
    │ ┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐│
    │ │R││G││B││ ││R││G││B││ ││
    │ │1││1││1││G││2││2││2││G││
    │ └─┘└─┘└─┘│N│└─┘└─┘└─┘│N││
    │          │D│          │D││
    │          └─┘          └─┘│
    ├─────────────────────────┤
    │  9 10 11 12 13 14 15 16 │
    │ ┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐┌─┐│
    │ │A││B││C││G││C││S││O││G││
    │ │ ││ ││ ││N││L││T││E││N││
    │ │ ││ ││ ││D││K││B││ ││D││
    │ └─┘└─┘└─┘└─┘└─┘└─┘└─┘└─┘│
    └─────────────────────────┘

Pin 1 (R1)  → ESP32 GPIO2     Pin 9  (A)   → ESP32 GPIO19
Pin 2 (G1)  → ESP32 GPIO15    Pin 10 (B)   → ESP32 GPIO23  
Pin 3 (B1)  → ESP32 GPIO4     Pin 11 (C)   → ESP32 GPIO18
Pin 4 (GND) → ESP32 GND       Pin 12 (GND) → ESP32 GND
Pin 5 (R2)  → ESP32 GPIO16    Pin 13 (CLK) → ESP32 GPIO14
Pin 6 (G2)  → ESP32 GPIO17    Pin 14 (STB) → ESP32 GPIO32
Pin 7 (B2)  → ESP32 GPIO5     Pin 15 (OE)  → ESP32 GPIO33
Pin 8 (GND) → ESP32 GND       Pin 16 (GND) → ESP32 GND
```

## ESP32 DEVKIT V1 - Pinout

```
                           ESP32 DEVKIT V1
                    ┌─────────────────────────┐
                    │                         │
                    │  ●                   ● │ 
               3.3V │ [ ]               [ ] │ VIN ──── Alimentation 5V+
                GND │ [ ]               [ ] │ GND ──── Alimentation 5V-
              Touch │ [ ]               [ ] │ GPIO13
              Touch │ [ ]               [ ] │ GPIO12
              Touch │ [ ]               [ ] │ GPIO14 ──── CLK (P10)
              Touch │ [ ]               [ ] │ GPIO27
                    │ [ ]               [ ] │ GPIO26
                    │ [ ]               [ ] │ GPIO25
                    │ [ ]               [ ] │ GPIO33 ──── OE (P10)
                    │ [ ]               [ ] │ GPIO32 ──── STB (P10)
                    │ [ ]               [ ] │ GPIO35
                    │ [ ]               [ ] │ GPIO34
                    │ [ ]               [ ] │ GPIO39
                    │ [ ]               [ ] │ GPIO36
                 EN │ [ ]               [ ] │ GPIO23 ──── B (P10)
                    │ [ ]               [ ] │ GPIO22 ──── SCL (RTC)
            GPIO2 ──│ [ ]               [ ] │ GPIO21 ──── SDA (RTC)
            GPIO4 ──│ [ ]               [ ] │ GPIO19 ──── A (P10)
           GPIO16 ──│ [ ]               [ ] │ GPIO18 ──── C (P10)
           GPIO17 ──│ [ ]               [ ] │ GPIO5  ──── B2 (P10)
            GPIO5 ──│ [ ]               [ ] │ TX
           GPIO15 ──│ [ ]               [ ] │ RX
                    │                         │
                    │    ┌─────────────┐     │
                    │    │    ESP32    │     │
                    │    │    WiFi     │     │
                    │    │ Bluetooth   │     │
                    │    └─────────────┘     │
                    │                         │
                    │  USB ┌─────────────┐   │
                    │ ──── │             │   │
                    └──────┴─────────────┴───┘

Connexions P10:
GPIO2  → R1        GPIO19 → A  
GPIO15 → G1        GPIO23 → B
GPIO4  → B1        GPIO18 → C
GPIO16 → R2        GPIO14 → CLK
GPIO17 → G2        GPIO32 → STB
GPIO5  → B2        GPIO33 → OE

Connexions RTC:
GPIO21 → SDA       3.3V → VCC
GPIO22 → SCL       GND  → GND
```

## Module RTC DS3231

```
    ┌─────────────────┐
    │   RTC DS3231    │
    ├─────────────────┤
    │ VCC ── 3.3V     │ ──── ESP32 3.3V
    │ GND ── GND      │ ──── ESP32 GND  
    │ SCL ── Clock    │ ──── ESP32 GPIO22
    │ SDA ── Data     │ ──── ESP32 GPIO21
    └─────────────────┘
    
Note: Certains modules ont aussi:
- 32K (sortie 32kHz)  - Non utilisé
- SQW (signal carré)  - Non utilisé  
- RST (reset)         - Non utilisé
```

## Alimentation - IMPORTANT !

```
    ┌─────────────────┐     ┌─────────────────┐
    │ Alimentation    │     │    ESP32        │
    │     5V/3A       │     │   (via USB ou   │
    │                 │     │   pin VIN)      │
    ├─────────────────┤     ├─────────────────┤
    │ +5V ────────────┼─┬───┼── VIN           │
    │ GND ────────────┼─┼───┼── GND           │
    └─────────────────┘ │   └─────────────────┘
                        │   
                        │   ┌─────────────────┐
                        │   │   Panneau P10   │
                        │   │   RGB 32x16     │
                        │   ├─────────────────┤
                        └───┼── +5V           │
                            │  (via HUB75)    │
                            └─────────────────┘

ATTENTION: 
- Le panneau P10 nécessite du 5V avec suffisamment de courant
- Utilisez une alimentation 5V/3A minimum
- L'ESP32 peut être alimenté via USB pour les tests
- Pour un fonctionnement permanent, alimentez l'ESP32 via VIN avec du 5V
```

## Câblage recommandé

```
Étape 1: Connexions d'alimentation
ESP32 VIN ← 5V+
ESP32 GND ← 5V-
P10 +5V   ← 5V+ (via HUB75)
P10 GND   ← 5V- (via HUB75, plusieurs pins)

Étape 2: Connexions I2C (RTC)
ESP32 GPIO21 ← RTC SDA  
ESP32 GPIO22 ← RTC SCL
ESP32 3.3V   ← RTC VCC
ESP32 GND    ← RTC GND

Étape 3: Connexions HUB75 (P10)
ESP32 GPIO2  ← P10 R1
ESP32 GPIO15 ← P10 G1
ESP32 GPIO4  ← P10 B1
ESP32 GPIO16 ← P10 R2
ESP32 GPIO17 ← P10 G2
ESP32 GPIO5  ← P10 B2
ESP32 GPIO19 ← P10 A
ESP32 GPIO23 ← P10 B
ESP32 GPIO18 ← P10 C
ESP32 GPIO14 ← P10 CLK
ESP32 GPIO32 ← P10 STB/LAT
ESP32 GPIO33 ← P10 OE
ESP32 GND    ← P10 GND (plusieurs connexions)
```

## Vérification des connexions

Utilisez un multimètre pour vérifier :
1. **Continuité** entre ESP32 et P10 pour chaque signal
2. **Absence de court-circuit** entre VCC et GND
3. **Tension 5V** sur les pins d'alimentation du P10
4. **Tension 3.3V** sur le module RTC

## Troubleshooting connexions

| Problème | Vérification |
|----------|-------------|
| Affichage noir | Alimentation 5V, connexion OE |
| Couleurs incorrectes | Connexions R1,G1,B1,R2,G2,B2 |
| Affichage instable | Connexions CLK, STB, GND |
| Position incorrecte | Connexions A, B, C |
| RTC ne fonctionne pas | Connexions SDA, SCL, alimentation 3.3V |
