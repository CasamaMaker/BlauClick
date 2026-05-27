    var hwFuncs      = [];
    var hwTemplates  = [];
    var hwInited     = false;
    var hwMcuProfiles  = {};
    var hwCustomMcu    = "";
    var hwIsCustomMode = false;
    var HW_CUSTOM_TMPL = 99;

    var HW_GPIO_MIN = 0;
    var HW_GPIO_MAX = 21;

    function hwMakeBadge(text, bg, color) {
      var b = document.createElement("span");
      b.textContent = text;
      b.style.cssText = "font-size:0.68em;background:" + bg + ";color:" + color + ";padding:1px 4px;border-radius:3px";
      return b;
    }

    function hwShowStdSection(show) {
      var el = document.getElementById("hwStdTable");
      if (el) el.style.display = show ? "" : "none";
    }

    function hwShowCustomSection(show) {
      var el = document.getElementById("hwCustomSection");
      if (el) el.style.display = show ? "block" : "none";
    }

    function hwInit() {
      if (hwInited) { hwFetchGpioMap(); return; }
      var pending = 3;
      function done() { if (--pending === 0) hwFetchGpioMap(); }
      apiGetHwFuncList(function(data) { hwFuncs = data; done(); });
      apiGetHwTemplates(function(data) {
        hwTemplates = data;
        var sel = document.getElementById("hwTemplateSelect");
        while (sel.options.length > 1) sel.remove(1);
        data.forEach(function(t, i) {
          var opt = document.createElement("option");
          opt.value = i; opt.textContent = t.name;
          sel.appendChild(opt);
        });
        var optCustom = document.createElement("option");
        optCustom.value = HW_CUSTOM_TMPL;
        optCustom.textContent = "Personalitzar";
        sel.appendChild(optCustom);
        done();
      });
      apiGetHwGpioCaps(function(data) {
        data.forEach(function(p) { hwMcuProfiles[p.id] = p; });
        done();
      });
      hwInited = true;
    }

    function hwFetchGpioMap() {
      apiGetHwGpioMap(function(data) { hwBuildTable(data); });
    }

    function hwBuildTable(data) {
      var tbody = document.getElementById("hwGpioTableBody");
      tbody.innerHTML = "";
      var c3caps = hwMcuProfiles["esp32c3"] ? hwMcuProfiles["esp32c3"].caps : null;
      for (var i = HW_GPIO_MIN; i <= HW_GPIO_MAX; i++) {
        var func = data["f" + i] || 0;
        var tr = document.createElement("tr");
        tr.style.borderBottom = "1px solid #f0f0f0";
        var cap = c3caps ? c3caps[i] : null;
        if (cap && !cap.valid) tr.style.opacity = "0.4";

        var tdGpio = document.createElement("td");
        tdGpio.style.cssText = "padding:5px 8px;font-family:monospace;vertical-align:top";
        tdGpio.appendChild(document.createTextNode("GPIO " + i));
        if (cap) {
          var badges = document.createElement("div");
          badges.style.cssText = "display:flex;gap:3px;margin-top:3px;flex-wrap:wrap";
          if (cap.hasAdc)    badges.appendChild(hwMakeBadge("ADC", "#d4edda", "#155724"));
          if (cap.hasPwm)    badges.appendChild(hwMakeBadge("PWM", "#cce5ff", "#004085"));
          if (cap.inputOnly) badges.appendChild(hwMakeBadge("IN",  "#fff3cd", "#856404"));
          tdGpio.appendChild(badges);
        }

        var tdFunc = document.createElement("td");
        tdFunc.style.padding = "4px 8px";

        var sel = document.createElement("select");
        sel.id = "hwFunc_" + i;
        sel.style.width = "100%";
        if (cap && !cap.valid) sel.disabled = true;
        sel.onchange = hwValidate;
        hwFuncs.forEach(function(f) {
          var opt = document.createElement("option");
          opt.value = f.id;
          opt.textContent = f.label;
          if (f.id === func) opt.selected = true;
          sel.appendChild(opt);
        });

        tdFunc.appendChild(sel);
        tr.appendChild(tdGpio);
        tr.appendChild(tdFunc);
        tbody.appendChild(tr);
      }

      var tmpl = data["tmpl"];
      var tmplSel = document.getElementById("hwTemplateSelect");
      if (tmpl === HW_CUSTOM_TMPL) {
        hwIsCustomMode = true;
        tmplSel.value = String(HW_CUSTOM_TMPL);
        hwShowStdSection(false);
        hwShowCustomSection(true);
        var mcu = data["mcu"] || "";
        if (mcu) {
          document.getElementById("hwMcuSelect").value = mcu;
          hwCustomMcu = mcu;
          hwBuildCustomGpioRows(mcu, data);
        }
      } else {
        hwIsCustomMode = false;
        tmplSel.value = (tmpl !== undefined && tmpl >= 0) ? tmpl : -1;
        hwShowStdSection(true);
        hwShowCustomSection(false);
      }

      hwValidate();
    }

    function hwApplyTemplate(idxStr) {
      var idx = parseInt(idxStr);
      if (idx === HW_CUSTOM_TMPL) {
        hwIsCustomMode = true;
        hwShowStdSection(false);
        hwShowCustomSection(true);
        hwCustomMcu = "";
        document.getElementById("hwMcuSelect").value = "";
        document.getElementById("hwCustomGpioContainer").innerHTML = "";
        hwValidate();
        return;
      }
      hwIsCustomMode = false;
      hwShowStdSection(true);
      hwShowCustomSection(false);
      if (idx < 0 || idx >= hwTemplates.length) { hwValidate(); return; }
      var tmpl = hwTemplates[idx];
      for (var i = HW_GPIO_MIN; i <= HW_GPIO_MAX; i++) {
        var sel = document.getElementById("hwFunc_" + i);
        if (sel) sel.value = 0;
      }
      tmpl.pins.forEach(function(p) {
        if (p.gpio >= HW_GPIO_MIN && p.gpio <= HW_GPIO_MAX) {
          var sel = document.getElementById("hwFunc_" + p.gpio);
          if (sel) sel.value = p.func;
        }
      });
      hwValidate();
    }

    function hwSelectMcu(mcuId) {
      if (!mcuId) {
        hwCustomMcu = "";
        document.getElementById("hwCustomGpioContainer").innerHTML = "";
        hwValidate(); return;
      }
      if (hwCustomMcu && hwCustomMcu !== mcuId) {
        var hadSel = false;
        var prof = hwMcuProfiles[hwCustomMcu];
        if (prof) {
          for (var i = 0; i < prof.caps.length; i++) {
            var s = document.getElementById("hwCustomFunc_" + i);
            if (s && parseInt(s.value) !== 0) { hadSel = true; break; }
          }
        }
        if (hadSel && !confirm("Canviar de MCU esborrar\xE0 les assignacions actuals. Continuar?")) {
          document.getElementById("hwMcuSelect").value = hwCustomMcu;
          return;
        }
      }
      hwCustomMcu = mcuId;
      hwBuildCustomGpioRows(mcuId, null);
    }

    function hwBuildCustomGpioRows(mcuId, data) {
      var container = document.getElementById("hwCustomGpioContainer");
      container.innerHTML = "";
      var profile = hwMcuProfiles[mcuId];
      if (!profile) return;
      var table = document.createElement("table");
      table.style.cssText = "width:100%;border-collapse:collapse;margin:8px 0";
      table.innerHTML = "<thead><tr style='border-bottom:1px solid #ddd'>" +
        "<th style='text-align:left;padding:6px 8px;width:100px'>GPIO</th>" +
        "<th style='text-align:left;padding:6px 8px'>Funci\xF3</th></tr></thead>";
      var tbody = document.createElement("tbody");
      profile.caps.forEach(function(cap, i) {
        var tr = document.createElement("tr");
        tr.style.borderBottom = "1px solid #f0f0f0";
        if (!cap.valid) tr.style.opacity = "0.4";

        var tdGpio = document.createElement("td");
        tdGpio.style.cssText = "padding:5px 8px;font-family:monospace;vertical-align:top";
        tdGpio.appendChild(document.createTextNode("GPIO " + i));
        var badges = document.createElement("div");
        badges.style.cssText = "display:flex;gap:3px;margin-top:3px;flex-wrap:wrap";
        if (cap.hasAdc)    badges.appendChild(hwMakeBadge("ADC", "#d4edda", "#155724"));
        if (cap.hasPwm)    badges.appendChild(hwMakeBadge("PWM", "#cce5ff", "#004085"));
        if (cap.inputOnly) badges.appendChild(hwMakeBadge("IN",  "#fff3cd", "#856404"));
        tdGpio.appendChild(badges);

        var tdFunc = document.createElement("td");
        tdFunc.style.padding = "4px 8px";
        var sel = document.createElement("select");
        sel.id = "hwCustomFunc_" + i;
        sel.style.width = "100%";
        sel.disabled = !cap.valid;
        sel.onchange = hwValidate;
        var savedVal = data ? (data["f" + i] || 0) : 0;
        hwFuncs.forEach(function(f) {
          var opt = document.createElement("option");
          opt.value = f.id;
          opt.textContent = f.label;
          if (f.id === savedVal) opt.selected = true;
          sel.appendChild(opt);
        });
        tdFunc.appendChild(sel);
        tr.appendChild(tdGpio);
        tr.appendChild(tdFunc);
        tbody.appendChild(tr);
      });
      table.appendChild(tbody);
      container.appendChild(table);
      hwValidate();
    }

    function hwValidate() {
      var counts = {};
      var duplicate = false;
      if (hwIsCustomMode && hwCustomMcu && hwMcuProfiles[hwCustomMcu]) {
        var caps = hwMcuProfiles[hwCustomMcu].caps;
        for (var i = 0; i < caps.length; i++) {
          if (!caps[i].valid) continue;
          var sel = document.getElementById("hwCustomFunc_" + i);
          if (!sel) continue;
          var v = parseInt(sel.value);
          if (v === 0) continue;
          counts[v] = (counts[v] || 0) + 1;
          if (counts[v] > 1) duplicate = true;
        }
      } else {
        for (var i = HW_GPIO_MIN; i <= HW_GPIO_MAX; i++) {
          var sel = document.getElementById("hwFunc_" + i);
          if (!sel) continue;
          var v = parseInt(sel.value);
          if (v === 0) continue;
          counts[v] = (counts[v] || 0) + 1;
          if (counts[v] > 1) duplicate = true;
        }
      }
      var msg = document.getElementById("hwValidationMsg");
      var btn = document.getElementById("hwSaveBtn");
      if (duplicate) {
        msg.textContent = "Error: la mateixa funci\xF3 est\xE0 assignada a m\xE9s d'un GPIO.";
        msg.style.display = "block";
        btn.disabled = true;
      } else {
        msg.style.display = "none";
        btn.disabled = false;
      }
    }

    function hwSave() {
      var params = [];
      if (hwIsCustomMode && hwCustomMcu && hwMcuProfiles[hwCustomMcu]) {
        var caps = hwMcuProfiles[hwCustomMcu].caps;
        for (var i = 0; i < caps.length; i++) {
          var sel = document.getElementById("hwCustomFunc_" + i);
          params.push("f" + i + "=" + (sel ? sel.value : 0));
        }
        params.push("mcu=" + hwCustomMcu);
        params.push("tmpl=" + HW_CUSTOM_TMPL);
      } else {
        for (var i = HW_GPIO_MIN; i <= HW_GPIO_MAX; i++) {
          var sel = document.getElementById("hwFunc_" + i);
          params.push("f" + i + "=" + (sel ? sel.value : 0));
        }
        var tmplSel = document.getElementById("hwTemplateSelect");
        params.push("tmpl=" + tmplSel.value);
      }

      var btn = document.getElementById("hwSaveBtn");
      btn.disabled = true;
      btn.textContent = "Guardant...";

      apiSaveHwGpioMap(params.join("&"), function() {
        btn.textContent = "Reiniciant dispositiu...";
      });
    }

    function hwConfirmClear() {
      if (!confirm("Esborrar la configuraci\xF3 de hardware? El dispositiu reiniciar\xE0.")) return;
      apiClearHwConfig(function() {});
    }
