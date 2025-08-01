# Guide des Panneaux P10 en Cascade

## 🔗 Vue d'ensemble

Ce projet supporte maintenant plusieurs panneaux P10 connectés en cascade pour créer des affichages plus larges ou plus hauts.

## 📐 Configurations Supportées

### Configurations Horizontales (recommandées)
- **1x1** : 32x16 pixels (1 panneau) - Configuration standard
- **2x1** : 64x16 pixels (2 panneaux) - Ideal pour texte plus long
- **3x1** : 96x16 pixels (3 panneaux) - Texte défilant étendu
- **4x1** : 128x16 pixels (4 panneaux) - Affichage type bandeau
- **6x1** : 192x16 pixels (6 panneaux) - Grand bandeau
- **8x1** : 256x16 pixels (8 panneaux) - Très grand bandeau

### Configurations Verticales/Mixtes
- **2x2** : 64x32 pixels (4 panneaux) - Affichage carré
- **1x2** : 32x32 pixels (2 panneaux) - Plus de hauteur
- **4x2** : 128x32 pixels (8 panneaux) - Grand affichage

## 🛠️ Connexion Physique

### Connexion en Cascade (Chaînage)
```
ESP32 ──── Panneau 1 ──── Panneau 2 ──── Panneau 3 ──── ...
           (Premier)      (Deuxième)     (Troisième)
```

### Détails de Connexion
1. **Premier panneau** : Connecté directement à l'ESP32 (voir WIRING.md)
2. **Panneaux suivants** : Connectés via les ports de sortie du panneau précédent
3. **Alimentation** : Chaque panneau doit être alimenté en 5V individuellement

### Important - Alimentation
```
⚠️  ATTENTION ALIMENTATION !

Chaque panneau P10 consomme environ 1-2A sous 5V
- 2 panneaux : 5V/4A minimum
- 4 panneaux : 5V/8A minimum  
- 6 panneaux : 5V/12A minimum

Utilisez une alimentation adaptée !
```

## 🎯 Compilation pour Cascade

### Commandes PlatformIO

```bash
# 2 panneaux horizontaux (64x16)
pio run -e cascade_2x1 --target upload

# 3 panneaux horizontaux (96x16) 
pio run -e cascade_3x1 --target upload

# 4 panneaux horizontaux (128x16)
pio run -e cascade_4x1 --target upload

# 2x2 panneaux (64x32)
pio run -e cascade_2x2 --target upload

# 6 panneaux horizontaux (192x16)
pio run -e cascade_6x1 --target upload

# 8 panneaux horizontaux (256x16)
pio run -e cascade_8x1 --target upload

# Test de cascade (2 panneaux)
pio run -e test_cascade --target upload
```

### Configuration Personnalisée

Pour une configuration non-standard, modifiez `platformio.ini` :

```ini
[env:custom_cascade]
extends = env:main
build_flags = 
    ${env.build_flags}
    -DMATRIX_WIDTH=32        ; Largeur d'un panneau
    -DMATRIX_HEIGHT=16       ; Hauteur d'un panneau  
    -DMATRIX_PANELS_X=5      ; Nombre panneaux horizontaux
    -DMATRIX_PANELS_Y=1      ; Nombre panneaux verticaux
    -DCASCADE_MODE=1
```

## 🧪 Test des Panneaux

### Test d'Alignement
```bash
pio run -e test_cascade --target upload
```

Ce test affiche :
1. **Bordures** : Contour de chaque panneau  
2. **Numérotation** : P1, P2, P3... sur chaque panneau
3. **Couleurs** : Test de toutes les couleurs
4. **Texte défilant** : Test sur toute la largeur

### Vérifications Visuelles
- ✅ Tous les panneaux s'allument
- ✅ Bordures alignées correctement
- ✅ Numérotation séquentielle (P1, P2, P3...)
- ✅ Couleurs uniformes sur tous les panneaux
- ✅ Texte défile sans coupure

## ⚙️ Ajustements Automatiques

### Luminosité Auto-Adaptée
```cpp
// Le code ajuste automatiquement la luminosité :
1-2 panneaux  : 125/255 (luminosité normale)
3-4 panneaux  : 100/255 (réduite)
5-6 panneaux  : 80/255  (plus réduite)
7+ panneaux   : 60/255  (très réduite)
```

### Centrage Automatique
- **Horloge** : Centrée automatiquement sur la largeur totale
- **Texte défilant** : Utilise toute la largeur disponible
- **Interface web** : Fonctionne normalement

## 🌐 Interface Web Adaptée

L'interface web fonctionne normalement avec les cascades :
- Configuration identique
- Texte défilant adapté à la largeur
- Couleurs appliquées sur tous les panneaux
- Luminosité globale

## 🔧 Dépannage Cascade

### Problème : Un panneau ne s'allume pas
1. ✅ Vérifiez l'alimentation 5V de chaque panneau
2. ✅ Vérifiez les connexions en cascade
3. ✅ Testez avec moins de panneaux

### Problème : Affichage décalé
1. ✅ Vérifiez l'ordre des panneaux (P1, P2, P3...)
2. ✅ Utilisez le test d'alignement
3. ✅ Vérifiez les paramètres PANELS_X et PANELS_Y

### Problème : Scintillement
1. ✅ Réduisez la luminosité
2. ✅ Vérifiez l'alimentation (suffisamment puissante ?)
3. ✅ Réduisez `PxMATRIX_SPI_FREQUENCY`

### Problème : Texte coupé
1. ✅ Vérifiez que TOTAL_WIDTH est correct
2. ✅ Utilisez le test de texte défilant
3. ✅ Vérifiez la configuration PANELS_X

## 📋 Checklist Installation Cascade

### Étape 1 : Préparation
- [ ] Alimentation suffisante calculée (2A × nombre de panneaux)
- [ ] Panneaux P10 testés individuellement
- [ ] Câbles de connexion préparés

### Étape 2 : Connexion Physique  
- [ ] Premier panneau connecté à l'ESP32
- [ ] Panneaux en cascade connectés
- [ ] Alimentation 5V sur chaque panneau
- [ ] Masse commune vérifiée

### Étape 3 : Configuration Logicielle
- [ ] Environnement cascade choisi (ex: cascade_2x1)
- [ ] Code compilé et téléversé
- [ ] Test d'alignement réussi

### Étape 4 : Test Final
- [ ] Tous les panneaux s'allument
- [ ] Numérotation correcte visible
- [ ] Horloge centrée correctement
- [ ] Texte défile sur toute la largeur
- [ ] Interface web accessible

## 💡 Conseils Optimisation

### Performance
- Plus de panneaux = plus de données à traiter
- Réduisez la luminosité si nécessaire
- Utilisez une alimentation stable

### Esthétique  
- Alignez physiquement les panneaux parfaitement
- Utilisez des supports adaptés
- Protégez les connexions

### Fiabilité
- Alimentation dimensionnée correctement
- Connexions bien fixées
- Test avant installation finale

## 🎨 Exemples d'Utilisation

### Bandeau d'Information (4x1)
```
┌─────────────────────────────┐
│     12:34    ESP32 CLOCK    │
│ Aujourd'hui 15°C - Beau... │
└─────────────────────────────┘
```

### Affichage Carré (2x2)
```
┌─────────────────┐
│     12:34       │
│   ESP32 CLOCK   │
│                 │
│ Message long... │
└─────────────────┘
```

### Grand Bandeau (8x1)
```
┌───────────────────────────────────────────────────────────┐
│                    12:34 - ESP32 DIGITAL CLOCK           │
│ Très long message défilant avec beaucoup d'informations... │
└───────────────────────────────────────────────────────────┘
```

La cascade de panneaux P10 permet de créer des affichages personnalisés adaptés à vos besoins !
