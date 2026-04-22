// Blaulink, codi que te el boto que va amb bateria
/* BlauLink projecte
 * https://github.com/CasamaMaker/BlauLink
 */


#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>

#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"

#include <esp_sleep.h>
#include <EEPROM.h>
#include <esp_now.h>
#include <FastLED.h>
#include "blauprotocol.h"
#include "blauprotocol_link.h"
#include "blauprotocol_trg.h"



// #define BLAULINK_V1
#define BLAULINK_V2
// #define PICO_CLICK


#if defined(BLAULINK_V1)
  #define enVBatterySense 4
  #define VbatSense 3
  #define Boto 5
  #define enBoto 99  // 99 per indicar no disponible o mode deepsleep
  #define digitalLed 6

#elif defined(BLAULINK_V2)
  #define enVBatterySense 0
  #define VbatSense 3
  #define Boto 1
  #define enBoto 4
  #define digitalLed 5

#elif defined(PICO_CLICK)
  #define enVBatterySense 99  // No implementat
  #define VbatSense 4
  #define Boto 5
  #define enBoto 3
  #define digitalLed 6

#else
  #error "Defineix una versió del dispositiu (BLAULINK_V1, BLAULINK_V2 o PICO_CLICK)"
#endif

#define idioma  "CAT"      // CAT:català (per defecte), EN:english

const char* ssid = "BlauLink"; //Name of the WIFI network hosted by the device
const char* password =  "";               //Password

AsyncWebServer server(80);                //This creates a web server, required in order to host a page for connected devices

DNSServer dnsServer;                      //This creates a DNS server, required for the captive portal

const char* PARAM_INPUT_1 = "mac";  // Search for parameter in HTTP POST request
String strMac;  
char macStr[18];  
byte receiverMac[6];                   //Variables to save values from HTML form
const char* macPath = "/mac.txt"; // File paths to save input values permanently

String myAddresss, myAddresssDoted, myAddresssEnd;

//****************** DIGITAL LED ******************************
// #include <FastLED.h>
#define NUM_LEDS 1
#define DATA_PIN digitalLed //6
#define BRIGHTNESS  15
CRGB leds[NUM_LEDS];

unsigned long startTime; // Variable per emmagatzemar el temps d'inici

#define MAX_NETWORKS 5  // Definir un límit per al nombre de xarxes que podem guardar

// Definir un array global per emmagatzemar les adreces MAC
String macAddresses[MAX_NETWORKS];  // Array per emmagatzemar les adreces MAC de les xarxes trobades


// BlauProtocol: comptador de seqüència (circular 0–255)
// static uint8_t blau_seq = 0;
static uint8_t          blau_seq          = 0;
volatile bool           blau_ack_received = false; // escrit des del callback ESP-NOW
volatile uint8_t        blau_ack_seq      = 0;     // seq confirmat per l'ACK rebut
volatile uint8_t        blau_ack_result   = 0;     // codi ACK_* rebut (camp p1 de l'ACK)


// flag llegit pel task principal — mai tocar FastLED des d'aquí
static volatile bool _mac_delivery_ok = false;

#include "esp_adc_cal.h"

esp_adc_cal_characteristics_t adc_chars;

float batteryVoltage;
int batteryLevel;
bool isCharging;

bool isMacValid(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) return true;
  }
  return false;
}

String macToString(const uint8_t *mac) {
  char buf[18];  // Buffer estàtic per mantenir la cadena després de sortir de la funció
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  String s(buf);
  s.toUpperCase();   // Això modifica 's' i no retorna res
  return s;
}

void deleteEeprom(){
  // Delete 6 bytes of eeprom
  memset(receiverMac, 0xFF, sizeof(receiverMac));
  EEPROM.put(0, receiverMac);
  EEPROM.commit();
}

void readEeprom(){
  EEPROM.get(0, receiverMac);   // Get MAC address saved in eeprom
  // Serial.println("read");
  // strMac = macToString(receiverMac);

  if (isMacValid(receiverMac)) {
    strMac = macToString(receiverMac);
  } else {
    Serial.println("No hi ha cap MAC guardada");
    strMac = "FF:FF:FF:FF:FF:FF"; // O posa una cadena indicativa
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  _mac_delivery_ok = (status == ESP_NOW_SEND_SUCCESS);
  Serial.println(_mac_delivery_ok ? "MAC ACK: OK" : "MAC ACK: FAIL");
  // ← res més: cap FastLED, cap delay, cap Serial llarg
}

void printPacket(const BlauPacket_t* pkt) {
  Serial.println("---- PACKET ----");
  Serial.printf("version: 0x%02X\n", pkt->version);
  Serial.printf("type:    0x%02X\n", pkt->type);
  Serial.printf("seq:     %d\n", pkt->seq);
  Serial.printf("cmd:     0x%02X\n", pkt->cmd);
  Serial.printf("p1:      0x%02X\n", pkt->p1);
  Serial.printf("p2:      0x%02X\n", pkt->p2);
  Serial.printf("p3:      0x%02X\n", pkt->p3);
  Serial.printf("src_id:  0x%04X\n", pkt->src_id);
  Serial.printf("crc:     0x%02X\n", pkt->crc8);
  Serial.println("----------------");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  Serial.print("<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>");
  Serial.print("len: ");
  Serial.println(len);

  for (int i = 0; i < len; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();

  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }


  BlauPacket_t pkt;
  if (!blau_parse_packet(data, len, &pkt)) {
    Serial.print("blau_parse_packet FAIL, len="); Serial.println(len);
    return;
  }
  printPacket(&pkt);   // ara sí: sobre dades vàlides

  if (pkt.type == TYPE_ACK) {
    // Per protocol: cmd=seq confirmat, p1=codi ACK_*
    blau_ack_seq      = pkt.cmd;          // blau_is_ack_for() compara pkt.cmd
    blau_ack_result   = pkt.p1;           // blau_ack_status() retorna pkt.p1
    blau_ack_received = true;
    Serial.print("ACK rebut! confirmed_seq="); Serial.print(pkt.cmd);
    Serial.print(" status=0x"); Serial.println(pkt.p1, HEX);
  } else if (pkt.type == TYPE_PONG) {
    Serial.print("PONG rebut, seq="); Serial.println(pkt.seq);
  } else if (pkt.type == TYPE_STATUS_RSP) {
    Serial.print("STATUS_RSP: on="); Serial.print(pkt.p1);
    Serial.print(" bri="); Serial.print(pkt.p2);
    Serial.print(" type="); Serial.println(pkt.p3);
  } else {
    Serial.print("Paquet rebut, type=0x"); Serial.println(pkt.type, HEX);
  }
}

void config_ESPNOW(){
  
  WiFi.mode(WIFI_STA);
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }


  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  // Registrar el dispositivo receptor
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6); //broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Añadir el receptor
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

bool sendWithAck(BlauPacket_t *pkt) {
  for (int attempt = 0; attempt < BLAU_MAX_RETRIES; attempt++) {
    blau_ack_received = false;

    if (esp_now_send(receiverMac, (uint8_t*)pkt, sizeof(BlauPacket_t)) != ESP_OK) {
      Serial.print("esp_now_send error, intent="); Serial.println(attempt + 1);
      continue;
    }

    uint32_t t0 = millis();
    while (millis() - t0 < BLAU_ACK_TIMEOUT_MS) {
      if (blau_ack_received && blau_ack_seq == pkt->seq) {
        uint8_t st = blau_ack_result;   // blau_ack_status() equivalent (ja llegit)
        if (st == ACK_OK || st == ACK_DUPLICATE) {
          Serial.print("ACK acceptat (intent "); Serial.print(attempt + 1);
          Serial.print(") status=0x"); Serial.println(st, HEX);
          return true;
        }
        // ACK_ERROR o altres → reintent
        Serial.print("ACK rebutjat, status=0x"); Serial.println(st, HEX);
        break;   // surt del while, passa al següent intent
      }
      delay(1);  // ceda CPU però no bloqueja 50 ms sencers
    }
    Serial.print("Timeout/fail sense ACK acceptat, intent="); Serial.println(attempt + 1);
  }
  return false;
}

void send_ping() {
  BlauPacket_t pkt;
  blau_seq = millis() & 0xFF;
  blau_build_ping_packet(&pkt, blau_seq);

  blau_ack_received = false;
  esp_err_t r = esp_now_send(receiverMac, (uint8_t*)&pkt, sizeof(pkt));
  Serial.print("PING enviat seq="); Serial.print(pkt.seq);
  Serial.print(" err=0x"); Serial.println(r, HEX);
  // La resposta (PONG) es processarà a OnDataRecv — no bloqueja
}

void send_ESPNOW(){
  BlauPacket_t pkt;
  blau_seq = millis() & 0xFF;
  blau_build_event_packet(&pkt, blau_seq, EVT_CLICK_1);

  bool ok = sendWithAck(&pkt);
  blau_seq = millis() & 0xFF;

  // LED al task principal: segur, sense interferir amb WiFi task
  leds[0] = ok ? CRGB::Green : CRGB::Red;
  FastLED.show();

  if (!ok) Serial.println("WARN: sense ACK després de tots els intents");
}

String* scanNetworks() {
  Serial.println("scanning networks");
  int n = WiFi.scanNetworks();  // Escaneja les xarxes Wi-Fi disponibles
  int count = 0;  // Comptador per emmagatzemar les adreces MAC trobades

  // Reinicialitza el contingut
  for (int i = 0; i < MAX_NETWORKS; i++) {
    macAddresses[i] = "";
  }

  if (n == 0) {
    Serial.println("No s'han trobat xarxes.");
  } else {
    for (int i = 0; i < n && count < MAX_NETWORKS; i++) {
      // Emmagatzema l'adreça MAC de la xarxa al nostre array
      macAddresses[count] = WiFi.BSSIDstr(i)+" >> "+ WiFi.SSID(i);  // Guarda l'adreça MAC
      count++;
    }
  }

  // Retorna l'array amb les adreces MAC trobades
  // Serial.printf(*macAddresses);
  return macAddresses;
}

float getBatteryVoltage() {
  // const int enBatterySensePin = 0;
  pinMode(enVBatterySense, OUTPUT);
  digitalWrite(enVBatterySense, LOW);  
  
  // const int batteryPin = 3;  // GPIO que llegeix la bateria
  const float voltageDividerRatio = 2.0;  // divisor resistiu 1:1
  static esp_adc_cal_characteristics_t adc_chars;  // característiques de calibració

  // Calibració de l'ADC (només cal fer-ho un cop, però no és greu repetir)
  esp_adc_cal_characterize(
      ADC_UNIT_1,
      ADC_ATTEN_DB_12,  // Fins a ~2.2V — ideal per llegir fins a 2.1V
      ADC_WIDTH_BIT_12,
      ESP_ADC_CAL_VAL_EFUSE_VREF,  // Usa la Vref gravada al xip si està disponible
      &adc_chars
  );

  // // Llegir el valor cru
  // uint32_t raw = analogRead(batteryPin);

  // // Convertir a mil·liVolts tenint en compte la calibració
  // uint32_t voltage_mV = esp_adc_cal_raw_to_voltage(raw, &adc_chars);


  uint32_t sum_mV = 0;
  for (int i = 0; i < 10; i++) {
    uint32_t raw = analogRead(VbatSense);
    sum_mV += esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    delay(2);  // petita pausa per estabilitzar lectura
  }
  uint32_t voltage_mV = sum_mV / 10;

  // Serial.println(voltage_mV);

  // Tornar la tensió real de la bateria
  // Serial.println (voltage_mV * voltageDividerRatio/ 1000.0) ;  // volts
  return (voltage_mV * voltageDividerRatio) / 1000.0;  // volts
}

// Función para calcular el porcentaje de batería
int calculateBatteryPercentage(float voltage) {
    // Valores típicos para batería LiPo
    const float minVoltage = 3.2; // Voltaje mínimo de la batería
    const float maxVoltage = 4.2; // Voltaje máximo de la batería
    
    if (voltage >= maxVoltage) return 100;
    if (voltage <= minVoltage) return 0;
    
    int percentage = ((voltage - minVoltage) / (maxVoltage - minVoltage)) * 100;
    return constrain(percentage, 0, 100);
}

// Función para detectar si está cargando
bool isDeviceCharging() {
    // Esto depende de tu circuito de carga
    // Podrías usar un pin digital para detectar el estado de carga
    const int chargingPin = 2; // Pin conectado al indicador de carga
    
    // Si tienes un pin que se pone HIGH cuando está cargando
    return digitalRead(chargingPin) == HIGH;
    
    // Alternativa: detectar por incremento de voltaje
    // (requiere mediciones consecutivas)
}

void serveixWifiManager(AsyncWebServerRequest *request) {
  String path = "/wifimanager_" + String(idioma) + ".html";
  request->send(LittleFS, path, "text/html");
}

void webServerSetup(){
  // accedeix aquí just conectar-se a la wifi des de l'ordinador
  server.on("/", HTTP_GET, serveixWifiManager);
  // Required
	server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	// windows 11 captive portal workaround
	server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });

  // accedeix aquí just conectar-se a la wifi des del mobil android
  server.on("/generate_204", HTTP_GET, serveixWifiManager);

// .  server.on("/connecttest.txt", HTTP_GET, serveixWifiManager);
  server.on("/ncsi.txt", HTTP_GET, serveixWifiManager);
  
  // Rutas adicionales para engañar a Windows
  server.on("/hotspot-detect.html", HTTP_GET, serveixWifiManager);
  server.on("/library/test/success.html", HTTP_GET, serveixWifiManager);
  server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox captive portal call home
  server.on("/redirect", HTTP_GET, serveixWifiManager);
  server.on("/fwlink", HTTP_GET, serveixWifiManager);
  server.on("/cdn-cgi/", HTTP_GET, serveixWifiManager);
  server.on("/canonical.html", HTTP_GET, serveixWifiManager);

  // return 404 to webpage icon
	server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });	// webpage icon

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
    Serial.println("Served CSS");
  });

  server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", strMac); //String(mac).c_str());
      Serial.println(strMac);
  });

  server.on("/deletemac", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "macDeleted"); //String(mac).c_str());
      deleteEeprom();
      Serial.println("Mac esborrada");
      readEeprom();
  });

  // Ruta per obtenir la llista d'adreces MAC
  server.on("/macList", HTTP_GET, [](AsyncWebServerRequest *request) {
    String macListStr = "";  // Crear una cadena per a les adreces MAC
    String* macList = scanNetworks();  // Crida a la funció per obtenir les adreces MAC
    // Serial.println(*macList);

    Serial.println("Llista de xarxes trobades:");
    for (int i = 0; i < MAX_NETWORKS; i++) {
      if (macList[i] != "") {  // Només afegir adreces MAC no buides
        macListStr += macList[i];  // Afegir l'adreça MAC a la cadena
        macListStr += "\n";  // Afegir un salt de línia entre cada MAC

        // Serial.println(macList[i]);
      }
    }
    // Retornar la cadena amb totes les adreces MAC
    request->send(200, "text/plain", macListStr);
    Serial.println(macListStr);

  });

  server.on("/mymac", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", myAddresssDoted); //String(mac).c_str());
    Serial.println(myAddresssDoted);
  });

  server.on("/battery", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Obtener el nivel de batería (esto depende de tu configuración de hardware)
    // float batteryVoltage = 3.8;//getBatteryVoltage(); // Función que debes implementar
    // int batteryLevel = 100;//calculateBatteryPercentage(batteryVoltage);
    // bool isCharging = true;//isDeviceCharging(); // Función que debes implementar
    
    // Crear JSON response
    String jsonResponse = "{";
    jsonResponse += "\"level\":" + String(batteryLevel) + ",";
    jsonResponse += "\"charging\":" + String(isCharging ? "true" : "false");
    jsonResponse += "}";
    
    request->send(200, "application/json", jsonResponse);
    Serial.println("Battery info sent: " + String(batteryLevel) + "% - Charging: " + String(isCharging));

    // batteryVoltage = getBatteryVoltage(); // Función que debes implementar
    // batteryLevel = calculateBatteryPercentage(batteryVoltage);
    // isCharging = false;//isDeviceCharging(); // Función que debes implementar

    // Serial.println(batteryVoltage);
    // Serial.println(batteryLevel);
  });

  server.on("/disconnect-ap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Disconnecting WiFi AP...");
    
    delay(1000);
    // esp_deep_sleep_start();
    // digitalWrite(enBoto, LOW);
    if(enBoto!=99){
      digitalWrite(enBoto, LOW);
    }else{
      esp_deep_sleep_start();
    }
    // // Apagar WiFi abans de dormir
    // dnsServer.stop();
    // webServer.end();
    // WiFi.softAPdisconnect(true);
    // WiFi.mode(WIFI_OFF);
    // btStop();
    // esp_wifi_stop();
  });

  // reb les variables des de la web
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0;i<params;i++){
      const AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        if (p->name() == PARAM_INPUT_1) {
          strMac = p->value().c_str();
          Serial.print("Nova adreça MAC: -");
          Serial.print(strMac);
          Serial.println("-");
          //EEPROM.write(0, String(mac));
          strMac.replace(":", "");
          if(strMac == ""){
            Serial.println("no introduida cap mac, no guardar");
          }else{
            for (int i = 0; i < 6; i++) {
              String byteString = strMac.substring(i * 2, i * 2 + 2); // Obtenim cada parell de dígits
              receiverMac[i] = strtol(byteString.c_str(), NULL, 16); // Convertim el parell a byte
              EEPROM.write(i, receiverMac[i]);
            }
            EEPROM.commit();
          }
        }
      }
    }
    
    request->send(200, "text/plain", "Configurat! Ja pots prova");
    delay(1000);
    // esp_deep_sleep_start();
    // digitalWrite(enBoto, LOW);
    if(enBoto!=99){
      digitalWrite(enBoto, LOW);
    }else{
      esp_deep_sleep_start();
    }
    //ESP.restart();
  });


  // accedeix aquí quan busques qualsevol web al navegador
  server.onNotFound(serveixWifiManager);
  // // IMPORTANTE: Configurar el manejador para solicitudes no encontradas
  // server.onNotFound([](AsyncWebServerRequest *request){
  //   // Esto es crucial para el portal cautivo
  //   request->redirect("http://" + WiFi.softAPIP().toString());
  // });

  server.begin();                         //Starts the server process
  Serial.println("Web server started");
}

void getMyMacAddress() {
  // myAddresssDoted = WiFi.macAddress();  //retorna la MAC de la interfície WiFi STA (station)
  myAddresssDoted = WiFi.softAPmacAddress();    //retorna la MAC de la interfície WiFi AP (access point)
  // Serial.print("MAC del microcontrolador: ");

  myAddresss = myAddresssDoted;
  myAddresss.replace(":", "");
  Serial.print("La meva adreça MAC (sta)"); Serial.println(myAddresssDoted);
  myAddresssEnd = myAddresss.substring(myAddresss.length() - 4);
}

void wifiApModeServer(){
  WiFi.mode(WIFI_STA);    // Iniciat wifi en station mode per poder llegir la MAC
  
  getMyMacAddress();
  String fullSSID = String(ssid) + "_" + myAddresssEnd;     // Concatenar el nom base ssid amb els últims 4 digits de l'adreça MAC
  WiFi.softAP(fullSSID.c_str(), password);                  //This starts the WIFI radio in access point mode
  Serial.println("Wifi initialized");
  Serial.println(WiFi.softAPIP());                          //Print out the IP address
  
  dnsServer.start(53, "*", WiFi.softAPIP());                //This starts the DNS server.  The "*" sends any request for port 53 straight to the IP address of the device
  
  webServerSetup();                                         //Configures the behavior of the web server
  Serial.println("Setup complete");
}

void setup() {
  startTime = millis();     // Set starting time variable 
  
  // Init. pinout
  pinMode(Boto, INPUT);
  if (enBoto != 99) pinMode(enBoto, OUTPUT), digitalWrite(enBoto, HIGH);
  
  Serial.begin(115200);     // Init. serial port
  // delay(2000);
  
  EEPROM.begin(6);        // Init. eeprom memory (or 512)
  

  if (!LittleFS.begin()) return Serial.println("Error muntant LittleFS"), void();   // Init. file system

  readEeprom();

  batteryVoltage = getBatteryVoltage(); // Función que debes implementar
  batteryLevel = calculateBatteryPercentage(batteryVoltage);
  isCharging = false;//isDeviceCharging(); // Función que debes implementar

  // Serial.println("holaaaaaaaaaaaa");
  Serial.println(batteryVoltage);
  Serial.println(batteryLevel);

  // Init. digital led
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  leds[0] = CRGB::Black;
  FastLED.show();


  if(isMacValid(receiverMac)){  //strMac != "FF:FF:FF:FF:FF:FF"){
    config_ESPNOW();

    // Send message via ESP-NOW
    send_ESPNOW();

  }else{
    Serial.println("No hi ha cap MAC del slave guardada");
    leds[0] = CRGB::Yellow;
    FastLED.show();
  }

  delay(10);
}

void loop() {
  if(digitalRead(Boto)){
    leds[0] = CRGB::Black;
    FastLED.show();
    delay(100);
  }else{
    if(enBoto!=99){
      digitalWrite(enBoto, LOW);
    }else{
      esp_deep_sleep_start();
    }
  }

  if(startTime + 3000 < millis()){

    Serial.print("MAC guardada:  "); Serial.println(strMac);

    leds[0] = CRGB::Blue;
    FastLED.show();

    wifiApModeServer();

    bool buttonStateLow1=false;
    bool buttonStateHigh2=false;
    bool buttonReleased = false;
    while(1){
      dnsServer.processNextRequest();     //requisit dns constant
      

      if(startTime + 60000 < millis()){         // s'apaga l'equip després de 60 segons
        Serial.println("Temps excedit");
        delay(200);

        if(enBoto!=99){                         // s'apaga per enLDO o per deepsleep
          digitalWrite(enBoto, LOW);
        }else{
          esp_deep_sleep_start();
        }
      }



      // seqüència per detectar que el boto es deixa de presionar i es torna a presionar, per apagar l'equip
      static bool lastButtonState = HIGH;       // Estat anterior del botó
      bool buttonState = digitalRead(Boto);     // Llegeix el botó

      if (buttonState == LOW && lastButtonState == HIGH && !buttonReleased) {
          buttonReleased = true;                // Marquem que s'ha alliberat el botó
          Serial.println("Botó alliberat");
          delay(200);
      }

      if (buttonState == HIGH && lastButtonState == LOW && buttonReleased) {
          Serial.println("Botó premut després d'alliberar");
          buttonReleased = false;                 // Reiniciem per detectar una nova seqüència
          delay(200);

          if(enBoto!=99){                         // s'apaga per enLDO o per deepsleep
            digitalWrite(enBoto, LOW);
          }else{
            esp_deep_sleep_start();
          }
      }
      lastButtonState = buttonState;              // Guardem l'estat per a la següent iteració
    }
  }

}