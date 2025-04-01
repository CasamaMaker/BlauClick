![](pictures/2.jpg) <img src="https://github.com/CasamaMaker/CasamaMaker/blob/main/CasamaMaker-textlogo.png"  width="300" align="right">

[![GitHub version](https://img.shields.io/github/release/CasamaMaker/BlauLink.svg)](https://github.com/CasamaMaker/BlauLink/releases)
[![GitHub download](https://img.shields.io/github/downloads/CasamaMaker/BlauLink/total.svg)](https://github.com/CasamaMaker/BlauLink/releases/latest)
[![License](https://img.shields.io/github/license/CasamaMaker/BlauLink.svg)](LICENSE.txt)

# BlauLink
My IoT switch design

BlauLink is a small WiFi and BLE IoT button for various applications. Originally designed for smart home devices using ESP-NOW, BlauLink can also be used as an actuator for IFTTT automations or as an MQTT device. It is based on the single-core ESP32-C3 RISC-V processor, offering a wide range of useful features. With dimensions similar to a suitable switch, such as the [Mi Wireless Switch](https://tuxiaomi.es/informatica/mi-wireless-switch-global-version/), it is optimized to last at least a year on a single battery charge.

## Key features before development
- Battery-powered
- Integrated battery charger and protection, easy to charge
- Fast ESPNOW protocol for controlling devices like lights, without the need for WiFi or Bluetooth
- Easy integration of 3D printing and electronics
- Simple assembly
- 1-year battery life (4 clicks per day)
- Inspiration from designs: [PicoClick-C3](https://github.com/makermoekoe/Picoclick-C3) and [OBJEX_LINK](https://github.com/salvatoreraccardi/OBJEX_LINK)

## Web manager
To configure the BlauLink, simply press and hold the button for 3 seconds until the LED turns red. A WiFi network called "EspLink-AP_xxxx" will be created, and you can connect to it. Once connected, a web page will open where you can configure the address of your desired slave device.

![](pictures/web-manager.png) <img src="https://github.com/CasamaMaker/CasamaMaker/blob/main/CasamaMaker-textlogo.png"  width="300" align="right">
