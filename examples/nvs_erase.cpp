/**
 * Effacement et reformatage de la mémoire NVS
 * Utilisez ce programme si vous voulez réinitialiser la mémoire de préférences
 */

#include <Arduino.h>
#include <nvs_flash.h>

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(1000);

  Serial.println("=== NVS Memory Erase Tool ===");
  Serial.println("Erasing the NVS partition...");
  nvs_flash_erase();  // Effacer la partition NVS
  
  Serial.println("Initializing the NVS partition...");
  nvs_flash_init();   // Initialiser la partition NVS
  
  Serial.println("NVS Memory erase completed successfully.");
  Serial.println("You can now upload your main program.");
}

void loop() {
  delay(10);
}
