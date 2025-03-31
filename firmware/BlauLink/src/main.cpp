/* ESP-32 Captive portal example
 * github.com/elliotmade/ESP32-Captive-Portal-Example
 * This isn't anything new, and doesn't do anything special
 * just an example I would have appreciated while I was searching for a solution
 */


#include <Arduino.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"
// #include <wifimanager.h>
#include "SPIFFS.h"
// #include "spiffs.h"
// #include <LittleFS.h>
#include <esp_sleep.h>
#include <EEPROM.h>
#include <esp_now.h>


#define enVBatterySense 0
#define Boto 1
#define enBoto 4
#define digitalLed 5



const char* ssid = "EspLink-AP"; //Name of the WIFI network hosted by the device
const char* password =  "";               //Password

AsyncWebServer server(80);                //This creates a web server, required in order to host a page for connected devices

DNSServer dnsServer;                      //This creates a DNS server, required for the captive portal

const char* PARAM_INPUT_1 = "mac";  // Search for parameter in HTTP POST request
String mac;    
byte receiverMac[6];                   //Variables to save values from HTML form
const char* macPath = "/mac.txt"; // File paths to save input values permanently


//****************** DIGITAL LED ******************************
#include <FastLED.h>
#define NUM_LEDS 1
#define DATA_PIN digitalLed //6
#define BRIGHTNESS  15
CRGB leds[NUM_LEDS];


unsigned long startTime; // Variable per emmagatzemar el temps d'inici

#define MAX_NETWORKS 20  // Definir un límit per al nombre de xarxes que podem guardar

// Definir un array global per emmagatzemar les adreces MAC
String macAddresses[MAX_NETWORKS];  // Array per emmagatzemar les adreces MAC de les xarxes trobades


// REPLACE WITH YOUR RECEIVER MAC Address
//uint8_t broadcastAddress[] = {0x10, 0x52, 0x1C, 0x88, 0x5D, 0xBD}; //llum //{0xA0, 0x76, 0x4E, 0x36, 0x0A, 0x89};  //PETIT //A0:76:4E:36:0A:89 // BIG  //64:E8:33:C7:62:50

// Structure example to send data
// Must match the receiver structure
typedef struct {
  bool estat;
} struct_message;

// Create a struct_message called missatge
struct_message missatge;

// esp_now_peer_info_t peerInfo;



// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  // Serial.println(status);
  if(status == ESP_NOW_SEND_SUCCESS){
    Serial.println("Delivery Success" + status);
    leds[0] = CRGB::Green;
    FastLED.show();
    // leds[0] = CRGB::Green;
    // FastLED.show();
  }else{
    Serial.println("Delivery Fail" + status);
    leds[0] = CRGB::Red;
    FastLED.show();
    // leds[0] = CRGB::Red;
    // FastLED.show();
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

void send_ESPNOW(){
  esp_err_t result = esp_now_send(receiverMac, (uint8_t *) &missatge, sizeof(missatge));  //broadcastAddress, (uint8_t *) &missatge, sizeof(missatge));
  if (result != ESP_OK) {
    Serial.println("Error al enviar el dato");
  }
}

// void scanNetworksss() {
//   Serial.println("Scanning for networks...");
//   int n = WiFi.scanNetworks();  // Obtenir les xarxes disponibles
//   if (n == 0) {
//     Serial.println("No networks found");
//   } else {
//     Serial.println("Networks found:");
//     for (int i = 0; i < n; i++) {
//       String ssidd = WiFi.SSID(i);  // Obtenir el nom de la xarxa
//       String macAddress = WiFi.BSSIDstr(i);  // Obtenir l'adreça MAC de la xarxa
//       Serial.print("SSID: ");
//       Serial.print(ssidd);
//       Serial.print(" | MAC Address: ");
//       Serial.println(macAddress);
//     }
//   }
// }

String* scanNetworks() {
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

void webServerSetup(){

  // scanNetworks();  // Cridem la funció per escanejar xarxes i obtenir les MACs
  

  // accedeix aquí just conectar-se a la wifi des de l'ordinador
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/wifimanager.html", "text/html");
    Serial.println("myweb /");
  });

  // accedeix aquí just conectar-se a la wifi des del mobil android
  //This is an example of triggering for a known location.  This one seems to be common for android devices
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(200, "text/plain", "You were sent here by a captive portal after requesting generate_204");
    request->send(SPIFFS, "/wifimanager.html", "text/html");
    Serial.println("requested /generate_204");
  });

  // accedeix aquí quan busques qualsevol web al navegador
  //This is an example of a redirect type response.  onNotFound acts as a catch-all for any request not defined above
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/wifimanager.html", "text/html"); //request->redirect("/");
    Serial.print("server.notfound triggered: ");
    Serial.println(request->url());       //This gives some insight into whatever was being requested
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
    Serial.println("Served CSS");
  });

  server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", mac); //String(mac).c_str());
      Serial.println(mac);
  });

  // Ruta per obtenir la llista d'adreces MAC
  server.on("/macList", HTTP_GET, [](AsyncWebServerRequest *request) {
    String macListStr = "";  // Crear una cadena per a les adreces MAC
    String* macList = scanNetworks();  // Crida a la funció per obtenir les adreces MAC
    Serial.println(*macList);
    for (int i = 0; i < MAX_NETWORKS; i++) {
      if (macList[i] != "") {  // Només afegir adreces MAC no buides
        macListStr += macList[i];  // Afegir l'adreça MAC a la cadena
        macListStr += "\n";  // Afegir un salt de línia entre cada MAC
      }
    }
    // Retornar la cadena amb totes les adreces MAC
    request->send(200, "text/plain", macListStr);
  });

  // reb les variables des de la web
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0;i<params;i++){
      const AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        if (p->name() == PARAM_INPUT_1) {
          mac = p->value().c_str();
          Serial.print("Nova adreça MAC: -");
          Serial.print(mac);
          Serial.println("-");
          //EEPROM.write(0, String(mac));
          mac.replace(":", "");
          if(mac == ""){
            Serial.println("no introduida cap mac, no guardar");
          }else{
            for (int i = 0; i < 6; i++) {
              String byteString = mac.substring(i * 2, i * 2 + 2); // Obtenim cada parell de dígits
              receiverMac[i] = strtol(byteString.c_str(), NULL, 16); // Convertim el parell a byte
              EEPROM.write(i, receiverMac[i]);
            }
            EEPROM.commit();
          }
          //writeFile(SPIFFS, macPath, mac.c_str());  // Write file to save value
          //writeFile(SPIFFS, ssidPath, ssid.c_str());  // Write file to save value
        }
        /*if (p->name() == PARAM_INPUT_2) {
          passs = p->value().c_str();
          Serial.print("Password set to: ");
          Serial.println(passs);
          //writeFile(SPIFFS, passPath, pass.c_str());  // Write file to save value
        }*/
      }
    }
    
    request->send(200, "text/plain", "Configurat! Ja pots prova");
    delay(1000);
    esp_deep_sleep_start();
    //ESP.restart();

  });
  server.begin();                         //Starts the server process
  Serial.println("Web server started");
}



void setup() {
  startTime = millis(); // Guarda el temps actual en mil·lisegons al iniciar
  Serial.begin(115200);
  // delay(1000);
  Serial.println("Aaaaaaaaaaaaaaaaaaaa");
  EEPROM.begin(512);
 //******************** SPIFFS ***********************
  // initSPIFFS();
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  //mac = EEPROM.read(0);

  for (int i = 0; i < 6; i++) {
    receiverMac[i] = EEPROM.read(i);
  }

  for (int i = 0; i < 6; i++) {
    if (i > 0) mac += ":"; // Afegim el separador : entre cada byte
    mac += String(receiverMac[i], HEX); // Convertim el byte a hexadecimal
  }

  /*for (int i = 0; i < 6; i++) {
      broadcastAddress[i] = receiverMac[i];
  }*/

  pinMode(Boto, INPUT);  // 5 boto
  pinMode(enBoto, OUTPUT);
  digitalWrite(enBoto, HIGH);

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  // leds[0] = CRGB::Blue;
  // FastLED.show();

  config_ESPNOW();
  // WiFi.mode(WIFI_STA);
  // // Init ESP-NOW
  // if (esp_now_init() != ESP_OK) {
  //   Serial.println("Error initializing ESP-NOW");
  //   return;
  // }

  // // Once ESPNow is successfully Init, we will register for Send CB to
  // // get the status of Trasnmitted packet
  // esp_now_register_send_cb(OnDataSent);
  
  // // Registrar el dispositivo receptor
  // esp_now_peer_info_t peerInfo = {};
  // memcpy(peerInfo.peer_addr, receiverMac, 6); //broadcastAddress, 6);
  // peerInfo.channel = 0;  
  // peerInfo.encrypt = false;
  
  // // Añadir el receptor
  // if (esp_now_add_peer(&peerInfo) != ESP_OK){
  //   Serial.println("Failed to add peer");
  //   return;
  // }

  // Set values to send
  // strcpy(missatge.a, "THIS IS A CHAR");
  // missatge.b = random(1,20);
  // missatge.c = 1.2;
  missatge.estat = false;
  
  // Send message via ESP-NOW
  send_ESPNOW();
  // esp_err_t result = esp_now_send(receiverMac, (uint8_t *) &missatge, sizeof(missatge));  //broadcastAddress, (uint8_t *) &missatge, sizeof(missatge));
  // if (result != ESP_OK) {
  //   Serial.println("Error al enviar el dato");
  // }
  delay(100);
}


void loop() {
  //dnsServer.processNextRequest();         //Without this, the connected device will simply timeout trying to reach the internet
                                          //or it might fall back to mobile data if it has it
  if(digitalRead(Boto)){
    leds[0] = CRGB::Green;
    FastLED.show();
    // leds[0] = CRGB::Blue;
    // FastLED.show();
    delay(100);
    leds[0] = CRGB::Black;
    FastLED.show();
    delay(100);
  
  }else{
    delay(50);
    // esp_deep_sleep_start();
    digitalWrite(enBoto, LOW);
  }


  if(startTime + 3000 < millis()){
    //mac = EEPROM.read(0);//readFile(SPIFFS, macPath);
    Serial.print("MAC guardada:  "); Serial.println(mac);

    // leds[0] = CRGB::Red;
    // FastLED.show();
    leds[0] = CRGB::Red;
    FastLED.show();
    WiFi.softAP(ssid, password);            //This starts the WIFI radio in access point mode
    Serial.println("Wifi initialized");
    Serial.println(WiFi.softAPIP());        //Print out the IP address on the serial port (this is where you should end up if the captive portal works)
    dnsServer.start(53, "*", WiFi.softAPIP());  //This starts the DNS server.  The "*" sends any request for port 53 straight to the IP address of the device
    webServerSetup();                       //Configures the behavior of the web server
    Serial.println("Setup complete");

    bool buttonStateLow1=false;
    bool buttonStateHigh2=false;
    bool buttonReleased = false;
    while(1){
      dnsServer.processNextRequest();
      if(startTime + 60000 < millis()){
        // esp_deep_sleep_start();
        digitalWrite(enBoto, LOW);
      }

      // detecta si s'ha tornat a presionar el boto, per aleshores apagar
      // if(!digitalRead(5) || buttonStateLow1){
      //   buttonStateLow1=true;
      //   if(digitalRead(5) || buttonStateHigh2){
      //     buttonStateHigh2 = true;
      //     if(!digitalRead(5)){
      //       esp_deep_sleep_start();
      //     }
      //   }
      // }

      static bool lastButtonState = HIGH;  // Estat anterior del botó

      bool buttonState = digitalRead(Boto);  // Llegeix el botó

      if (buttonState == LOW && lastButtonState == HIGH && !buttonReleased) {
          buttonReleased = true;  // Marquem que s'ha alliberat el botó
          Serial.println("Botó alliberat");
          delay(200);
      }

      if (buttonState == HIGH && lastButtonState == LOW && buttonReleased) {
          Serial.println("Botó premut després d'alliberar");
          buttonReleased = false;  // Reiniciem per detectar una nova seqüència
          delay(200);
          digitalWrite(enBoto, LOW);
      }
      lastButtonState = buttonState;  // Guardem l'estat per a la següent iteració

    }
  }

}