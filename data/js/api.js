    function apiGetChipInfo(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          try { cb(JSON.parse(this.responseText)); } catch(e) {}
        }
      };
      x.open("GET", "/chipinfo", true); x.send();
    }

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

    // Nom del dispositiu
    function apiGetDeviceName(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200 && cb) cb(this.responseText);
      };
      x.open("GET", "/devicename", true); x.send();
    }

    function apiSaveDeviceName(name, cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && cb) cb(this.status == 200);
      };
      x.open("POST", "/devicename", true);
      x.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      x.send("device_name=" + encodeURIComponent(name));
    }

    function apiClearDeviceName(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && cb) cb(this.responseText);
      };
      x.open("POST", "/cleardevicename", true);
      x.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      x.send("");
    }

    // Seguretat BlauProtocol v2
    function apiGetSecurityStatus(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) cb(JSON.parse(this.responseText));
      };
      x.open("GET", "/securityStatus", true); x.send();
    }

    function apiSaveSecurity(pass, cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && cb) cb(this.status == 200);
      };
      x.open("POST", "/security", true);
      x.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      x.send("protopass=" + encodeURIComponent(pass));
    }

    function apiClearSecurity(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && cb) cb(this.status == 200);
      };
      x.open("POST", "/clearsecurity", true);
      x.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      x.send("");
    }

    // Nueva función para desconectar WiFi AP
    function apiGetInfo(cb) {
      var x = new XMLHttpRequest();
      x.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) cb(JSON.parse(this.responseText));
      };
      x.open("GET", "/info", true); x.send();
    }

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
