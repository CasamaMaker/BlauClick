/* ESP-32 with ESP-NOW and Captive Portal
 * Basado en github.com/elliotmade/ESP32-Captive-Portal-Example
 * Mejoras para envío ESP-NOW, manejo de energía y compatibilidad con Windows
 */

 #include <Arduino.h>
 #include <AsyncTCP.h>
 #include "ESPAsyncWebServer.h"
 #include "DNSServer.h"
 #include <LittleFS.h>
 #include <EEPROM.h>
 #include <esp_now.h>
 #include <WiFi.h>
 #include <FastLED.h>
 
 // Definición de pines
 #define enVBatterySense 0     // Pin para habilitar la lectura de batería
 #define VbatSense 3           // Pin para leer nivel de batería
 #define BUTTON_PIN 1          // Pin para el botón
 #define BUTTON_EN_PIN 4       // Pin para habilitar el botón
 #define LED_PIN 5             // Pin para el LED digital
 
 // Configuración del idioma
 #define LANGUAGE "EN"         // CAT:català, EN:english
 
 // Configuración de WiFi
 const char* ssid = "BlauLink-AP";  // Nombre base de la red WiFi
 const char* password = "";         // Contraseña del AP (vacía para acceso abierto)
 
 // Configuración del LED
 #define NUM_LEDS 1
 #define BRIGHTNESS 15
 CRGB leds[NUM_LEDS];
 
 // Configuración ESP-NOW
 #define CRYPTO_KEY "PASSWORD12345678"  // Clave de cifrado para ESP-NOW
 #define MAX_NETWORKS 20                // Límite de redes para escanear
 
 // Servidores para portal cautivo
 AsyncWebServer server(80);
 DNSServer dnsServer;
 
 // Variables para la MAC
 const char* PARAM_INPUT_1 = "mac";
 String strMac;
 byte receiverMac[6];
 const char* macPath = "/mac.txt";
 String myAddress, myAddressDotted, myAddressEnd;
 
 // Array para almacenar direcciones MAC escaneadas
 String macAddresses[MAX_NETWORKS];
 
 // Estructura para enviar mensajes ESP-NOW
 typedef struct {
   char topic[50];
   char payload[50];
 } struct_message;
 
 struct_message message;
 
 // Variables de tiempo
 unsigned long startTime;
 unsigned long buttonPressTime = 0;
 bool buttonPressed = false;
 bool apActive = false;
 
 // Callback para confirmar envío ESP-NOW
 void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
   if(status == ESP_NOW_SEND_SUCCESS) {
     Serial.println("ESP-NOW: Envío exitoso");
     leds[0] = CRGB::Green;
     FastLED.show();
     delay(300);
     leds[0] = CRGB::Black;
     FastLED.show();
   } else {
     Serial.println("ESP-NOW: Fallo en envío");
     leds[0] = CRGB::Red;
     FastLED.show();
     delay(300);
     leds[0] = CRGB::Black;
     FastLED.show();
   }
 }
 
 // Configuración de ESP-NOW
 void configESPNOW() {
   WiFi.mode(WIFI_STA);
   
   if (esp_now_init() != ESP_OK) {
     Serial.println("Error al inicializar ESP-NOW");
     return;
   }
 
   // Establecer clave de cifrado
   esp_now_set_pmk((uint8_t *)CRYPTO_KEY);
   
   // Registrar callback para el estado de envío
   esp_now_register_send_cb(OnDataSent);
   
   // Registrar dispositivo receptor
   esp_now_peer_info_t peerInfo = {};
   memcpy(peerInfo.peer_addr, receiverMac, 6);
   peerInfo.channel = 0;  
   peerInfo.encrypt = false;
   
   if (esp_now_add_peer(&peerInfo) != ESP_OK) {
     Serial.println("Error al añadir peer");
     return;
   }
   
   Serial.println("ESP-NOW configurado correctamente");
 }
 
 // Enviar mensaje por ESP-NOW
 bool sendESPNOW(const char* topic, const char* payload) {
   // Copiar datos al mensaje
   strncpy(message.topic, topic, sizeof(message.topic));
   strncpy(message.payload, payload, sizeof(message.payload));
   
   // Enviar mensaje
   esp_err_t result = esp_now_send(receiverMac, (uint8_t *)&message, sizeof(message));
   
   // Debug info
   char macStr[18];
   snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            receiverMac[0], receiverMac[1], receiverMac[2],
            receiverMac[3], receiverMac[4], receiverMac[5]);
            
   Serial.print("Enviando a MAC: ");
   Serial.print(macStr);
   Serial.print(" - Tema: ");
   Serial.print(message.topic);
   Serial.print(" - Payload: ");
   Serial.println(message.payload);
 
   return (result == ESP_OK);
 }
 
 // Escanear redes WiFi para mostrar en portal de configuración
 String* scanNetworks() {
   int n = WiFi.scanNetworks();
   int count = 0;
 
   // Reinicializar array
   for (int i = 0; i < MAX_NETWORKS; i++) {
     macAddresses[i] = "";
   }
 
   if (n == 0) {
     Serial.println("No se encontraron redes WiFi");
   } else {
     Serial.printf("Se encontraron %d redes WiFi\n", n);
     for (int i = 0; i < n && count < MAX_NETWORKS; i++) {
       macAddresses[count] = WiFi.BSSIDstr(i) + " >> " + WiFi.SSID(i);
       count++;
     }
   }
 
   return macAddresses;
 }
 
 // Obtener la MAC del dispositivo
 void getMyMacAddress() {
   myAddressDotted = WiFi.macAddress();
   Serial.print("MAC del ESP32: ");
   Serial.println(myAddressDotted);
 
   myAddress = myAddressDotted;
   myAddress.replace(":", "");
   myAddressEnd = myAddress.substring(myAddress.length() - 4);
 }
 
 // Servir página web según idioma
 void serveWifiManager(AsyncWebServerRequest *request) {
   String path = "/wifimanager_" + String(LANGUAGE) + ".html";
   request->send(LittleFS, path, "text/html");
 }
 
 // Respuesta para los archivos de verificación de conectividad de Windows
 void serveWindowsConnectivity(AsyncWebServerRequest *request) {
   request->send(200, "text/plain", "Microsoft NCSI");
 }
 
 // Respuesta para archivo success.txt
 void serveSuccessText(AsyncWebServerRequest *request) {
   request->send(200, "text/plain", "success");
 }
 
 // Respuesta para iOS/macOS
 void serveAppleHotspotDetect(AsyncWebServerRequest *request) {
   request->send(200, "text/html", 
     "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
 }
 
 // Respuesta para 204 (Android)
 void serveNoContent(AsyncWebServerRequest *request) {
   AsyncWebServerResponse *response = request->beginResponse(204);
   response->addHeader("Content-Type", "text/plain");
   response->addHeader("Connection", "close");
   request->send(response);
 }
 
 // Configurar servidor web para portal cautivo mejorado
 void setupWebServer() {
   // Rutas principales
   server.on("/", HTTP_GET, serveWifiManager);
   
   // MEJORA: Respuestas específicas por plataforma para evitar desconexiones
   
   // Android captive portal detection
   server.on("/generate_204", HTTP_GET, serveNoContent);
   server.on("/gen_204", HTTP_GET, serveNoContent);
   
   // Windows connectivity checks
   server.on("/connecttest.txt", HTTP_GET, serveWindowsConnectivity);
   server.on("/ncsi.txt", HTTP_GET, serveWindowsConnectivity);
   
   // iOS/macOS detection
   server.on("/hotspot-detect.html", HTTP_GET, serveAppleHotspotDetect);
   server.on("/library/test/success.html", HTTP_GET, serveAppleHotspotDetect);
   
   // Archivos generales de verificación
   server.on("/success.txt", HTTP_GET, serveSuccessText);
   
   // Otras redirecciones comunes
   server.on("/redirect", HTTP_GET, serveWifiManager);
   server.on("/fwlink", HTTP_GET, serveWifiManager);
   server.on("/cdn-cgi/", HTTP_GET, serveWifiManager);
 
   // Archivo CSS
   server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
     request->send(LittleFS, "/style.css", "text/css");
     Serial.println("Sirviendo CSS");
   });
 
   // Rutas para obtener información
   server.on("/mac", HTTP_GET, [](AsyncWebServerRequest *request) {
     request->send(200, "text/plain", strMac);
   });
 
   // Listar MACs disponibles
   server.on("/macList", HTTP_GET, [](AsyncWebServerRequest *request) {
     String macListStr = "";
     String* macList = scanNetworks();
     for (int i = 0; i < MAX_NETWORKS; i++) {
       if (macList[i] != "") {
         macListStr += macList[i];
         macListStr += "\n";
       }
     }
     request->send(200, "text/plain", macListStr);
   });
 
   // Mostrar MAC del dispositivo
   server.on("/mymac", HTTP_GET, [](AsyncWebServerRequest *request) {
     request->send(200, "text/plain", myAddressDotted);
   });
 
   // Recibir datos del formulario
   server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
     int params = request->params();
     for (int i = 0; i < params; i++) {
       const AsyncWebParameter* p = request->getParam(i);
       if (p->isPost()) {
         if (p->name() == PARAM_INPUT_1) {
           strMac = p->value().c_str();
           Serial.print("Nueva MAC recibida: ");
           Serial.println(strMac);
           
           // Procesar y guardar MAC
           strMac.replace(":", "");
           if (strMac == "") {
             Serial.println("MAC vacía, no se guardará");
           } else {
             for (int i = 0; i < 6; i++) {
               String byteString = strMac.substring(i * 2, i * 2 + 2);
               receiverMac[i] = strtol(byteString.c_str(), NULL, 16);
               EEPROM.write(i, receiverMac[i]);
             }
             EEPROM.commit();
             
             // Reconstruir la MAC con formato para mostrar
             strMac = "";
             for (int i = 0; i < 6; i++) {
               if (i > 0) strMac += ":";
               char hexByte[3];
               sprintf(hexByte, "%02X", receiverMac[i]);
               strMac += String(hexByte);
             }
           }
         }
       }
     }
     
     request->send(200, "text/plain", "¡Configuración guardada! Reiniciando...");
     
     // Reiniciar después de 1 segundo para aplicar cambios
     delay(1000);
     ESP.restart();
   });
 
   // MEJORA: Handler para tipos MIME comunes que Windows puede solicitar
   server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
     request->send(200, "image/x-icon", "");
   });
 
   // MEJORA: El handler onNotFound ahora es más inteligente
   server.onNotFound([](AsyncWebServerRequest *request) {
     String host = request->host();
     
     // Si la petición es para un dominio externo, redirigir al portal
     if (host != WiFi.softAPIP().toString() && host != "captive.apple.com") {
       // Registramos la URL que intentan acceder para diagnóstico
       Serial.print("Redirigiendo: ");
       Serial.print(host);
       Serial.print(request->url());
       Serial.println(" → portal cautivo");
       
       // Redirigir a la raíz donde está nuestro portal
       request->redirect("/");
     } else {
       // Si es una solicitud directa a nuestra IP, servir el portal
       serveWifiManager(request);
     }
   });
 
   server.begin();
   Serial.println("Servidor web iniciado");
 }
 
 // Iniciar punto de acceso WiFi
 void startAP() {
   // MEJORA: Configurar DNS para el AP para mejorar captive portal
   IPAddress apIP(192, 168, 4, 1);
   IPAddress netMsk(255, 255, 255, 0);
   
   String fullSSID = String(ssid) + "_" + myAddressEnd;
   
   WiFi.mode(WIFI_AP);
   WiFi.softAPConfig(apIP, apIP, netMsk);
   WiFi.softAP(fullSSID.c_str(), password);
   
   Serial.print("Punto de acceso iniciado: ");
   Serial.println(fullSSID);
   Serial.print("IP: ");
   Serial.println(WiFi.softAPIP());
   
   // MEJORA: El DNS server ahora usa la dirección IP del AP
   dnsServer.setErrorReplyCode(DNSReplyCode::NoError); // MEJORA: Responder con NoError en lugar de NXDomain
   dnsServer.start(53, "*", apIP);
   
   setupWebServer();
   
   apActive = true;
   
   // Indicación visual
   leds[0] = CRGB::Purple;
   FastLED.show();
 }
 
 // Función para entrar en deep sleep
 void enterDeepSleep() {
   Serial.println("Entrando en deep sleep...");
   leds[0] = CRGB::Black;
   FastLED.show();
   delay(100);
   
   // Deshabilitar el pin del botón para ahorrar energía
   digitalWrite(BUTTON_EN_PIN, LOW);
   
   // Configurar wake-up por botón (cambiar GPIO según tu configuración)
 //  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, LOW);
   
   // Entrar en deep sleep
//    esp_deep_sleep_start();
 }
 
 void setup() {
   Serial.begin(115200);
   Serial.println("\n\nIniciando BlauLink ESP32...");
   
   // Inicializar EEPROM
   EEPROM.begin(512);
   
   // Inicializar sistema de archivos
   if (!LittleFS.begin()) {
     Serial.println("Error al montar LittleFS");
     return;
   }
   
   // Leer MAC guardada en EEPROM
   for (int i = 0; i < 6; i++) {
     receiverMac[i] = EEPROM.read(i);
   }
   
   // Reconstruir string de MAC
   strMac = "";
   for (int i = 0; i < 6; i++) {
     if (i > 0) strMac += ":";
     char hexByte[3];
     sprintf(hexByte, "%02X", receiverMac[i]);
     strMac += String(hexByte);
   }
   
   Serial.print("MAC guardada: ");
   Serial.println(strMac);
   
   // Configurar pines
   pinMode(BUTTON_PIN, INPUT);
   pinMode(BUTTON_EN_PIN, OUTPUT);
   digitalWrite(BUTTON_EN_PIN, HIGH);  // Habilitar circuito del botón
   
   // Inicializar LED
   FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
   FastLED.setBrightness(BRIGHTNESS);
   leds[0] = CRGB::Black;
   FastLED.show();
   
   // Obtener MAC propia
   getMyMacAddress();
   
   // Configurar ESP-NOW
   configESPNOW();
   
   // Enviar mensaje inicial
   sendESPNOW("luz", "ON");
   
   // Guardar tiempo de inicio
   startTime = millis();
   
   Serial.println("Setup completado");
 }
 
 void loop() {
   // Leer estado del botón
   bool buttonState = digitalRead(BUTTON_PIN);
   
   // Gestión de presión prolongada del botón
   if (buttonState == HIGH) {  // Botón presionado
     if (!buttonPressed) {
       buttonPressed = true;
       buttonPressTime = millis();
       
       // Indicación visual
       leds[0] = CRGB::Blue;
       FastLED.show();
     } else {
       // Comprobar si se ha mantenido presionado durante 3 segundos
       if (!apActive && (millis() - buttonPressTime > 3000)) {
         Serial.println("Botón presionado durante 3 segundos - Iniciando AP");
         leds[0] = CRGB::White;  // Indicador visual de cambio de modo
         FastLED.show();
         delay(200);
         
         // Iniciar punto de acceso y servidor web
         startAP();
       }
     }
   } else {  // Botón no presionado
     if (buttonPressed) {
       buttonPressed = false;
       
       // Si fue una pulsación corta y no estamos en modo AP
       if (!apActive && (millis() - buttonPressTime < 3000)) {
         // Enviar mensaje ESP-NOW como respuesta a pulsación corta
         Serial.println("Pulsación corta - Enviando mensaje");
         leds[0] = CRGB::Yellow;
         FastLED.show();
         
         sendESPNOW("luz", "TOGGLE");
         
         delay(100);
         leds[0] = CRGB::Black;
         FastLED.show();
       }
     }
   }
   
   // Si el AP está activo, procesar solicitudes DNS
   if (apActive) {
     dnsServer.processNextRequest();
     
     // MEJORA: Timeout más largo para el modo AP (5 minutos = 300000ms)
     // Windows puede tardar un tiempo en mostrar la interfaz de usuario
     if (millis() - startTime > 300000) {
       Serial.println("Timeout del AP - Entrando en deep sleep");
       enterDeepSleep();
     }
   } else {
     // Timeout para entrar en deep sleep si no se hace nada (5 segundos)
     if (millis() - startTime > 5000 && !buttonPressed) {
       enterDeepSleep();
     }
   }
 }