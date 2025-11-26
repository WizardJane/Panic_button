#include <driver/i2s.h>
#include <SPIFFS.h>

File file;
const char filename[] = "/recording.wav";



void setup() {
  Serial.begin(115200);
  SPIFFSInit();
}

void loop() {
  // put your main code here, to run repeatedly:
}


void SPIFFSInit() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialisation failed!");
    return;
  }

  file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("File is not available!");
  }
  //Serial.print("New file");
  int cnt = 0;
  while (file.available()) {
    cnt++;
    //printf("%02x ", buf[i]);
    // c += 1
    // if (c == 65000) {
    //   Serial.println();
    //   c = 0;
    // }
    byte b = file.read();
    Serial.print(b, HEX);
  }
  Serial.println('cnt');
  Serial.print(cnt);


  file.close();
}