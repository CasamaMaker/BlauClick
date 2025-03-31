/* ESP-32 Captive portal example
 * github.com/elliotmade/ESP32-Captive-Portal-Example
 * This isn't anything new, and doesn't do anything special
 * just an example I would have appreciated while I was searching for a solution
 */


#include <Arduino.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "DNSServer.h"
#include <wifimanager.h>
// #include "SPIFFS.h"
// #include "spiffs.h"
#include <LittleFS.h>
#include <esp_sleep.h>
#include <EEPROM.h>
#include <esp_now.h>

const char* ssid = "EspLink-AP"; //Name of the WIFI network hosted by the device
const char* password =  "";               //Password

AsyncWebServer server(80);                //This creates a web server, required in order to host a page for connected devices

DNSServer dnsServer;                      //This creates a DNS server, required for the captive portal

const char* PARAM_INPUT_1 = "mac";  // Search for parameter in HTTP POST request
String mac;    
byte macAddress[6];                   //Variables to save values from HTML form
const char* macPath = "/mac.txt"; // File paths to save input values permanently


//****************** DIGITAL LED ******************************
#include <FastLED.h>
#define NUM_LEDS 1
#define DATA_PIN 6
#define BRIGHTNESS  15
CRGB leds[NUM_LEDS];


unsigned long startTime; // Variable per emmagatzemar el temps d'inici


// REPLACE WITH YOUR RECEIVER MAC Address
//uint8_t broadcastAddress[] = {0x10, 0x52, 0x1C, 0x88, 0x5D, 0xBD}; //llum //{0xA0, 0x76, 0x4E, 0x36, 0x0A, 0x89};  //PETIT //A0:76:4E:36:0A:89 // BIG  //64:E8:33:C7:62:50

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  char a[32];
  int b;
  float c;
  bool d;
} struct_message;

// Create a struct_message called myData
struct_message myData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  Serial.println(status);
  if(status==0){
    leds[0] = CRGB::Green;
    FastLED.show();
    leds[0] = CRGB::Green;
    FastLED.show();
  }else{
    leds[0] = CRGB::Red;
    FastLED.show();
    leds[0] = CRGB::Red;
    FastLED.show();
  }
}



void webServerSetup(){
  // accedeix aquí just conectar-se a la wifi des de l'ordinador
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/wifimanager.html", "text/html");
    Serial.println("myweb /");
  });

  // accedeix aquí just conectar-se a la wifi des del mobil android
  //This is an example of triggering for a known location.  This one seems to be common for android devices
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(200, "text/plain", "You were sent here by a captive portal after requesting generate_204");
    request->send(LittleFS, "/wifimanager.html", "text/html");
    Serial.println("requested /generate_204");
  });

  // accedeix aquí quan busques qualsevol web al navegador
  //This is an example of a redirect type response.  onNotFound acts as a catch-all for any request not defined above
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(LittleFS, "/wifimanager.html", "text/html"); //request->redirect("/");
    Serial.print("server.notfound triggered: ");
    Serial.println(request->url());       //This gives some insight into whatever was being requested
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
    Serial.println("Served CSS");
  });

  server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", mac); //String(mac).c_str());
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
              macAddress[i] = strtol(byteString.c_str(), NULL, 16); // Convertim el parell a byte
              EEPROM.write(i, macAddress[i]);
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
  EEPROM.begin(512);
 //******************** SPIFFS ***********************
  initSPIFFS();
  //mac = EEPROM.read(0);

  for (int i = 0; i < 6; i++) {
    macAddress[i] = EEPROM.read(i);
  }

  for (int i = 0; i < 6; i++) {
    if (i > 0) mac += ":"; // Afegim el separador : entre cada byte
    mac += String(macAddress[i], HEX); // Convertim el byte a hexadecimal
  }

  /*for (int i = 0; i < 6; i++) {
      broadcastAddress[i] = macAddress[i];
  }*/

  pinMode(5, INPUT);  // boto

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  leds[0] = CRGB::Blue;
  FastLED.show();

  WiFi.mode(WIFI_STA);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, macAddress, 6); //broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  // Set values to send
  strcpy(myData.a, "THIS IS A CHAR");
  myData.b = random(1,20);
  myData.c = 1.2;
  myData.d = false;
  
  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(macAddress, (uint8_t *) &myData, sizeof(myData));  //broadcastAddress, (uint8_t *) &myData, sizeof(myData));

}


void loop() {
  //dnsServer.processNextRequest();         //Without this, the connected device will simply timeout trying to reach the internet
                                          //or it might fall back to mobile data if it has it
  if(digitalRead(5)){
  leds[0] = CRGB::Blue;
  FastLED.show();
  leds[0] = CRGB::Blue;
  FastLED.show();
  delay(100);
  leds[0] = CRGB::Black;
  FastLED.show();
  delay(100);
  
  }else{
    delay(1000);
    esp_deep_sleep_start();
  }


  if(startTime + 3000 < millis()){
    //mac = EEPROM.read(0);//readFile(SPIFFS, macPath);
    Serial.print("MAC guardada:  "); Serial.println(mac);

    leds[0] = CRGB::Red;
    FastLED.show();
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
    while(1){
      dnsServer.processNextRequest();
      if(startTime + 60000 < millis()){
        esp_deep_sleep_start();
      }

      // detecta si s'ha tornat a presionar el boto, per aleshores apagar
      if(!digitalRead(5) || buttonStateLow1){
        buttonStateLow1=true;
        if(digitalRead(5) || buttonStateHigh2){
          buttonStateHigh2 = true;
          if(!digitalRead(5)){
            esp_deep_sleep_start();
          }
        }
      }

    }
  }

}