#include <driver/i2s.h>
#include <SPIFFS.h>
#include <WiFi.h> //подключаем библиотеку для работы с WiFi
#include <PubSubClient.h> // подключаем библиотеку для работы с MQTT

#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
// L/R -> GND
// VDD -> 3.3V
// GND -> GND
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE   (16000)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      (16 * 1024)
#define RECORD_TIME       (10) //Seconds
#define I2S_CHANNEL_NUM   (1)
#define FLASH_RECORD_SIZE (I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * RECORD_TIME)

File file;
const char filename[] = "/recording.wav";
const int headerSize = 44;
const char* ssid = "Android";
const char* password = "sirius123";

// Адрес MQTT брокера и логин/пароль:
const char* mqtt_server = "10.127.251.120";
const char* mqttUsr = "miptfab";
const char* mqttPass = "miptfab2025";

WiFiClient espClient; 
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
String inmsg = "";
int BUILTIN_LED = 2;
String geoposition = "179.179 179.179";


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  digitalWrite(BUILTIN_LED, HIGH);
  //delay(10000);
  digitalWrite(BUILTIN_LED, LOW);

  while (!client.connected()) {
    Serial.println("reconnecting...");
    reconnect();
    sendData("/miptfab/esp32led/ledState/", "up");
  }

  SPIFFSInit();
  i2sInit();
  xTaskCreate(i2s_adc, "i2s_adc", 1024 * 3, NULL, 1, NULL);



}

void loop() {

  // if (!client.connected()) {
  //   Serial.println("reconnecting...");
  //   reconnect();
  //   sendData("/miptfab/esp32led/ledState/", "up");
  // }
  // client.loop();
}


void send_geoposition_and_audio() {
    sendData("/miptfab/esp32led/geoposition/", geoposition);
    sendData("/miptfab/esp32led/audio/", "start");
    sendData("/miptfab/esp32led/ledState/", "sent");
    Serial.println("sent");

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
    //byte* audio = new byte[311340];
    // char audio[327724];
    // while (file.available()) {
    //     char b = file.read();
    //     audio[cnt] = b;
    //     Serial.print(b, HEX);


    //   //cnt++;
    // }
    // file.close();
    // sendData("/miptfab/esp32led/audio/", audio);
    // Serial.println('cnt');
    // Serial.print(cnt);
    // unsigned int len = 327724;
    //client.publish("/miptfab/esp32led/audio/", "a");
    //delete[] audio;
    char audio[125];
    cnt = 0;
    while (file.available() && cnt < 125) {
        char b = file.read();
        audio[cnt] = b;
        Serial.print(b, HEX);
        cnt++;
    }
    file.close();
    sendData("/miptfab/esp32led/audio/", audio);
    Serial.println('cnt');
    Serial.print(cnt);
    //unsigned int len = 327724;

    //recording_available = true;
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void callback(char* topic, byte* payload, unsigned int length) {
  inmsg = "";
  // Serial.print("Message arrived [");
  // Serial.print(topic);
  // Serial.print("] ");
  for (int i = 0; i < length; i++) {
    inmsg += (char)payload[i];
    // Serial.print((char)payload[i]);
  }
  // Serial.println(inmsg);
  // Serial.println("from topic: " + inmsg);

  if ((String)inmsg.c_str() == "off") {
    digitalWrite(BUILTIN_LED, LOW);  // выключаем светодиод
    Serial.println(inmsg.c_str());
    sendData("/miptfab/esp32led/ledState/", "OFF"); // отправляем состояние светодиода
  } else {
    digitalWrite(BUILTIN_LED, HIGH); // включаем светодиод
    Serial.println(inmsg.c_str());
    sendData("/miptfab/esp32led/ledState/", "ON"); // отправляем состояние светодиода
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "esp32testClient-";
    clientId += String(random(0xffff), HEX);
    // пытаемся подключиться к брокеру MQTT
    if (client.connect(clientId.c_str(), mqttUsr, mqttPass)) {
      Serial.println("connected");
      // Как только подключились, сообщаем эту прекрасную весть...
      client.publish("/miptfab/esp32led/ledState/", "connected");
      // ... ну и переподписываемся на нужный топик
      client.subscribe("/miptfab/esp32led/ledControl/");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Ждём 5 секунд перед следующей попыткой подключиться к брокеру MQTT
      delay(5000);
    }
  }
}

bool connectionUp(String paramOne) {
  String msgj = paramOne;
  if (!client.connected()) {
    reconnect();
  } else {
    client.publish("/miptfab/esp32led/ledState/", msgj.c_str());

     Serial.println(msgj.c_str());
  }
  return true;
}

bool sendData(String topic, String data) {
  // String msgj = paramOne;
  if (!client.connected()) {
    reconnect();
  } else {
    client.publish(topic.c_str(), data.c_str());
//     Serial.println(topic + " " + data);
  }
  return true;
}


void SPIFFSInit(){
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS initialisation failed!");
    while(1) yield();
  }

  SPIFFS.remove(filename);
  file = SPIFFS.open(filename, FILE_WRITE);
  if(!file){
    Serial.println("File is not available!");
  }

  byte header[headerSize];
  wavHeader(header, FLASH_RECORD_SIZE);

  file.write(header, headerSize);
  for (int i = 0; i < headerSize; i++) {
    Serial.print(header[i], HEX);
  }
  listSPIFFS();
}

void i2sInit(){
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 64,
    .dma_buf_len = 1024,
    .use_apll = 1
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}


void i2s_adc_data_scale(uint8_t * d_buff, uint8_t* s_buff, uint32_t len)
{
    uint32_t j = 0;
    uint32_t dac_value = 0;
    for (int i = 0; i < len; i += 2) {
        dac_value = ((((uint16_t) (s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
        d_buff[j++] = 0;
        d_buff[j++] = dac_value * 256 / 2048;
    }
}

void i2s_adc(void *arg)
{
    
    int i2s_read_len = I2S_READ_LEN;
    int flash_wr_size = 0;
    size_t bytes_read;

    char* i2s_read_buff = (char*) calloc(i2s_read_len, sizeof(char));
    uint8_t* flash_write_buff = (uint8_t*) calloc(i2s_read_len, sizeof(char));
    
    Serial.println(" *** Recording Start *** ");
    while (flash_wr_size < FLASH_RECORD_SIZE) {
        //read data from I2S bus, in this case, from ADC.
        i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
        //example_disp_buf((uint8_t*) i2s_read_buff, 64);
        //save original data from I2S(ADC) into flash.
        //i2s_adc_data_scale(flash_write_buff, (uint8_t*)i2s_read_buff, i2s_read_len);
        file.write((const byte*) flash_write_buff, i2s_read_len);
        // for (int i = 0; i < i2s_read_len; i++) {
        //   Serial.print(flash_write_buff[i], HEX);
        // }
        flash_wr_size += i2s_read_len;
        ets_printf("Sound recording %u%%\n", flash_wr_size * 100 / FLASH_RECORD_SIZE);
        ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
    }
    file.close();

    free(i2s_read_buff);
    i2s_read_buff = NULL;
    free(flash_write_buff);
    flash_write_buff = NULL;
    
    listSPIFFS();
    send_geoposition_and_audio();
    vTaskDelete(NULL);
}

void example_disp_buf(uint8_t* buf, int length)
{
    printf("======\n");
    for (int i = 0; i < length; i++) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 8 == 0) {
            printf("\n");
        }
    }
    printf("======\n");
}

void wavHeader(byte* header, int wavSize){
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSize = wavSize + headerSize - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;
  header[21] = 0x00;
  header[22] = 0x01;
  header[23] = 0x00;
  header[24] = 0x80;
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02;
  header[33] = 0x00;
  header[34] = 0x10;
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(wavSize & 0xFF);
  header[41] = (byte)((wavSize >> 8) & 0xFF);
  header[42] = (byte)((wavSize >> 16) & 0xFF);
  header[43] = (byte)((wavSize >> 24) & 0xFF);
  
}


void listSPIFFS(void) {
  Serial.println(F("\r\nListing SPIFFS files:"));
  static const char line[] PROGMEM =  "=================================================";

  Serial.println(FPSTR(line));
  Serial.println(F("  File name                              Size"));
  Serial.println(FPSTR(line));

  fs::File root = SPIFFS.open("/");
  if (!root) {
    Serial.println(F("Failed to open directory"));
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(F("Not a directory"));
    return;
  }

  fs::File file = root.openNextFile();
  while (file) {

    if (file.isDirectory()) {
      Serial.print("DIR : ");
      String fileName = file.name();
      Serial.print(fileName);
    } else {
      String fileName = file.name();
      Serial.print("  " + fileName);
      // File path can be 31 characters maximum in SPIFFS
      int spaces = 33 - fileName.length(); // Tabulate nicely
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      String fileSize = (String) file.size();
      spaces = 10 - fileSize.length(); // Tabulate nicely
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      Serial.println(fileSize + " bytes");
    }

    file = root.openNextFile();
  }

  Serial.println(FPSTR(line));
  Serial.println();
  delay(1000);

}