#include <WiFi.h> //подключаем библиотеку для работы с WiFi
#include <PubSubClient.h> // подключаем библиотеку для работы с MQTT

// Замените значения SSID/Password на те, что вы указали при настройке роутера
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

void setup() {
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  digitalWrite(BUILTIN_LED, HIGH);
  delay(3000);
  digitalWrite(BUILTIN_LED, LOW);
}

void loop() {

  if (!client.connected()) {
    reconnect();
    sendData("/miptfab/esp32led/ledState/", "up");
  }
  client.loop();
}
