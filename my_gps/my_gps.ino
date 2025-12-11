#include <TinyGPS++.h>
// Создаем объект GPS
TinyGPSPlus gps;
String geoposition = "179.179 179.179";
void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
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
        geoposition = String(gps.location.lat(), 8) + " " + String(gps.location.lng(), 8);
        Serial.print(geoposition);
        // // Выводим время
        // Serial.print("Time: ");
        // Serial.print(gps.time.hour());
        // Serial.print(":");
        // Serial.print(gps.time.minute());
        // Serial.print(":");
        // Serial.println(gps.time.second());
        // // Выводим скорость
        // Serial.print("Speed: ");
        // Serial.print(gps.speed.kmph());
        // Serial.println(" km/h");
        delay(1000);
      }
    }
  }
}
