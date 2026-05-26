    // Nueva función para desconectar WiFi AP
    function disconnectWiFiAP() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4) {
          if (this.status == 200) {
            console.log("Señal de desconexión WiFi AP enviada correctamente");
          } else {
            console.log("Error al enviar señal de desconexión WiFi AP");
          }
        }
      };
      xhttp.open("GET", "/disconnect-ap", true);
      xhttp.send();
    }
