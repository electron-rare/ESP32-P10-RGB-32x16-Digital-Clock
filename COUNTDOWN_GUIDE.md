# Guide du Countdown (Compte à Rebours)

## Vue d'ensemble

La fonctionnalité de countdown permet d'afficher un compte à rebours en temps réel vers une date et heure cible spécifique. Le countdown s'affiche dans la rotation du texte défilant et peut être configuré via l'interface web.

## Fonctionnalités

### ✅ Affichage Dynamique
- **Format adaptatif** : Affiche jours, heures, minutes et secondes selon le temps restant
- **Couleur personnalisable** : Couleur spécifique pour le countdown (orange par défaut)
- **Indication d'expiration** : Affichage "EXPIRED!" en rouge quand le countdown est fini
- **Compatible cascade** : Fonctionne avec tous les types de configuration de panneaux

### ✅ Configuration Complète
- **Activation/désactivation** : Checkbox pour activer ou désactiver le countdown
- **Titre personnalisé** : Nom de l'événement (ex: "NOUVEL AN", "ANNIVERSAIRE")
- **Date et heure cible** : Configuration précise de la cible
- **Couleur RGB** : Personnalisation de la couleur d'affichage
- **Sauvegarde** : Tous les paramètres sont sauvegardés en mémoire flash

## Configuration via Interface Web

### 1. Accès à l'Interface
1. Connectez-vous au WiFi de l'ESP32 ou configurez votre WiFi
2. Ouvrez l'adresse IP dans votre navigateur
3. Entrez la clé de sécurité : `p10rgbesp32ws`

### 2. Section Countdown Timer
La section "Countdown Timer" contient :

#### Activation
- **Checkbox "Active"** : Cochez pour activer le countdown

#### Configuration de l'Événement
- **Title** : Nom de l'événement (max 50 caractères)
  - Exemples : "NEW YEAR", "BIRTHDAY", "WEDDING", "VACATION"

#### Date et Heure Cible
- **Target Date** : Jour, Mois, Année
- **Target Time** : Heure, Minute, Seconde (format 24h)

#### Couleur
- **RGB** : Valeurs Red, Green, Blue (0-255)
  - Orange par défaut : R=255, G=165, B=0
  - Rouge pour urgence : R=255, G=0, B=0
  - Bleu pour célébration : R=0, G=100, B=255

### 3. Boutons d'Action
- **"Set Countdown"** : Sauvegarde la configuration complète
- **"Set Countdown Color"** : Sauvegarde uniquement la couleur

## Formats d'Affichage

Le countdown adapte automatiquement son format selon le temps restant :

### Avec Jours
```
NEW YEAR: 15d 08h 30m 45s
```

### Sans Jours (moins de 24h)
```
NEW YEAR: 08h 30m 45s
```

### Dernière Heure
```
NEW YEAR: 30m 45s
```

### Dernière Minute
```
NEW YEAR: 45s
```

### Expiré
```
NEW YEAR - EXPIRED!
```

## Intégration dans l'Affichage

Le countdown s'intègre dans la rotation du texte défilant :

### Mode 1 (Couleurs Fixes)
1. **Date** : Affichage de la date actuelle
2. **Texte personnalisé** : Votre message personnel
3. **Countdown** : Compte à rebours (si actif)

### Mode 2 (Couleurs Changeantes)
1. **Date** : Couleur 1
2. **Texte personnalisé** : Couleur 2
3. **Countdown** : Couleur 3 (si actif)
4. **Changement** : Rotation des couleurs

## Exemples d'Utilisation

### 1. Nouvel An
```
Title: NEW YEAR 2026
Date: 31/12/2025 23:59:59
Couleur: Rouge (255,0,0)
```

### 2. Anniversaire
```
Title: BIRTHDAY
Date: 15/06/2025 00:00:00
Couleur: Rose (255,105,180)
```

### 3. Événement Professionnel
```
Title: CONFERENCE
Date: 20/03/2025 09:00:00
Couleur: Bleu (0,100,255)
```

### 4. Vacances
```
Title: HOLIDAYS
Date: 01/07/2025 12:00:00
Couleur: Vert (0,255,100)
```

## Test et Développement

### Test du Countdown
Un exemple de test spécifique est disponible :

```bash
# Compiler et uploader le test countdown
pio run -e countdown_test --target upload

# Monitorer la sortie série
pio device monitor
```

Le test configure automatiquement un countdown de 5 minutes dans le futur et affiche :
- L'heure actuelle en haut
- Le countdown en défilement
- Changement de couleur à l'expiration

### VS Code
Utilisez la tâche : **"PlatformIO: Test Countdown"**

## Sauvegarde et Persistance

Tous les paramètres du countdown sont sauvegardés dans la mémoire flash NVS :
- `cd_Active` : État activé/désactivé
- `cd_Year`, `cd_Month`, `cd_Day` : Date cible
- `cd_Hour`, `cd_Minute`, `cd_Second` : Heure cible
- `cd_Title` : Titre de l'événement
- `CD_R`, `CD_G`, `CD_B` : Couleur RGB

Les paramètres persistent après redémarrage de l'ESP32.

## Dépannage

### Le countdown ne s'affiche pas
1. Vérifiez que la checkbox "Active" est cochée
2. Confirmez que tous les champs de date/heure sont remplis
3. Vérifiez que la date cible est dans le futur

### Problème de format
- Le countdown s'adapte automatiquement au temps restant
- Si le temps est négatif, "EXPIRED!" s'affiche

### Couleur incorrecte
- Vérifiez les valeurs RGB (0-255)
- En mode 2, les couleurs du countdown rotent automatiquement

### RTC non synchronisé
- Configurez d'abord la date/heure système via l'interface web
- Le countdown dépend du module RTC DS3231

## Intégration dans Votre Projet

Le countdown peut être facilement adapté pour d'autres usages :

1. **Minuteur** : Countdown de courte durée (minutes/heures)
2. **Événements multiples** : Modification pour gérer plusieurs countdowns
3. **Notifications** : Ajout de signaux visuels à l'expiration
4. **API externe** : Récupération de dates cibles depuis Internet

## Performance

- **Mise à jour** : Chaque seconde
- **Mémoire** : ~200 bytes pour les variables countdown
- **Cascade** : Compatible avec tous les formats de panneaux
- **Stabilité** : Pas d'impact sur les autres fonctionnalités
