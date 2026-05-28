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
        while (sel.options.length > 0) sel.remove(0);
        var optCustom = document.createElement("option");
        optCustom.value = HW_CUSTOM_TMPL;
        optCustom.textContent = "Personalitzar";
        sel.appendChild(optCustom);
        data.forEach(function(t, i) {
          var opt = document.createElement("option");
          opt.value = i; opt.textContent = t.name;
          sel.appendChild(opt);
        });
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

        var tdName = document.createElement("td");
        tdName.style.cssText = "padding:2px 3px";
        var nameInp = document.createElement("input");
        nameInp.type = "text"; nameInp.maxLength = 12;
        nameInp.style.cssText = "width:12ch;font-size:0.82em;border:1px solid #ddd;border-radius:4px;padding:2px 4px;box-sizing:border-box";
        tdName.appendChild(nameInp);

        var tdGpio = document.createElement("td");
        tdGpio.style.cssText = "text-align:center;font-weight:600;color:#555;padding:4px 2px;white-space:nowrap";
        tdGpio.appendChild(document.createTextNode(i));

        var tdBadges = document.createElement("td");
        tdBadges.style.cssText = "padding:2px 2px;vertical-align:middle";
        if (cap) {
          var badgeWrap = document.createElement("div");
          badgeWrap.style.cssText = "display:flex;flex-direction:column;gap:2px";
          if (cap.hasAdc)    badgeWrap.appendChild(hwMakeBadge("ADC", "#d4edda", "#155724"));
          if (cap.hasPwm)    badgeWrap.appendChild(hwMakeBadge("PWM", "#cce5ff", "#004085"));
          if (cap.inputOnly) badgeWrap.appendChild(hwMakeBadge("IN",  "#fff3cd", "#856404"));
          tdBadges.appendChild(badgeWrap);
        }

        var tdFunc = document.createElement("td");
        tdFunc.style.padding = "4px 8px";

        var sel = document.createElement("select");
        sel.id = "hwFunc_" + i;
        sel.style.cssText = "width:100%;font-size:0.82em";
        if (cap && !cap.valid) sel.disabled = true;
        sel.onchange = hwValidate;
        hwFuncs.forEach(function(f) {
          var opt = document.createElement("option");
          opt.value = f.id;
          opt.textContent = (f.id === 0) ? "" : f.label;
          if (f.id === func) opt.selected = true;
          sel.appendChild(opt);
        });

        tdFunc.appendChild(sel);
        tr.appendChild(tdName);
        tr.appendChild(tdGpio);
        tr.appendChild(tdBadges);
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
        tmplSel.value = (tmpl !== undefined && tmpl >= 0 && tmpl !== HW_CUSTOM_TMPL)
          ? String(tmpl)
          : String(HW_CUSTOM_TMPL);
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
      table.innerHTML = "<thead><tr style='color:#888;font-size:0.8em'>" +
        "<th style='text-align:left;padding:2px 3px;width:12ch'>Nom</th>" +
        "<th style='text-align:center;padding:2px 4px;width:3em'>GPIO</th>" +
        "<th style='padding:2px 2px;width:2.5em'></th>" +
        "<th style='text-align:left;padding:2px 4px'>Funci\xF3</th></tr></thead>";
      var tbody = document.createElement("tbody");
      profile.caps.forEach(function(cap, i) {
        var tr = document.createElement("tr");
        tr.style.borderBottom = "1px solid #f0f0f0";
        if (!cap.valid) tr.style.opacity = "0.4";

        var tdName = document.createElement("td");
        tdName.style.cssText = "padding:2px 3px";
        var nameInp = document.createElement("input");
        nameInp.type = "text"; nameInp.maxLength = 12;
        nameInp.style.cssText = "width:12ch;font-size:0.82em;border:1px solid #ddd;border-radius:4px;padding:2px 4px;box-sizing:border-box";
        tdName.appendChild(nameInp);

        var tdGpio = document.createElement("td");
        tdGpio.style.cssText = "text-align:center;font-weight:600;color:#555;padding:4px 2px;white-space:nowrap";
        tdGpio.appendChild(document.createTextNode(i));

        var tdBadges = document.createElement("td");
        tdBadges.style.cssText = "padding:2px 2px;vertical-align:middle";
        var badgeWrap = document.createElement("div");
        badgeWrap.style.cssText = "display:flex;flex-direction:column;gap:2px";
        if (cap.hasAdc)    badgeWrap.appendChild(hwMakeBadge("ADC", "#d4edda", "#155724"));
        if (cap.hasPwm)    badgeWrap.appendChild(hwMakeBadge("PWM", "#cce5ff", "#004085"));
        if (cap.inputOnly) badgeWrap.appendChild(hwMakeBadge("IN",  "#fff3cd", "#856404"));
        tdBadges.appendChild(badgeWrap);

        var tdFunc = document.createElement("td");
        tdFunc.style.padding = "4px 8px";
        var sel = document.createElement("select");
        sel.id = "hwCustomFunc_" + i;
        sel.style.cssText = "width:100%;font-size:0.82em";
        sel.disabled = !cap.valid;
        sel.onchange = hwValidate;
        var savedVal = data ? (data["f" + i] || 0) : 0;
        hwFuncs.forEach(function(f) {
          var opt = document.createElement("option");
          opt.value = f.id;
          opt.textContent = (f.id === 0) ? "" : f.label;
          if (f.id === savedVal) opt.selected = true;
          sel.appendChild(opt);
        });
        tdFunc.appendChild(sel);
        tr.appendChild(tdName);
        tr.appendChild(tdGpio);
        tr.appendChild(tdBadges);
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

    var _hwClearPending = false, _hwClearTimer = null;

    function hwConfirmClear() {
      var btn = document.getElementById("hwClearBtn");
      if (!_hwClearPending) {
        _hwClearPending = true;
        btn.textContent = "Confirmar esborrat";
        btn.style.background = "#e74c3c";
        btn.style.color = "#fff";
        btn.style.borderColor = "#c0392b";
        _hwClearTimer = setTimeout(function() {
          _hwClearPending = false;
          btn.textContent = "Esborrar configuraci\xF3";
          btn.style.background = "";
          btn.style.color = "";
          btn.style.borderColor = "";
        }, 5000);
      } else {
        clearTimeout(_hwClearTimer);
        _hwClearPending = false;
        apiClearHwConfig(function() {});
      }
    }
