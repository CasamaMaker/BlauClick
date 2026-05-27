    var hwFuncs     = [];
    var hwTemplates = [];
    var hwInited    = false;

    var HW_GPIO_MIN = 0;
    var HW_GPIO_MAX = 10;

    function hwInit() {
      if (hwInited) { hwFetchGpioMap(); return; }
      var pending = 2;
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
      for (var i = HW_GPIO_MIN; i <= HW_GPIO_MAX; i++) {
        var func = data["f" + i] || 0;
        var tr = document.createElement("tr");
        tr.style.borderBottom = "1px solid #f0f0f0";

        var tdGpio = document.createElement("td");
        tdGpio.style.padding = "5px 8px";
        tdGpio.style.fontFamily = "monospace";
        tdGpio.textContent = "GPIO " + i;

        var tdFunc = document.createElement("td");
        tdFunc.style.padding = "4px 8px";

        var sel = document.createElement("select");
        sel.id = "hwFunc_" + i;
        sel.style.width = "100%";
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
      tmplSel.value = (tmpl !== undefined && tmpl >= 0) ? tmpl : -1;

      hwValidate();
    }

    function hwApplyTemplate(idxStr) {
      var idx = parseInt(idxStr);
      if (idx < 0 || idx >= hwTemplates.length) return;
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

    function hwValidate() {
      var counts = {};
      var duplicate = false;
      for (var i = HW_GPIO_MIN; i <= HW_GPIO_MAX; i++) {
        var sel = document.getElementById("hwFunc_" + i);
        if (!sel) continue;
        var v = parseInt(sel.value);
        if (v === 0) continue;
        counts[v] = (counts[v] || 0) + 1;
        if (counts[v] > 1) duplicate = true;
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
      for (var i = HW_GPIO_MIN; i <= HW_GPIO_MAX; i++) {
        var sel = document.getElementById("hwFunc_" + i);
        params.push("f" + i + "=" + (sel ? sel.value : 0));
      }
      var tmplSel = document.getElementById("hwTemplateSelect");
      params.push("tmpl=" + tmplSel.value);

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
