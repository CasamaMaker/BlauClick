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
      if (id === 'page-config-hardware') hwInit();
      if (id === 'page-config-seguretat') fetchSecurityStatus();
      if (id === 'page-config-altres') fetchDeviceName();
      if (id === 'page-ota') fetchOtaVersion();
    }

    // ── ESP-NOW / BlauProtocol v2 estat ────────────────────────────

    function fetchEspNowStatus() {
      apiGetSecurityStatus(function(data) {
        var badge = document.getElementById('espnowStatusBadge');
        if (badge) {
          badge.textContent = data.configured ? t('espnowActive') : t('espnowLegacy');
          badge.style.color = data.configured ? '#27ae60' : '#888';
        }
      });
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          var mac = this.responseText.trim();
          var el = document.getElementById('espnowPeer');
          if (el) el.textContent = (mac && mac !== 'FF:FF:FF:FF:FF:FF') ? mac : t('espnowNoPeer');
        }
      };
      xhttp.open("GET", "/mac", true);
      xhttp.send();
    }

    // ── Seguretat BlauProtocol v2 ──────────────────────────────────

    function fetchSecurityStatus() {
      apiGetSecurityStatus(function(data) {
        var badge = document.getElementById('secStatusBadge');
        if (badge) {
          badge.textContent = data.configured ? t('secConfigured') : t('secNotConfigured');
          badge.style.color = data.configured ? '#27ae60' : '#888';
        }
      });
    }

    // ── Nom del dispositiu ─────────────────────────────────────────

    function fetchDeviceName() {
      apiGetDeviceName(function(name) {
        var el = document.getElementById('deviceNameInput');
        if (el) el.value = name.trim();
      });
    }

    function _altresMsg(text, isError) {
      var el = document.getElementById('altresMsg');
      if (el) { el.textContent = text; el.style.color = isError ? '#e74c3c' : '#27ae60'; }
    }

    function saveDeviceName() {
      var name = document.getElementById('deviceNameInput').value.trim();
      if (!name) return;
      apiSaveDeviceName(name, function(ok) {
        _altresMsg(ok ? t('altresSaved') : t('altresError'), !ok);
      });
    }

    var _clearAltresPending = false, _clearAltresTimer = null;
    function confirmClearAltres() {
      var btn = document.getElementById('clearAltresBtn');
      if (!_clearAltresPending) {
        _clearAltresPending = true;
        btn.textContent = t('secClearConfirm');
        _clearAltresTimer = setTimeout(function() {
          _clearAltresPending = false;
          btn.textContent = t('clearAltresBtn');
        }, 5000);
      } else {
        clearTimeout(_clearAltresTimer); _clearAltresPending = false;
        btn.textContent = t('clearAltresBtn');
        apiClearDeviceName(function(name) {
          var el = document.getElementById('deviceNameInput');
          if (el) el.value = name.trim();
          _altresMsg(t('clearAltresDone'), false);
        });
      }
    }

    function _secMsg(text, isError) {
      var el = document.getElementById('secMsg');
      el.textContent = text;
      el.style.color = isError ? '#e74c3c' : '#27ae60';
    }

    function saveSecurityPass() {
      var pass = document.getElementById('secPassInput').value.trim();
      if (pass.length < 8 || pass.length > 63) { _secMsg(t('secLenError'), true); return; }
      apiSaveSecurity(pass, function(ok) {
        if (ok) {
          document.getElementById('secPassInput').value = '';
          _secMsg(t('secSaved'), false);
          fetchSecurityStatus();
        } else {
          _secMsg(t('secError'), true);
        }
      });
    }

    var _clearSecPending = false, _clearSecTimer = null;
    function confirmClearSecurity() {
      var btn = document.getElementById('clearSecBtn');
      if (!_clearSecPending) {
        _clearSecPending = true;
        btn.textContent = t('secClearConfirm');
        _clearSecTimer = setTimeout(function() {
          _clearSecPending = false;
          btn.textContent = t('secClearBtn');
        }, 5000);
      } else {
        clearTimeout(_clearSecTimer); _clearSecPending = false;
        btn.textContent = t('secClearBtn');
        apiClearSecurity(function(ok) {
          _secMsg(ok ? t('secCleared') : t('secError'), !ok);
          fetchSecurityStatus();
        });
      }
    }

    window.onload = function() {
      currentLang = detectLang();
      document.getElementById('langSelector').value = currentLang;
      applyTranslations(currentLang);

      fetchMyMac();
      fetchMac();
      fetchEspNowStatus();
      // simulateBattery();  // Para demo, usar batería simulada
      fetchBatteryLevel();  //per llegir info real de bateria
      apiGetInfo(function(info) {
        var el = document.getElementById('firmwareVersion');
        if (el && info.version) el.textContent = 'BlauClick ' + info.version + ' | Martí Casamayor';
      });
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

    // ── OTA ──────────────────────────────────────────────────────────

    function _buildChipInfoHtml(info) {
      function fmtB(n) { return n >= 1048576 ? (n/1048576).toFixed(1)+' MB' : Math.round(n/1024)+' kB'; }
      function pct(dbm) { return Math.min(100, Math.max(0, 2*(dbm+100))); }
      var net = info.wifi_ip && info.wifi_ip !== '0.0.0.0';
      var sections = [
        { h: t('otaSecSW'), rows: [
          [t('otaFwVer'),    info.fw_ver    || '—'],
          [t('otaFwFile'),   info.fw_file   || '—'],
          [t('otaBuildDate'),info.build_date|| '—'],
        ]},
        { h: t('otaSecSys'), rows: [
          [t('otaUptime'),  info.uptime         || '—'],
          [t('otaRestart'), info.restart_reason || '—'],
          [t('otaCpuTemp'), info.cpu_temp !== undefined ? parseFloat(info.cpu_temp).toFixed(1)+' °C' : '—'],
        ]},
        { h: t('otaSecChip'), rows: [
          ['Xip',         (info.chip||'—')+' rev.'+(info.chip_rev||0)],
          [t('otaCores'), (info.cores||0)+' × '+(info.cpu_mhz||0)+' MHz'],
          ['IDF',         info.idf_ver||'—'],
        ]},
      ];
      var mem = [
        [t('otaHeap'), fmtB(info.heap_free)+' lliure / '+fmtB(info.heap_total)],
      ];
      if ((info.psram_size||0) > 0) mem.push(['PSRAM', fmtB(info.psram_free)+' lliure / '+fmtB(info.psram_size)]);
      mem.push(['Flash',         fmtB(info.flash_size)+' @ '+info.flash_mhz+' MHz']);
      mem.push([t('otaSketch'),  fmtB(info.sketch_size)+' / '+fmtB(info.sketch_size+info.sketch_free)]);
      mem.push([t('otaFilesys'), fmtB(info.fs_used)+' / '+fmtB(info.fs_total)]);
      sections.push({ h: t('otaSecMem'), rows: mem });
      var netRows = [];
      if (info.wifi_hostname) netRows.push(['Hostname', info.wifi_hostname]);
      if (net) {
        netRows.push(['IP',      info.wifi_ip]);
        netRows.push(['Gateway', info.wifi_gw]);
        netRows.push(['Subnet',  info.wifi_mask]);
        if (info.wifi_dns1 && info.wifi_dns1 !== '0.0.0.0') netRows.push(['DNS 1', info.wifi_dns1]);
        if (info.wifi_dns2 && info.wifi_dns2 !== '0.0.0.0') netRows.push(['DNS 2', info.wifi_dns2]);
        netRows.push(['SSID',   info.wifi_ssid]);
        netRows.push([t('otaRSSI'), pct(info.wifi_rssi_dbm)+'%, '+info.wifi_rssi_dbm+' dBm']);
        netRows.push(['Canal',  info.wifi_ch]);
        netRows.push(['BSSID',  info.wifi_bssid]);
      }
      netRows.push(['MAC', info.mac]);
      sections.push({ h: t('otaSecNet'), rows: netRows });
      var h = '<table style="border-collapse:collapse;width:100%;">';
      sections.forEach(function(s) {
        h += '<tr><td colspan="2" style="padding:6px 0 2px;font-weight:600;color:#1a73e8;font-size:0.88em;border-bottom:1px solid #e0e0e0;">'+s.h+'</td></tr>';
        s.rows.forEach(function(r) {
          h += '<tr><td style="padding:1px 10px 1px 0;color:#888;white-space:nowrap;">'+r[0]+'</td><td style="padding:1px 0;font-weight:500;color:#333;">'+r[1]+'</td></tr>';
        });
      });
      return h + '</table>';
    }

    function fetchOtaVersion() {
      var el = document.getElementById('otaInfo');
      if (!el) return;
      apiGetChipInfo(function(info) {
        el.innerHTML = _buildChipInfoHtml(info);
      });
    }

    function startOTA() {
      var file = document.getElementById('otaFile').files[0];
      if (!file) { _otaMsg(t('otaNoFile'), false); return; }
      var xhr = new XMLHttpRequest();
      xhr.upload.onprogress = function(e) {
        if (!e.lengthComputable) return;
        var pct = Math.round(e.loaded / e.total * 100);
        document.getElementById('otaProgress').value = pct;
        document.getElementById('otaPct').textContent = pct + '%';
      };
      xhr.onload = function() {
        var ok = false;
        try { ok = JSON.parse(xhr.responseText).ok; } catch(e) {}
        _otaMsg(ok ? t('otaOk') : t('otaError'), !ok);
      };
      xhr.onerror = function() { _otaMsg(t('otaError'), true); };
      var form = new FormData();
      form.append('file', file, file.name);
      document.getElementById('otaProgressWrap').style.display = 'block';
      _otaMsg(t('otaUploading'), false);
      xhr.open('POST', '/ota-upload');
      xhr.send(form);
    }

    function _otaMsg(text, isError) {
      var el = document.getElementById('otaStatus');
      if (el) { el.textContent = text; el.style.color = isError ? '#e74c3c' : '#555'; }
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
