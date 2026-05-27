    function apiGetHwGpioMap(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) cb(JSON.parse(this.responseText));
      };
      x.open("GET", "/hw_gpiomap", true); x.send();
    }

    function apiSaveHwGpioMap(params, cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && cb) cb(this.status == 200);
      };
      x.open("POST", "/hw_gpiomap", true);
      x.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      x.send(params);
    }

    function apiGetHwTemplates(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) cb(JSON.parse(this.responseText));
      };
      x.open("GET", "/hw_templates", true); x.send();
    }

    function apiGetHwFuncList(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) cb(JSON.parse(this.responseText));
      };
      x.open("GET", "/hw_funclist", true); x.send();
    }

    function apiGetHwGpioCaps(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) cb(JSON.parse(this.responseText));
      };
      x.open("GET", "/hw_gpiocaps", true); x.send();
    }

    function apiClearHwConfig(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && cb) cb(this.status == 200);
      };
      x.open("POST", "/hw_clear", true);
      x.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      x.send("");
    }

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
