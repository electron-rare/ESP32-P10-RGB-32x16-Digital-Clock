# Guide de Test du Portail Captif

## Test Rapide - Portail Captif

### Étape 1 : Compiler et téléverser le firmware principal

```bash
# Si PlatformIO est installé
pio run -e main --target upload

# Ou via VS Code
# Utiliser la tâche "PlatformIO: Upload Main Project"
```

### Étape 2 : Configuration initiale

1. **Mode Point d'accès** : Assurez-vous que `useStationMode = false` dans le code
2. **Redémarrez l'ESP32** après le téléversement

### Étape 3 : Test du portail captif

1. **Connectez-vous au WiFi** :
   - Nom : `HOKA_Clock`
   - Mot de passe : `hoka`

2. **Test automatique** :
   - Ouvrez votre navigateur
   - Essayez d'aller sur n'importe quel site (google.com, facebook.com, etc.)
   - Vous devriez être automatiquement redirigé vers l'interface de l'horloge

3. **Test manuel** :
   - Accédez directement à : `http://192.168.1.1`

### Étape 4 : Vérification des fonctionnalités

1. **Interface web** : L'interface de configuration de l'horloge doit s'afficher
2. **Clé d'accès** : Utilisez `hoka` comme clé pour modifier les paramètres
3. **Redirection** : Toutes les tentatives de navigation doivent rediriger vers l'horloge

## Test Dédié - Portail Captif Simple

Pour tester uniquement la fonctionnalité de portail captif sans l'affichage LED :

### Étape 1 : Compiler le test

```bash
pio run -e captive_portal_test --target upload
```

### Étape 2 : Connection et test

1. **WiFi** : `HOKA_Clock_Test` (mot de passe : `hoka`)
2. **Navigation** : Essayez d'aller sur n'importe quel site
3. **Résultat attendu** : Page de test confirmant que le portail captif fonctionne

## Surveillance et débogage

### Monitoring série

```bash
pio device monitor
```

### Messages à surveiller

```
=== ESP32 P10 RGB Digital Clock ===
WIFI mode : AP avec Portail Captif
Serveur DNS démarré pour portail captif
HTTP server started
Captive Portal active - navigate to any website
```

### Débogage des problèmes

1. **Le portail ne s'ouvre pas** :
   - Vérifiez que `useStationMode = false`
   - Redémarrez l'ESP32
   - Testez l'accès direct à `http://192.168.1.1`

2. **Problèmes de redirection** :
   - Effacez le cache du navigateur
   - Testez avec un navigateur différent
   - Vérifiez les logs série pour voir les requêtes

3. **Compilation échoue** :
   - Vérifiez que `DNSServer` est dans les dépendances
   - Utilisez l'environnement `main` ou `captive_portal_test`

## Personnalisation

### Modifier le nom du réseau WiFi

Dans `src/main.cpp` :
```cpp
const char* ap_ssid = "MON_HORLOGE";  // Changez ici
```

### Modifier l'IP du portail

```cpp
const IPAddress apIP(192, 168, 1, 1);  // Changez ici
```

### Ajouter des routes personnalisées

```cpp
server.on("/ma-route", maFonction);
```

## Compatibilité testée

✅ **Android** : Notification automatique "Se connecter au réseau"  
✅ **iOS** : Ouverture automatique du navigateur captif  
✅ **Windows** : Redirection lors de la première navigation  
✅ **macOS** : Détection automatique du portail captif  
✅ **Linux** : Redirection manuelle nécessaire  

## Prochaines étapes

Une fois le portail captif testé et fonctionnel :

1. **Personnalisez l'interface** web selon vos besoins
2. **Ajoutez des fonctionnalités** (WPS, configuration WiFi avancée, etc.)
3. **Optimisez les performances** selon votre configuration matérielle
4. **Testez avec différents appareils** pour garantir la compatibilité

## Support

Pour toute question ou problème :
- Vérifiez les logs série avec `pio device monitor`
- Consultez la documentation dans `CAPTIVE_PORTAL_README.md`
- Testez d'abord avec l'environnement `captive_portal_test`
