/************************* Inclusão das Bibliotecas *********************************/
#include <stdlib.h>
#include "ESP8266WiFi.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h" 
#include <ArduinoOTA.h>
#include <NTPClient.h>

/************************* Conexão WiFi*********************************/

#define WIFI_SSID       "Genguini's house" // nome de sua rede wifi
#define WIFI_PASS       "01042017"     // senha de sua rede wifi

/********************* Credenciais Adafruit io *************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "ulissesg2" // Seu usuario cadastrado na plataforma da Adafruit
#define AIO_KEY         ""       // Sua key da dashboard

/********************** Variaveis globais *******************************/

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -3 * 3600, 60000);

WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

int rele01 = 16; // pino do rele
int rele02 = 5;
int horaLigou = NULL;
int minutoLigou = NULL;
int tempoLigado = 0;

/****************************** Declaração dos Feeds ***************************************/

/* feed responsavel por receber os dados da nossa dashboard */
Adafruit_MQTT_Publish IOSub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/grass-io", MQTT_QOS_1);

Adafruit_MQTT_Subscribe IO = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/grass-io", MQTT_QOS_1);

Adafruit_MQTT_Publish OnTimeSub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/OnTimeGrass", MQTT_QOS_1);

Adafruit_MQTT_Subscribe OnTime = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/OnTimeGrass", MQTT_QOS_1);

/* Observe em ambas declarações acima a composição do tópico mqtt
  --> AIO_USERNAME "/feeds/mcp9808"
  O mpc9808 será justamente o nome que foi dado la na nossa dashboard, portanto o mesmo nome atribuido la, terá de ser colocado aqui tambem
*/

/*************************** Declaração dos Prototypes ************************************/

void initSerial();
void initPins();
void initWiFi();
void OTAInit();
void initMQTT();
void conectar_broker();

/*************************** Sketch ************************************/

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); //tirar
  initSerial();
  initPins();
  initWiFi();
  initMQTT();
  timeClient.begin();
}

void loop() {
  
  ArduinoOTA.handle();
  conectar_broker();    
  mqtt.processPackets(5000);

  timeClient.update();

  checkTimeIsUp();
  OnTimeSub.publish(tempoLigado);

  delay(5000);
  
}

/*************************** Implementação dos Prototypes ************************************/

/* Conexao Serial */
void initSerial() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Booting");
}

/* Configuração dos pinos */
void initPins() {

  pinMode(rele01, OUTPUT);
  digitalWrite(rele01, HIGH);

  pinMode(rele02, OUTPUT);
  digitalWrite(rele02, HIGH);

}

/* Configuração da conexão WiFi */
void initWiFi() {
  int tentativas;
  WiFi.mode(WIFI_STA);
  Serial.print("Conectando-se na rede "); Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  if (WiFi.status() != WL_NO_SSID_AVAIL && tentativas < 50){
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      tentativas ++;
    }
    
    Serial.println();
    OTAInit();
    Serial.println("Conectado à rede com sucesso"); Serial.println("Endereço IP: "); Serial.println(WiFi.localIP());
  }else{
      Serial.println("Não foi possivel conectar ao roteador");
      ESP.restart();
  }

  
}

/*init OTA connection*/

void OTAInit(){
  // Port defaults to 8266
//   ArduinoOTA.setPort(82662);

  // Hostname defaults to esp8266-[ChipID]
   ArduinoOTA.setHostname("ESP GRASS IRRIGATION");

  // No authentication by default
   ArduinoOTA.setPassword("01042017");

  // Password can be set with it's md5 value as well
//   MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
//   ArduinoOTA.setPasswordHash("01042017");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

/* Configuração da conexão MQTT */
void initMQTT() {
  IO.setCallback(io_callback);
  mqtt.subscribe(&IO);
  OnTime.setCallback(OnTime_callback);
  mqtt.subscribe(&OnTime);
}

/*************************** Implementação dos Callbacks ************************************/

/* callback responsavel por tratar o feed do rele */
void io_callback(char *data, uint16_t len) {
  String state = data;

  if (state == "ON") {
    digitalWrite(LED_BUILTIN, LOW); //tirar
    pumpOn();
    
  } else if(state == "OFF") {
    digitalWrite(LED_BUILTIN, HIGH);  //tirar
    pumpOff();
  }

}

void OnTime_callback (char *data, uint16_t len){
  String state = data;

  tempoLigado = atoi(state.c_str());
  
}


/*************************** Demais implementações ************************************/

/* Conexão com o broker e também servirá para reestabelecer a conexão caso caia */
void conectar_broker() {
  int8_t ret;

  if (mqtt.connected()) {
    return;
  }

  Serial.println("Conectando-se ao broker mqtt...");

  uint8_t num_tentativas = 3;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Falha ao se conectar. Tentando se reconectar em 5 segundos.");
    mqtt.disconnect();
    delay(5000);
    num_tentativas--;
    if (num_tentativas == 0) {
      Serial.println("Seu ESP será resetado.");
      while (1);
    }
  }

  Serial.println("Conectado ao broker com sucesso.");
}


void pumpOn(){
  digitalWrite(rele01, LOW);
  digitalWrite(rele02, LOW);
  saveTime();
}

void pumpOff(){
  digitalWrite(rele01, HIGH);
  digitalWrite(rele02, HIGH);
}

void saveTime(){

  horaLigou = timeClient.getHours();
  minutoLigou = timeClient.getMinutes();
}

void checkTimeIsUp(){
  int diferencaMinutos = 0;
  
  if (horaLigou != NULL && minutoLigou != NULL){
    
    if(horaLigou == timeClient.getHours()){
      diferencaMinutos = timeClient.getMinutes() - minutoLigou;
    }else if(timeClient.getHours() > horaLigou ){
      diferencaMinutos = (((timeClient.getHours() - horaLigou)*60) - minutoLigou) + timeClient.getMinutes();
    }
   if(diferencaMinutos >= tempoLigado){
      pumpOff();
      IOSub.publish("OFF");
    }
  }else{
    pumpOff();
    IOSub.publish("OFF");
  }
}
