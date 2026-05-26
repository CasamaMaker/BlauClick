const i18n = {
  ca: {
    pageTitle:      'Configurador BlauClick',
    h1:             'Configurador BlauClick',
    labelMyMac:     'La meva adreça MAC',
    loading:        'Carregant...',
    navBack:        '← Enrere',
    labelMacSaved:  'Adreça MAC esclau guardada',
    labelNewMac:    'Nova adreça MAC esclau',
    submitMacBtn:   'Guardar MAC',
    devicesFound:   'Dispositius trobats',
    filterLabel:    'Mostra només ',
    scanningDevices:'Escaneant dispositius...',
    noDevicesFound:  "No s'han trobat dispositius",
    comingSoon:      'Pròximament...',
    ordresTitle:     'Ordres',
    cmdToggle:       'Toggle',
    cmdOn:           'On',
    cmdOff:          'Off',
    cmdSetBrightness:'Set Brightness',
    cmdSetRgb:       'RGB',
    cmdSetCct:       'CCT',
    cmdSetScene:     'Scene',
    cmdDimUp:        'Up',
    cmdDimDown:      'Down',
    brightnessLabel: 'Brillantor',
    rgbLabel:        'Color RGB',
    saveCmdBtn:      'Guardar configuració',
  },
  en: {
    pageTitle:      'BlauClick Configurator',
    h1:             'BlauClick Configurator',
    labelMyMac:     'My MAC address',
    loading:        'Loading...',
    navBack:        '← Back',
    labelMacSaved:  'Saved slave MAC address',
    labelNewMac:    'New slave MAC address',
    submitMacBtn:   'Save MAC',
    devicesFound:   'Devices found',
    filterLabel:    'Show only ',
    scanningDevices:'Scanning devices...',
    noDevicesFound:  'No devices found',
    comingSoon:      'Coming soon...',
    ordresTitle:     'Commands',
    cmdToggle:       'Toggle',
    cmdOn:           'On',
    cmdOff:          'Off',
    cmdSetBrightness:'Set Brightness',
    cmdSetRgb:       'RGB',
    cmdSetCct:       'CCT',
    cmdSetScene:     'Scene',
    cmdDimUp:        'Up',
    cmdDimDown:      'Down',
    brightnessLabel: 'Brightness',
    rgbLabel:        'RGB Color',
    saveCmdBtn:      'Save configuration',
  },
  es: {
    pageTitle:      'Configurador BlauClick',
    h1:             'Configurador BlauClick',
    labelMyMac:     'Mi dirección MAC',
    loading:        'Cargando...',
    navBack:        '← Atrás',
    labelMacSaved:  'Dirección MAC esclavo guardada',
    labelNewMac:    'Nueva dirección MAC esclavo',
    submitMacBtn:   'Guardar MAC',
    devicesFound:   'Dispositivos encontrados',
    filterLabel:    'Mostrar solo ',
    scanningDevices:'Escaneando dispositivos...',
    noDevicesFound:  'No se han encontrado dispositivos',
    comingSoon:      'Próximamente...',
    ordresTitle:     'Órdenes',
    cmdToggle:       'Toggle',
    cmdOn:           'On',
    cmdOff:          'Off',
    cmdSetBrightness:'Set Brightness',
    cmdSetRgb:       'RGB',
    cmdSetCct:       'CCT',
    cmdSetScene:     'Scene',
    cmdDimUp:        'Up',
    cmdDimDown:      'Down',
    brightnessLabel: 'Brillo',
    rgbLabel:        'Color RGB',
    saveCmdBtn:      'Guardar configuración',
  },
};

let currentLang = 'ca';

function t(key) {
  return (i18n[currentLang] || i18n.ca)[key] || key;
}

function applyTranslations(lang) {
  const tr = i18n[lang] || i18n.ca;
  document.querySelectorAll('[data-i18n]').forEach(el => {
    const k = el.getAttribute('data-i18n');
    if (tr[k] !== undefined) el.textContent = tr[k];
  });
  document.querySelectorAll('[data-i18n-value]').forEach(el => {
    const k = el.getAttribute('data-i18n-value');
    if (tr[k] !== undefined) el.value = tr[k];
  });
  document.title = tr.pageTitle || document.title;
  document.documentElement.lang = lang;
}

function setLanguage(lang) {
  if (!i18n[lang]) lang = 'ca';
  currentLang = lang;
  try { localStorage.setItem('blau_lang', lang); } catch(e) {}
  applyTranslations(lang);
}

function detectLang() {
  try { const s = localStorage.getItem('blau_lang'); if (s && i18n[s]) return s; } catch(e) {}
  const nav = (navigator.language || 'ca').toLowerCase();
  if (nav.startsWith('ca')) return 'ca';
  if (nav.startsWith('es')) return 'es';
  if (nav.startsWith('en')) return 'en';
  return 'ca';
}
