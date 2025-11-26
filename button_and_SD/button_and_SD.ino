#include <Arduino.h>          // Основная библиотека Arduino
#include <driver/i2s.h>       // Драйвер для работы с I2S (цифровые аудиоустройства)
#include <SD.h>               // Библиотека для работы с microSD-картой
#include <SPI.h>              // Библиотека для работы с интерфейсом SPI

// Настройки I2S микрофона
#define I2S_WS 25            // Пин для сигнала Word Select (выбор слова)
#define I2S_SD 33            // Пин для сигнала Serial Data (последовательные данные)
#define I2S_SCK 32           // Пин для сигнала Serial Clock (последовательные тактовые импульсы)
#define I2S_PORT I2S_NUM_0   // Используемый порт I2S (на ESP32 их два: I2S_NUM_0 и I2S_NUM_1)

// Настройки microSD-карты
#define SD_CS 5              // Пин для выбора microSD-карты (Chip Select)
#define SPI_MOSI 23          // Пин MOSI (Master Out Slave In) для SPI
#define SPI_MISO 19          // Пин MISO (Master In Slave Out) для SPI
#define SPI_SCK 18           // Пин SCK (Serial Clock) для SPI

// Настройки кнопки
#define BUTTON_PIN 0         // Встроенная кнопка на NodeMCU-32S 38pin (расположена справа от USB-разъёма)

// Настройки записи
#define SAMPLE_RATE 44100    // Частота дискретизации (44.1 кГц - стандартная для аудио)
#define BUFFER_SIZE 1024     // Размер буфера для хранения сэмплов

File audioFile;              // Объект для работы с файлом на microSD-карте
int16_t i2sBuffer[BUFFER_SIZE]; // Буфер для хранения аудиоданных (16-битные целые числа)
volatile bool isRecording = false; // Флаг состояния записи (volatile для работы в прерываниях)
volatile bool buttonPressed = false; // Флаг нажатия кнопки (volatile для работы в прерываниях)
int fileNumber = 0;          // Счетчик для нумерации файлов

// Функция обработки прерывания кнопки
void IRAM_ATTR buttonISR() {
  buttonPressed = true;      // Устанавливаем флаг при нажатии кнопки
  // IRAM_ATTR указывает, что функция должна быть размещена в RAM (а не во flash) для быстрого выполнения
}

// Функция записи заголовка WAV файла
void writeWavHeader(File file, int sampleRate, uint32_t totalSamples) {
  // Рассчитываем необходимые параметры для заголовка
  uint32_t byteRate = sampleRate * 2;  // 16-bit = 2 bytes per sample
  uint32_t totalDataSize = totalSamples * 2; // Общий размер данных в байтах
  uint32_t totalSize = totalDataSize + 36; // Общий размер файла (данные + заголовок)
  
  // RIFF header (основной заголовок файла)
  file.write('R'); file.write('I'); file.write('F'); file.write('F'); // "RIFF" метка
  // Размер файла (минус 8 байт для "RIFF" и размера)
  file.write((uint8_t)(totalSize & 0xFF));         // Младший байт
  file.write((uint8_t)((totalSize >> 8) & 0xFF));  // Следующий байт
  file.write((uint8_t)((totalSize >> 16) & 0xFF)); // ...
  file.write((uint8_t)((totalSize >> 24) & 0xFF)); // Старший байт
  file.write('W'); file.write('A'); file.write('V'); file.write('E'); // "WAVE" метка
  
  // fmt subchunk (описание формата данных)
  file.write('f'); file.write('m'); file.write('t'); file.write(' '); // "fmt " метка
  file.write(16); file.write(0); file.write(0); file.write(0);  // Subchunk1Size = 16 (для PCM)
  file.write(1); file.write(0);  // AudioFormat = PCM (линейный несжатый)
  file.write(1); file.write(0);  // NumChannels = 1 (моно)
  // Частота дискретизации
  file.write((uint8_t)(sampleRate & 0xFF));
  file.write((uint8_t)((sampleRate >> 8) & 0xFF));
  file.write((uint8_t)((sampleRate >> 16) & 0xFF));
  file.write((uint8_t)((sampleRate >> 24) & 0xFF));
  // Байтрейт (байт в секунду)
  file.write((uint8_t)(byteRate & 0xFF));
  file.write((uint8_t)((byteRate >> 8) & 0xFF));
  file.write((uint8_t)((byteRate >> 16) & 0xFF));
  file.write((uint8_t)((byteRate >> 24) & 0xFF));
  file.write(2); file.write(0);  // BlockAlign = 2 (байт на сэмпл для моно 16-бит)
  file.write(16); file.write(0);  // BitsPerSample = 16 (16-битное аудио)
  
  // data subchunk (начало блока данных)
  file.write('d'); file.write('a'); file.write('t'); file.write('a'); // "data" метка
  // Размер данных в байтах
  file.write((uint8_t)(totalDataSize & 0xFF));
  file.write((uint8_t)((totalDataSize >> 8) & 0xFF));
  file.write((uint8_t)((totalDataSize >> 16) & 0xFF));
  file.write((uint8_t)((totalDataSize >> 24) & 0xFF));
}

// Функция начала записи
void startRecording() {
  // Создание нового файла с уникальным именем
  char filename[20];
  do {
    sprintf(filename, "/recording%d.wav", fileNumber++); // Формируем имя файла
  } while(SD.exists(filename)); // Проверяем, не существует ли уже файл с таким именем
  
  // Открываем файл для записи
  audioFile = SD.open(filename, FILE_WRITE);
  if(!audioFile) {
    Serial.println("Ошибка создания файла!");
    return;
  }
  
  // Записываем временный заголовок WAV файла (с нулевым размером данных)
  // Фактический размер данных будет обновлен при остановке записи
  writeWavHeader(audioFile, SAMPLE_RATE, 0);
  
  // Устанавливаем флаг записи
  isRecording = true;
  Serial.println("Начало записи...");
}

// Функция остановки записи
void stopRecording() {
  // Сбрасываем флаг записи
  isRecording = false;
  
  if (audioFile) {
    // Вычисляем количество записанных сэмплов
    // Размер файла минус размер заголовка (36 байт), деленный на 2 (16 бит = 2 байта на сэмпл)
    uint32_t totalSamples = (audioFile.size() - 36) / 2;
    
    // Переходим к началу файла и обновляем заголовок с правильным размером данных
    audioFile.seek(0);
    writeWavHeader(audioFile, SAMPLE_RATE, totalSamples);
    
    // Закрываем файл
    audioFile.close();
    Serial.println("Запись завершена.");
    Serial.print("Записано сэмплов: ");
    Serial.println(totalSamples);
  }
}

// Функция инициализации
void setup() {
  Serial.begin(9600); // Инициализация последовательного порта для отладки
  delay(1000); // Даем время для стабилизации
  
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
  
  // Инициализация microSD-карты
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI); // Инициализация SPI с указанными пинами
  if (!SD.begin(SD_CS)) { // Инициализация microSD-карты с указанным пином выбора
    Serial.println("Ошибка инициализации microSD-карты!");
    while(1); // Бесконечный цикл при ошибке
  }
  Serial.println("microSD-карта инициализирована.");
  
  // Настройка кнопки
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Вход с подтягивающим резистором
  // Настройка прерывания на спад сигнала (нажатие кнопки)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
  
  Serial.println("Готов к работе. Нажмите кнопку для начала записи.");
}

// Основной цикл программы
void loop() {
  // Обработка нажатия кнопки
  if (buttonPressed) {
    buttonPressed = false; // Сбрасываем флаг
    delay(50); // Задержка для подавления дребезга контактов
    
    // В зависимости от текущего состояния, начинаем или останавливаем запись
    if (!isRecording) {
      startRecording();
    } else {
      stopRecording();
    }
  }
  
  // Если идет запись, читаем данные с I2S и записываем на microSD-карту
  if (isRecording) {
    size_t bytesRead = 0;
    // Чтение данных из I2S в буфер
    i2s_read(I2S_PORT, &i2sBuffer, BUFFER_SIZE * sizeof(int16_t), &bytesRead, portMAX_DELAY);
    
    // Если данные прочитаны, записываем их в файл
    if (bytesRead > 0) {
      audioFile.write((const uint8_t*)i2sBuffer, bytesRead);
    }
  }
}