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

                              //  V1  | V2  | Pico-Click
#define enVBatterySense 0     //  4   | 0   | -   [no implementat encara]
#define VbatSense 3           //  3   | 3   | 4   [no implementat encara]
#define Boto 1                //  5   | 1   | 5
#define enBoto 4              //  -   | 4   | 3   [-: deepsleep mode (variable=99), n: pin mode]
#define digitalLed 5          //  6   | 5   | 6

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

#define MAX_NETWORKS 20  // Definir un límit per al nombre de xarxes que podem guardar

// Definir un array global per emmagatzemar les adreces MAC
String macAddresses[MAX_NETWORKS];  // Array per emmagatzemar les adreces MAC de les xarxes trobades

// #define CRYPTO_KEY "PASSWORD12345678"//"PASSWORD1"  // La mateixa clau de xifratge que en el dispositiu broker


// Structure data to send
typedef struct {
  // bool estat;
  char topic[50];
  char payload[50];
} struct_message;

// Create a struct_message called missatge
struct_message missatge;


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
  Serial.print("----->");
  strMac = macToString(receiverMac);

  Serial.println(strMac);
  Serial.print(missatge.topic);
  Serial.println(missatge.payload);

  esp_err_t result = esp_now_send(receiverMac, (uint8_t *) &missatge, sizeof(missatge));  //broadcastAddress, (uint8_t *) &missatge, sizeof(missatge));
  
  // char macStr[18];
  // snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
  //          receiverMac[0], receiverMac[1], receiverMac[2],
  //          receiverMac[3], receiverMac[4], receiverMac[5]);

  // Serial.print("--");
  // Serial.println(macStr);

  if (result != ESP_OK) {
    Serial.println("Error al enviar el dato");
  }else{
    Serial.println("Enviament correcte");
  }
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


void setup() {
  startTime = millis();     // Set starting time variable 
  
  Serial.begin(115200);     // Init. serial port
  
  EEPROM.begin(6);        // Init. eeprom memory (or 512)

  if (!LittleFS.begin()) return Serial.println("Error muntant LittleFS"), void();   // Init. file system

  readEeprom();

  // delay(1000);
  // Serial.println(strMac);




  // Init. pinout
  pinMode(Boto, INPUT);
  if (enBoto != 99) digitalWrite(enBoto, HIGH), pinMode(enBoto, OUTPUT);

  // Init. digital led
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  leds[0] = CRGB::Black;
  FastLED.show();


  if(isMacValid(receiverMac)){  //strMac != "FF:FF:FF:FF:FF:FF"){

    config_ESPNOW();

    // set variables to sent
    strcpy(missatge.topic, "llum");
    strcpy(missatge.payload, "conmuta");
    
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

    WiFi.mode(WIFI_STA);
    // Concatenar el nom base amb l'adreça MAC
    getMyMacAddress();
    String fullSSID = String(ssid) + "_" + myAddresssEnd;
    WiFi.softAP(fullSSID.c_str(), password);            //This starts the WIFI radio in access point mode
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
        
        Serial.println("Temps excedit");
        delay(200);

        if(enBoto!=99){
          digitalWrite(enBoto, LOW);
        }else{
          esp_deep_sleep_start();
        }
      }

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

          if(enBoto!=99){
            digitalWrite(enBoto, LOW);
          }else{
            esp_deep_sleep_start();
          }
      }
      lastButtonState = buttonState;  // Guardem l'estat per a la següent iteració
    }
  }

}