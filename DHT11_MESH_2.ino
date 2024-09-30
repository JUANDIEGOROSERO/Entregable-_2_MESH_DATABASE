#include <Adafruit_Sensor.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include "DHT.h"
#include "esp_sleep.h"  // Librería para controlar el deep sleep

// MESH Details
#define MESH_PREFIX "whateveryoulike"  // Nombre para tu MESH
#define MESH_PASSWORD "somethingSneak"    // Contraseña para tu MESH
#define MESH_PORT 1515            // Puerto por defecto

#define DHTPIN 4       // Pin digital conectado al sensor DHT
#define DHTTYPE DHT11  // Tipo de sensor DHT

DHT dht(DHTPIN, DHTTYPE);

int nodeNumber = 2;  // Número de este nodo
String readings;     // String para almacenar las lecturas

Scheduler userScheduler;  // Para manejar las tareas
painlessMesh mesh;

// Prototipos de funciones
void sendMessage();
String getReadings();
void goToSleep();  // Función para entrar en deep sleep

// Crear tareas para enviar mensajes y obtener lecturas
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);  

// Función para obtener lecturas del sensor DHT
/* Esta función recoge los datos del sensor DHT11, específicamente la
   temperatura y la humedad. Luego, estos valores se almacenan en un 
   objeto JSON, junto con el número del nodo. El objeto JSON se convierte
   a una cadena (string) para ser enviada a otros nodos. Los valores también 
   se imprimen en el monitor serie para su monitoreo. */
String getReadings() {
  JSONVar jsonReadings;

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature);
  Serial.println(F("°C "));

  jsonReadings["node"] = nodeNumber;
  jsonReadings["temp"] = temperature;
  jsonReadings["hum"] = humidity;
  
  readings = JSON.stringify(jsonReadings);
  return readings;
}

// Función para enviar el mensaje a la red mesh
/* Esta función obtiene las lecturas del sensor DHT mediante la función
   getReadings() y envía estos datos a todos los nodos en la red mesh.
   También imprime el mensaje en el monitor serie para su verificación.
   Se puede implementar un modo de "deep sleep" después de enviar el mensaje,
   aunque actualmente está comentado. */
void sendMessage() {
  String msg = getReadings();
  Serial.println(msg);
  mesh.sendBroadcast(msg);
  
  // Después de enviar el mensaje, entra en deep sleep
  //goToSleep();
}

// Función para entrar en deep sleep
void goToSleep() {
  Serial.println("Entrando en Deep Sleep durante 10 segundos...");
  
  // Configurar el temporizador para despertar después de 10 segundos
  esp_sleep_enable_timer_wakeup(10 * 1000000);  // 10 segundos (en microsegundos)
  
  // Poner al ESP32 en modo deep sleep
  esp_deep_sleep_start();
}

// Callbacks necesarios para la librería painlessMesh
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup() {
  Serial.begin(115200);
  
  // Iniciar el sensor DHT
  dht.begin();

  // Configurar la malla mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  // Agregar la tarea de enviar mensaje
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();  // Habilitar la tarea para que se ejecute una vez
}

void loop() {
  // Actualizar la malla
  mesh.update();
}