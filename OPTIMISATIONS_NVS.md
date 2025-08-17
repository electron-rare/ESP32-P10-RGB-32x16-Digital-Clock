# Optimisations NVS - Correction du problème de corruption

## Problème initial
L'erreur `assert failed: xQueueSemaphoreTake queue.c:1554` indiquait une corruption des sémaphores FreeRTOS lors de la sauvegarde des paramètres en mémoire NVS.

## Causes identifiées

1. **Conflit entre timer d'affichage et NVS** : Le timer d'interruption pour l'affichage pouvait interrompre la sauvegarde NVS
2. **Timeouts inadéquats** : Des timeouts trop longs pouvaient créer des deadlocks
3. **Absence de vérifications de sécurité** : Aucune vérification du contexte avant utilisation des sémaphores
4. **Gestion d'erreur insuffisante** : Pas de mécanisme de récupération en cas de corruption

## Solutions implémentées

### 1. Fonction de vérification de sanité des mutex
```cpp
bool checkMutexSanity() {
  // Vérifier que les mutex sont valides
  // Vérifier l'état du scheduler
  // Vérifier que nous ne sommes pas dans une ISR
}
```

### 2. Protection de la fonction saveSettings()
- Désactivation temporaire du timer d'affichage
- Vérification du contexte avant acquisition des mutex
- Timeouts courts (50ms max) pour éviter les blocages
- Mécanisme de retry pour les opérations NVS
- Gestion d'exception pour les écritures NVS

### 3. Amélioration de la fonction display_updater()
- Double vérification de la validité du timer
- Protection contre les accès concurrents

### 4. Système de surveillance (watchdog)
- Vérification périodique de la sanité du système
- Compteur de corruption avec récupération automatique
- Surveillance de l'état des tâches et de la mémoire

### 5. Fonction de récupération d'urgence
```cpp
void emergencyRecovery() {
  // Arrêt propre de tous les timers et tâches
  // Nettoyage des mutex
  // Redémarrage du système
}
```

### 6. Réparation de corruption NVS
```cpp
void repairNVSCorruption() {
  // Effacement et réinitialisation de la partition NVS
  // Gestion des erreurs avec retry
}
```

## Optimisations techniques

### Timeouts optimisés
- **MUTEX_TIMEOUT_FAST** (50ms) : Pour les opérations rapides
- **MUTEX_TIMEOUT_NORMAL** (200ms) : Pour les opérations normales
- **MUTEX_TIMEOUT_SLOW** (1000ms) : Pour les opérations I/O
- **MUTEX_TIMEOUT_CRITICAL** (2000ms) : Pour les opérations critiques

### Gestion des contextes
- Vérification `xPortInIsrContext()` avant toute opération mutex
- Vérification `xTaskGetSchedulerState()` pour s'assurer que le scheduler fonctionne
- Désactivation temporaire des interruptions lors des opérations critiques

### Mécanismes de retry
- Retry automatique pour `preferences.begin()`
- Fallback vers des valeurs par défaut en cas d'échec
- Sauvegarde différée en cas de contexte inapproprié

## Résultats attendus

1. **Élimination des corruptions** : Les vérifications de contexte empêchent les accès concurrents dangereux
2. **Robustesse accrue** : Le système peut se récupérer automatiquement des corruptions mineures
3. **Performance maintenue** : Les timeouts courts évitent les blocages sans impact sur les performances
4. **Monitoring continu** : Le watchdog détecte et corrige proactivement les problèmes

## Instructions d'utilisation

1. Compiler et uploader le firmware optimisé
2. Surveiller les logs série pour détecter les éventuels problèmes
3. Le système se récupère automatiquement des corruptions mineures
4. En cas de corruption grave, le système redémarre automatiquement après nettoyage

## Logs de débogage

Le système produit maintenant des logs détaillés :
- `"Saving settings to NVS..."` : Début de sauvegarde
- `"Settings saved successfully"` : Sauvegarde réussie
- `"Mutex sanity check failed"` : Problème de mutex détecté
- `"System corruption detected"` : Corruption du système
- `"EMERGENCY RECOVERY"` : Récupération d'urgence en cours

## Maintenance

- Surveiller les compteurs de corruption dans les logs
- Vérifier périodiquement l'état de la mémoire heap
- En cas de corruptions répétées, vérifier l'alimentation et la qualité des connexions
