#include <Arduino.h>          // Основная библиотека Arduino
#include <driver/i2s.h>       // Драйвер для работы с I2S (цифровые аудиоустройства)
#include <SPI.h>              // Библиотека для работы с интерфейсом SPI
#include <WiFi.h> //подключаем библиотеку для работы с WiFi
#include <PubSubClient.h> // подключаем библиотеку для работы с MQTT

// Настройки I2S микрофона
#define I2S_WS 25            // Пин для сигнала Word Select (выбор слова)
#define I2S_SD 33            // Пин для сигнала Serial Data (последовательные данные)
#define I2S_SCK 32           // Пин для сигнала Serial Clock (последовательные тактовые импульсы)
#define I2S_PORT I2S_NUM_0   // Используемый порт I2S (на ESP32 их два: I2S_NUM_0 и I2S_NUM_1)


// Настройки кнопки
#define BUTTON_PIN 0         // Встроенная кнопка на NodeMCU-32S 38pin (расположена справа от USB-разъёма)

// Настройки записи
#define SAMPLE_RATE 44100    // Частота дискретизации (44.1 кГц - стандартная для аудио)
#define BUFFER_SIZE 1024     // Размер буфера для хранения сэмплов


// File audioFile;              // Объект для работы с файлом 
int16_t i2sBuffer[BUFFER_SIZE]; // Буфер для хранения аудиоданных (16-битные целые числа)
volatile bool isRecording = false; // Флаг состояния записи (volatile для работы в прерываниях)
volatile bool buttonPressed = false; // Флаг нажатия кнопки (volatile для работы в прерываниях)
int fileNumber = 0;          // Счетчик для нумерации файлов
const uint32_t HEADER_SIZE = 44;
int16_t finalBuffer[BUFFER_SIZE + HEADER_SIZE];
uint8_t header[44];


// Замените значения SSID/Password на те, что вы указали при настройке роутера
const char* ssid = "MTEST";
const char* password = "mtestpass";
const char* espHostname = "microphone";

// Адрес MQTT брокера и логин/пароль:
const char* mqtt_server = "192.168.42.190";
const char* mqttUsr = "miptfab";
const char* mqttPass = "miptfab";

WiFiClient espClient; 
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
String inmsg = "";
int BUILTIN_LED = 2;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.setHostname(espHostname);

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


// void callback(char* topic, byte* payload, unsigned int length) {
//   inmsg = "";
//   // Serial.print("Message arrived [");
//   // Serial.print(topic);
//   // Serial.print("] ");
//   for (int i = 0; i < length; i++) {
//     inmsg += (char)payload[i];
//     // Serial.print((char)payload[i]);
//   }
//   // Serial.println(inmsg);
//   // Serial.println("from topic: " + inmsg);

//   if ((String)inmsg.c_str() == "off") {
//     digitalWrite(BUILTIN_LED, LOW);  // выключаем светодиод
//     Serial.println(inmsg.c_str());
//     sendData("/miptfab/esp32led/ledState/", "OFF"); // отправляем состояние светодиода
//   } else {
//     digitalWrite(BUILTIN_LED, HIGH); // включаем светодиод
//     Serial.println(inmsg.c_str());
//     sendData("/miptfab/esp32led/ledState/", "ON"); // отправляем состояние светодиода
//   }
// }

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

bool sendData(String topic, const uint8_t * data, unsigned int data_length) {
  // String msgj = paramOne;
  if (!client.connected()) {
    reconnect();
  } else {
    client.publish(topic.c_str(), data, data_length);
//     Serial.println(topic + " " + data);
  }
  return true;
}

// Функция обработки прерывания кнопки
void IRAM_ATTR buttonISR() {
  buttonPressed = true;      // Устанавливаем флаг при нажатии кнопки
  // IRAM_ATTR указывает, что функция должна быть размещена в RAM (а не во flash) для быстрого выполнения
}

// Функция записи заголовка WAV файла
void writeWavHeader(int sampleRate, uint32_t totalSamples) {
  // Рассчитываем необходимые параметры для заголовка
  uint32_t byteRate = sampleRate * 2;  // 16-bit = 2 bytes per sample
  uint32_t totalDataSize = totalSamples * 2; // Общий размер данных в байтах
  uint32_t totalSize = totalDataSize + 36; // Общий размер файла (данные + заголовок)
  
//  // RIFF header (основной заголовок файла)
//  file.write('R'); file.write('I'); file.write('F'); file.write('F'); // "RIFF" метка
//  // Размер файла (минус 8 байт для "RIFF" и размера)
//  file.write((uint8_t)(totalSize & 0xFF));         // Младший байт
//  file.write((uint8_t)((totalSize >> 8) & 0xFF));  // Следующий байт
//  file.write((uint8_t)((totalSize >> 16) & 0xFF)); // ...
//  file.write((uint8_t)((totalSize >> 24) & 0xFF)); // Старший байт
//  file.write('W'); file.write('A'); file.write('V'); file.write('E'); // "WAVE" метка
//  
//  // fmt subchunk (описание формата данных)
//  file.write('f'); file.write('m'); file.write('t'); file.write(' '); // "fmt " метка
//  file.write(16); file.write(0); file.write(0); file.write(0);  // Subchunk1Size = 16 (для PCM)
//  file.write(1); file.write(0);  // AudioFormat = PCM (линейный несжатый)
//  file.write(1); file.write(0);  // NumChannels = 1 (моно)
//  // Частота дискретизации
//  file.write((uint8_t)(sampleRate & 0xFF));
//  file.write((uint8_t)((sampleRate >> 8) & 0xFF));
//  file.write((uint8_t)((sampleRate >> 16) & 0xFF));
//  file.write((uint8_t)((sampleRate >> 24) & 0xFF));
//  // Байтрейт (байт в секунду)
//  file.write((uint8_t)(byteRate & 0xFF));
//  file.write((uint8_t)((byteRate >> 8) & 0xFF));
//  file.write((uint8_t)((byteRate >> 16) & 0xFF));
//  file.write((uint8_t)((byteRate >> 24) & 0xFF));
//  file.write(2); file.write(0);  // BlockAlign = 2 (байт на сэмпл для моно 16-бит)
//  file.write(16); file.write(0);  // BitsPerSample = 16 (16-битное аудио)
//  
//  // data subchunk (начало блока данных)
//  file.write('d'); file.write('a'); file.write('t'); file.write('a'); // "data" метка
//  // Размер данных в байтах
//  file.write((uint8_t)(totalDataSize & 0xFF));
//  file.write((uint8_t)((totalDataSize >> 8) & 0xFF));
//  file.write((uint8_t)((totalDataSize >> 16) & 0xFF));
//  file.write((uint8_t)((totalDataSize >> 24) & 0xFF));



    // RIFF header (основной заголовок файла)
  header[0] = (uint8_t) ('R');
  header[1] = (uint8_t) ('I');
  header[2] = (uint8_t) ('F');
  header[3] = (uint8_t) ('F');
  // Размер файла (минус 8 байт для "RIFF" и размера)
  header[4] = (uint8_t)(totalSize & 0xFF);         // Младший байт
  header[5] = (uint8_t)((totalSize >> 8) & 0xFF);  // Следующий байт
  header[6] = (uint8_t)((totalSize >> 16) & 0xFF); // ...
  header[7] = (uint8_t)((totalSize >> 24) & 0xFF); // Старший байт

  header[8] = (uint8_t) ('W');
  header[9] = (uint8_t) ('A');
  header[10] = (uint8_t) ('V');
  header[11] = (uint8_t) ('E');
  // "WAVE" метка
  
  // fmt subchunk (описание формата данных)
  header[12] = (uint8_t) ('f');
  header[13] = (uint8_t) ('m');
  header[14] = (uint8_t) ('t');
  header[15] = (uint8_t) (' ');
  // "fmt " метка

  header[16] = (uint8_t) (16);
  header[17] = (uint8_t) (0);
  header[18] = (uint8_t) (0);
  header[19] = (uint8_t) (0);
  // Subchunk1Size = 16 (для PCM)
  header[20] = (uint8_t) (1);
  header[21] = (uint8_t) (0);
// AudioFormat = PCM (линейный несжатый)
  header[22] = (uint8_t) (1);
  header[23] = (uint8_t) (0);
// NumChannels = 1 (моно)

  // Частота дискретизации
  header[24] = (uint8_t)(sampleRate & 0xFF);
  header[25] = (uint8_t)((sampleRate >> 8) & 0xFF);
  header[26] = (uint8_t)((sampleRate >> 16) & 0xFF);
  header[27] = (uint8_t)((sampleRate >> 24) & 0xFF);
  // Байтрейт (байт в секунду)
  header[28] = (uint8_t)(byteRate & 0xFF);
  header[29] = (uint8_t)((byteRate >> 8) & 0xFF);
  header[30] = (uint8_t)((byteRate >> 16) & 0xFF);
  header[31] = (uint8_t)((byteRate >> 24) & 0xFF);

  header[32] = (uint8_t) (2);
  header[33] = (uint8_t) (0);
  header[34] = (uint8_t) (16);
  header[35] = (uint8_t) (0);  
 // BlockAlign = 2 (байт на сэмпл для моно 16-бит)
 // BitsPerSample = 16 (16-битное аудио)
  
  // data subchunk (начало блока данных)
  header[36] = (uint8_t) ('d');
  header[37] = (uint8_t) ('a');
  header[38] = (uint8_t) ('t');
  header[39] = (uint8_t) ('a');
  // "data" метка
  // Размер данных в байтах
  header[40] = (uint8_t)(totalDataSize & 0xFF);
  header[41] = (uint8_t)((totalDataSize >> 8) & 0xFF);
  header[42] = (uint8_t)((totalDataSize >> 16) & 0xFF);
  header[43] = (uint8_t)((totalDataSize >> 24) & 0xFF);
}

// Функция начала записи
// void startRecording() {
//   // Создание нового файла с уникальным именем
//   char filename[20];
//   do {
//     sprintf(filename, "/recording%d.wav", fileNumber++); // Формируем имя файла
//   } while(SPIFFS.exists(filename)); // Проверяем, не существует ли уже файл с таким именем
  
//   // Открываем файл для записи
//   audioFile = SPIFFS.open(filename, FILE_WRITE);
//   if(!audioFile) {
//     Serial.println("Ошибка создания файла!");
//     return;
//   }
  
  // Записываем временный заголовок WAV файла (с нулевым размером данных)
  // Фактический размер данных будет обновлен при остановке записи
//   writeWavHeader(audioFile, SAMPLE_RATE, 0);
  
//   // Устанавливаем флаг записи
//   isRecording = true;
//   Serial.println("Начало записи...");
// }

// Функция остановки записи
// void stopRecording() {
//   // Сбрасываем флаг записи
//   isRecording = false;
  
//   if (audioFile) {
//     // Вычисляем количество записанных сэмплов
//     // Размер файла минус размер заголовка (36 байт), деленный на 2 (16 бит = 2 байта на сэмпл)
//     uint32_t totalSamples = (audioFile.size() - 36) / 2;
    
//     // Переходим к началу файла и обновляем заголовок с правильным размером данных
//     audioFile.seek(0);
//     writeWavHeader(audioFile, SAMPLE_RATE, totalSamples);
    
//     // Закрываем файл
//     audioFile.close();
//     Serial.println("Запись завершена.");
//     Serial.print("Записано сэмплов: ");
//     Serial.println(totalSamples);
//   }
// }

// Функция инициализации
void setup() {
  //SPIFFS.format()
  Serial.begin(115200); // Инициализация последовательного порта для отладки
  delay(1000); // Даем время для стабилизации
  pinMode(BUILTIN_LED, OUTPUT);

  // setup_wifi();
  // client.setServer(mqtt_server, 1883);
  // client.setCallback(callback);
  // digitalWrite(BUILTIN_LED, HIGH);
  // delay(3000);
  // digitalWrite(BUILTIN_LED, LOW);
  
  // Настройка I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // Режим мастера и приемника
    .sample_rate = SAMPLE_RATE, // Частота дискретизации
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // 16 бит на сэмпл
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Только левый канал (моно)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S, // Стандартный формат I2S
    .intr_alloc_flags = 0, // Флаги прерываний (по умолчанию)
    .dma_buf_count = 8, // Количество буферов DMA
    .dma_buf_len = BUFFER_SIZE, // Размер каждого буфера DMA
    .use_apll = false // Не использовать APLL для тактирования
  };
  
  // Конфигурация пинов I2S
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,   // Пин тактового сигнала
    .ws_io_num = I2S_WS,     // Пин выбора слова
    .data_out_num = -1,      // Не используется (мы только принимаем данные)
    .data_in_num = I2S_SD    // Пин входных данных
  };
  
  // Установка драйвера I2S
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  // Настройка пинов I2S
  i2s_set_pin(I2S_PORT, &pin_config);
  
  
  // Настройка кнопки
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Вход с подтягивающим резистором
  // Настройка прерывания на спад сигнала (нажатие кнопки)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
  
  Serial.println("Готов к работе. Нажмите кнопку для начала записи.");
}

// Основной цикл программы
void loop() {
//    if (!client.connected()) {
//    reconnect();
//    sendData("/miptfab/esp32led/ledState/", "up");
//  }


  // Обработка нажатия кнопки
  if (buttonPressed) {
    buttonPressed = false; // Сбрасываем флаг
    delay(50); // Задержка для подавления дребезга контактов
    
    // В зависимости от текущего состояния, начинаем или останавливаем запись
    if (!isRecording) {
      isRecording = true;
      // startRecording();
    } else {
      isRecording = false;
      // stopRecording();
    }
  }
  
  // Если идет запись, читаем данные с I2S и записываем 
  if (isRecording) {                                    
    size_t bytesRead = 0;
    // Чтение данных из I2S в буфер
    i2s_read(I2S_PORT, &i2sBuffer, BUFFER_SIZE * sizeof(int16_t), &bytesRead, portMAX_DELAY);
    uint32_t totalSamples = (BUFFER_SIZE) / 2;
    writeWavHeader(SAMPLE_RATE, totalSamples);
    
    for (uint32_t i = 0; i < HEADER_SIZE; i++) {
      finalBuffer[i] = (int16_t) header[i];
      Serial.print(finalBuffer[i]);
    }

    for (int i = 0; i < BUFFER_SIZE; i++) {
      finalBuffer[HEADER_SIZE + i] = (int16_t) i2sBuffer[i];
      Serial.print(finalBuffer[HEADER_SIZE + i]);
    }


    
    
//    // Если данные прочитаны, отправляем их
//    if (bytesRead > 0) {
//      //audioFile.write((const uint8_t*)i2sBuffer, bytesRead);
//      sendData("/miptfab/esp32led/ledState/", (const uint8_t*)i2sBuffer, bytesRead);
//    }

    
  }

  client.loop();
}
