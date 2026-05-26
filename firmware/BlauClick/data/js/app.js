    const SSID_FILTER_PREFIX = "BlauLux_";
    var allMacEntries = [];

    function formatMAC(input) {
        let value = input.value.replace(/[^A-Fa-f0-9]/g, '').toUpperCase();
        let formattedValue = '';

        for (let i = 0; i < value.length && i < 12; i += 2) {
            if (i > 0) {
                formattedValue += ':';
            }
            formattedValue += value.substr(i, 2);
        }

        input.value = formattedValue;
    }

    function fetchMac() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          document.getElementById("macSaved").innerHTML = this.responseText;
        }
      };
      xhttp.open("GET", "/mac", true);
      xhttp.send();
    }

    function deleteMac() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          fetchMac(); // Actualizar la MAC guardada después de borrar
        }
      };
      xhttp.open("GET", "/deletemac", true);
      xhttp.send();
    }

    function fetchMyMac() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          document.getElementById("myMac").innerHTML = this.responseText;
        }
      };
      xhttp.open("GET", "/mymac", true);
      xhttp.send();
    }

    function fetchMacList() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          const macList = this.responseText.split("\n");
          allMacEntries = [];
          macList.forEach(mac => {
            if (mac.trim() === "") return;
            const macParts = mac.split(" >> ");
            if (macParts.length > 1) {
              allMacEntries.push({ mac: macParts[0], ssid: macParts[1] });
            } else {
              allMacEntries.push({ mac: mac, ssid: "" });
            }
          });
          renderMacList();
        }
      };
      xhttp.open("GET", "/macList", true);
      xhttp.send();
    }

    function renderMacList() {
      const macListContainer = document.getElementById("macList");
      macListContainer.innerHTML = '';
      const filterEnabled = document.getElementById("ssidFilter").checked;
      const entries = filterEnabled
        ? allMacEntries.filter(e => e.ssid.startsWith(SSID_FILTER_PREFIX))
        : allMacEntries;
      if (entries.length === 0) {
        macListContainer.innerHTML = '<p class="no-devices">' + t('noDevicesFound') + '</p>';
        return;
      }
      entries.forEach(entry => {
        const macDiv = document.createElement("div");
        macDiv.classList.add("mac-item");
        macDiv.innerText = entry.ssid || entry.mac;
        macDiv.setAttribute('data-mac', entry.mac);
        macDiv.setAttribute('data-ssid', entry.ssid);
        macDiv.onclick = function() { setMac(this.getAttribute('data-mac'), this.getAttribute('data-ssid')); };
        macListContainer.appendChild(macDiv);
      });
    }

    function applyFilter() {
      renderMacList();
    }

    function setMac(mac, ssid) {
      document.getElementById("mac").value = mac;
      document.getElementById("ssid").value = ssid || "";
    }

    function refreshDevices() {
      fetchMacList();
    }

    // Función para manejar el clic en el enlace de GitHub
    function handleGitHubClick(event) {
      // Enviar señal al servidor para desconectar WiFi AP
      disconnectWiFiAP();

      // Permitir que el enlace se abra normalmente
      // No necesitamos event.preventDefault() porque queremos que el enlace funcione
    }

 // Función para obtener el nivel de batería
    async function fetchBatteryLevel() {
      try {
        if ('getBattery' in navigator) {
          const battery = await navigator.getBattery();
          updateBatteryInfo(battery);

          // Actualizar cuando cambie el estado de la batería
          battery.addEventListener('levelchange', () => updateBatteryInfo(battery));
          battery.addEventListener('chargingchange', () => updateBatteryInfo(battery));
        } else {
          // Fallback: intentar obtener del servidor
          fetchBatteryFromServer();
        }
      } catch (error) {
        console.log('Battery API no disponible, intentando obtener del servidor');
        fetchBatteryFromServer();
      }
    }

    function updateBatteryInfo(battery) {
      const level = Math.round(battery.level * 100);
      const isCharging = battery.charging;

      // document.getElementById("batteryLevel").textContent = level + '%';

      const batteryIcon = document.getElementById("batteryIcon");
      const batteryFill = document.getElementById("batteryFill");
      const batteryPercentage = document.getElementById("batteryPercentage");

      // Calcular el ancho del relleno respetando los márgenes del contenedor
      // El contenedor interno tiene width=16 (18-2=16 por los márgenes de 1px a cada lado)
      const maxFillWidth = 26;
      const fillWidth = Math.max(0, Math.min(maxFillWidth, (level / 100) * maxFillWidth));

      // Actualizar el relleno de la batería
      batteryFill.setAttribute('width', fillWidth);

      // Actualizar el texto del porcentaje
      batteryPercentage.textContent = level + '%';

      // Cambiar color según el nivel
      if (level <= 20) {
        batteryFill.style.fill = '#ff4444';
        batteryPercentage.style.fill = '#fff';
        batteryPercentage.style.stroke = '#000';
      } else if (level <= 50) {
        batteryFill.style.fill = '#ffaa00';
        batteryPercentage.style.fill = '#fff';
        batteryPercentage.style.stroke = '#000';
      } else {
        batteryFill.style.fill = '#44ff44';
        batteryPercentage.style.fill = '#fff';
        batteryPercentage.style.stroke = '#000';
      }

      // Mostrar icono de carga si está cargando
      if (isCharging) {
        batteryIcon.classList.add('charging');
      } else {
        batteryIcon.classList.remove('charging');
      }
    }

    function fetchBatteryFromServer() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          try {
            const batteryData = JSON.parse(this.responseText);
            const mockBattery = {
              level: batteryData.level / 100,
              charging: batteryData.charging || false
            };
            updateBatteryInfo(mockBattery);
          } catch (e) {
            // Si no hay respuesta válida, mostrar un valor por defecto
            document.getElementById("batteryPercentage").textContent = '--';
          }
        }
      };
      xhttp.open("GET", "/battery", true);
      xhttp.send();
    }

    // Simular batería para demo
    function simulateBattery() {
      const mockBattery = {
        level: 0.15, // 75%
        charging: false
      };
      updateBatteryInfo(mockBattery);
    }

    function showPage(id) {
      document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
      document.getElementById(id).classList.add('active');
    }

    window.onload = function() {
      currentLang = detectLang();
      document.getElementById('langSelector').value = currentLang;
      applyTranslations(currentLang);

      fetchMyMac();
      fetchMac();
      // simulateBattery();  // Para demo, usar batería simulada
      fetchBatteryLevel();  //per llegir info real de bateria
      fetchMacList();
      initCmdCard();

      // Establecer un temporizador para actualizar la lista de dispositivos cada 30 segundos
      setInterval(refreshDevices, 30000);
      // // Actualizar batería cada 60 segundos
      // setInterval(fetchBatteryLevel, 60000);
    }

    function selectCmd(el) {
      document.querySelectorAll('.cmd-item').forEach(function(d) {
        d.classList.remove('cmd-item-selected');
      });
      el.classList.add('cmd-item-selected');
      onCmdChange(el.getAttribute('data-value'));
    }

    function initCmdCard() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var data = JSON.parse(this.responseText);
          document.querySelectorAll('.cmd-item').forEach(function(d) {
            if (parseInt(d.getAttribute('data-value')) === data.cmd) {
              d.classList.add('cmd-item-selected');
            }
          });
          document.getElementById('brightnessSlider').value = data.p1;
          document.getElementById('brightnessValue').textContent = data.p1;
          var hex = '#' + ('0' + data.p1.toString(16)).slice(-2) +
                          ('0' + data.p2.toString(16)).slice(-2) +
                          ('0' + data.p3.toString(16)).slice(-2);
          document.getElementById('rgbPicker').value = hex;
          onCmdChange(String(data.cmd));
        }
      };
      xhttp.open("GET", "/1click_cmd", true);
      xhttp.send();
    }

    function onCmdChange(val) {
      document.getElementById('brightnessControl').style.display = (val === '4') ? 'block' : 'none';
      document.getElementById('rgbControl').style.display        = (val === '5') ? 'block' : 'none';
    }

    function saveCmdConfig() {
      var sel = document.querySelector('.cmd-item.cmd-item-selected');
      if (!sel) return;
      var cmd = parseInt(sel.getAttribute('data-value'));
      var p1 = 0, p2 = 0, p3 = 0;
      if (cmd === 4) {
        p1 = parseInt(document.getElementById('brightnessSlider').value);
      } else if (cmd === 5) {
        var hex = document.getElementById('rgbPicker').value;
        p1 = parseInt(hex.substr(1, 2), 16);
        p2 = parseInt(hex.substr(3, 2), 16);
        p3 = parseInt(hex.substr(5, 2), 16);
      }
      var xhttp = new XMLHttpRequest();
      xhttp.open("POST", "/save_1click_cmd", true);
      xhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
      xhttp.send("cmd=" + cmd + "&p1=" + p1 + "&p2=" + p2 + "&p3=" + p3);
    }
