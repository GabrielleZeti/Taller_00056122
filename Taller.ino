#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ======================================================
// CONFIGURACIÓN WIFI
// ======================================================

const char* WIFI_SSID = "Pixel7Pro";
const char* WIFI_PASSWORD = "13245678";


// ======================================================
// CONFIGURACIÓN MQTT
// ======================================================


const char* MQTT_SERVER = "10.251.26.98";
const int MQTT_PORT = 1883;

// Topics utilizados por el dashboard React.
const char* TOPIC_TEMPERATURA = "clima/esp32/temperatura";
const char* TOPIC_HUMEDAD = "clima/esp32/humedad";
const char* TOPIC_LUZ = "clima/esp32/luz";

// ======================================================
// CONFIGURACIÓN DE SENSORES
// ======================================================
#define DHTPIN 4       // Pin DATA del DHT11 conectado al GPIO 4
#define DHTTYPE DHT11  // Definir tipo de sensor DHT11

DHT dht(DHTPIN, DHTTYPE);

const int LDR_PIN = 32; // LDR conectada al divisor de voltaje en GPIO 32

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Enviar una lectura cada 2 segundos.
const unsigned long INTERVALO_ENVIO = 2000;
unsigned long ultimoEnvio = 0;

void conectarWiFi() {
  Serial.print("Conectando a WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a Mosquitto...");

    // Crear un Client ID diferente para este ESP32.
    String clientId = "ESP32-";
    clientId += String(
      (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF),
      HEX
    );

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" conectado");
    } else {
      Serial.print(" error, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Reintentando en 2 segundos...");
      delay(2000);
    }
  }
}

// ======================================================
// LEER Y PUBLICAR SENSORES
// ======================================================
void publicarSensores() {
  // 1. Obtener la temperatura y 2. humedad desde el DHT11
  float temperatura = dht.readTemperature();
  float humedad = dht.readHumidity();

  // Verificar si la lectura es correcta
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("¡Error al leer el sensor DHT11!");
    return;
  }

  // 3. Obtener la lectura del sensor de iluminación (LDR) y mapear a porcentaje (0 - 100%)
  int ldrAdc = analogRead(LDR_PIN);
  int luzPorcentaje = map(ldrAdc, 0, 4095, 0, 100);

  // 4. Preparar los valores convirtiéndolos a cadenas (char arrays)
  char tempStr[8];
  char humStr[8];
  char luzStr[8];

  dtostrf(temperatura, 4, 2, tempStr);
  dtostrf(humedad, 4, 2, humStr);
  sprintf(luzStr, "%d", luzPorcentaje);

  // 5. Publicar cada dato en su topic correspondiente
  mqttClient.publish(TOPIC_TEMPERATURA, tempStr);
  mqttClient.publish(TOPIC_HUMEDAD, humStr);
  mqttClient.publish(TOPIC_LUZ, luzStr);

  // 6. Mostrar las mediciones en el Monitor Serial
  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  Serial.print("Humedad: ");
  Serial.print(humedad);
  Serial.println(" %");

  Serial.print("LDR ADC: ");
  Serial.print(ldrAdc);
  Serial.print(" | Luz: ");
  Serial.print(luzPorcentaje);
  Serial.println(" %");
  Serial.println("------------------------------");
}

void setup() {
  Serial.begin(115200);

  // Inicializar los sensores y configurar los pines necesarios
  dht.begin();
  pinMode(LDR_PIN, INPUT);

  conectarWiFi();

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );
}

void loop() {
  // Si se pierde WiFi, volver a conectarse.
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  // Si se pierde MQTT, volver a conectarse.
  if (!mqttClient.connected()) {
    conectarMQTT();
  }

  mqttClient.loop();

  // Publicar sensores cada 2 segundos.
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = millis();
    publicarSensores();
  }
}