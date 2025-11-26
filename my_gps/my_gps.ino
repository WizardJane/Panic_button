#include <TinyGPS++.h>
// Создаем объект GPS
TinyGPSPlus gps;
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
}
void loop() {
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      // Если есть данные
      if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid() && gps.speed.isValid()) {
        // Выводим координаты
        Serial.print("Latitude: ");
        Serial.print(gps.location.lat(), 6);
        Serial.print(" Longitude: ");
        Serial.println(gps.location.lng(), 6);
        // Выводим время
        Serial.print("Time: ");
        Serial.print(gps.time.hour());
        Serial.print(gps.time.minute());
        Serial.print(":");
        Serial.println(gps.time.second());
        // Выводим скорость
        Serial.print("Speed: ");
        Serial.print(gps.speed.kmph());
        Serial.println(" km/h");
        delay(1000);
      }
      else {
      Serial.println("Problem 2");
      }
    } else {
      Serial.println("Problem 1");
    }
  }
}
