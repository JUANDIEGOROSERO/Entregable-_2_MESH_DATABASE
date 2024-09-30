#include "painlessMesh.h"
#include <Arduino_JSON.h>

#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
#define DEVICE "ESP32"
  #elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Configuración de la red Mesh
#define MESH_PREFIX "whateveryoulike"
#define MESH_PASSWORD "somethingSneak"
#define MESH_PORT 1515

// Configuración WiFi
#define WIFI_SSID "FAMILIA_BENAVIDES"
#define WIFI_PASSWORD "juan2004"

// Configuración de InfluxDB
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "5TICmC_HubInWYvWkdNaQPHrN7aw0nZyCEEjsVa_43DfPY_Fbli72M4YtxoUV_2XymZC54QshKj0LbzyzCTW9A=="
#define INFLUXDB_ORG "16446c2508ada63c"
#define INFLUXDB_BUCKET "ESP32flowcabron"

// Pines del LED RGB
#define RED_PIN 13
#define GREEN_PIN 14
#define BLUE_PIN 12

// Zona horaria
#define TZ_INFO "UTC-5"

// Cliente InfluxDB
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Scheduler y mesh
Scheduler userScheduler;
painlessMesh mesh;

// Variables
float temp_f = 0.0f, hum_f = 0.0f; // Cambiado a float
double temperature = 0, humidity = 0;
int redValue = 0, greenValue = 0, blueValue = 0;

// Punto de datos para InfluxDB
Point sensor("environment_data");

// Prototipos de funciones
void controlLedRGB(double temp);
void sendToInfluxDB(float temp, float hum);
void receivedCallback(uint32_t from, String &msg);

// Función para controlar el LED RGB
/* Esta función ajusta los valores de los pines del LED RGB en función
   de la temperatura recibida. Si la temperatura es menor a 20°C, el
   LED se establece en azul (indica frío). Si está entre 20°C y 30°C,
   el LED se establece en verde (indica temperatura moderada). Si es
   mayor a 30°C, el LED se establece en rojo (indica calor). Los valores
   se escriben en los pines correspondientes del LED RGB. */
void controlLedRGB(double temp) {
  if (temp < 20) {
    redValue = 0; greenValue = 0; blueValue = 255; // Frío
  } else if (temp >= 20 && temp < 30) {
    redValue = 0; greenValue = 255; blueValue = 0; // Moderado
  } else {
    redValue = 255; greenValue = 0; blueValue = 0; // Calor
  }

  analogWrite(RED_PIN, redValue);
  analogWrite(GREEN_PIN, greenValue);
  analogWrite(BLUE_PIN, blueValue);

}

// Función para enviar datos a InfluxDB
/* Esta función envía los datos de temperatura y humedad al servidor
   InfluxDB. Se crea un punto de datos utilizando la clase Point y se
   añaden los campos correspondientes. Si hay un error al enviar los
   datos, se imprime un mensaje de error en el monitor serie. En caso
   contrario, se indica que los datos se enviaron correctamente. */
void sendToInfluxDB(float temp, float hum) {
  
  sensor.clearFields();
  sensor.addField("temperature", temp);
  sensor.addField("humidity", hum);

  if (!client.writePoint(sensor)) {
    Serial.print("Error al escribir en InfluxDB: ");
    Serial.println(client.getLastErrorMessage());
  } else {
    Serial.println("Datos enviados a InfluxDB con éxito");
  }
}

// Callback al recibir mensajes
/* Esta función se ejecuta cuando se recibe un mensaje en la red mesh.
   El mensaje se analiza como un objeto JSON para extraer los valores
   de temperatura y humedad. Luego, se llama a la función controlLedRGB
   para ajustar el LED RGB y a la función sendToInfluxDB para enviar
   los datos a la base de datos. Se imprime un mensaje de error si
   hay problemas al parsear el JSON. */
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Mensaje recibido desde %u: %s\n", from, msg.c_str());

  JSONVar myObject = JSON.parse(msg.c_str());
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Error al parsear JSON");
    return;
  }

  temperature = (double)myObject["temp"];
  humidity = (double)myObject["hum"];

  float temp_f = temperature;
  float hum_f = humidity;
  controlLedRGB(temperature); // Controlar LED RGB
  sendToInfluxDB(temp_f, hum_f); //Control de Base de Datos
}

// Configuración inicial
void setup() {
  Serial.begin(115200);

  // Conectar a WiFi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConectado a WiFi");
  // Sincronización de tiempo para InfluxDB
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Verificar conexión a InfluxDB
  if (client.validateConnection()) {
    Serial.print("Conectado a InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("Fallo la conexión a InfluxDB: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Prueba adicional de conexión a base de datos
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conexión a Internet establecida.");
    
    // Validar la conexión a InfluxDB
    if (client.validateConnection()) {
      Serial.println("Conexión a InfluxDB exitosa.");
    } else {
      Serial.println("Error en la conexión a InfluxDB: " + client.getLastErrorMessage());
    }
  } else {
    Serial.println("No hay conexión a Internet.");
  }

  // Inicializar pines del LED RGB
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  // Inicializar la red mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
}

// Bucle principal
void loop() {
  mesh.update();
}