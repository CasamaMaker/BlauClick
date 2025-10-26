import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
import pandas as pd
from scipy.optimize import curve_fit
import warnings
warnings.filterwarnings('ignore')

# Enllaç compartit (pot contenir 'edit?usp=sharing')
sheet_url = "https://docs.google.com/spreadsheets/d/1WHqHPniyA06FTHolcYKQ4djUFw6aU1ZD4E3WRfT_PN0/edit?usp=sharing"
# Convertim l'URL per obtenir el CSV
csv_url = sheet_url.replace("/edit?usp=sharing", "").replace("/edit", "") + "/export?format=csv"
# Llegim el CSV
df = pd.read_csv(csv_url)
# Ens assegurem que les columnes es diguin 'data' i 'voltatge'
df.columns = df.columns.str.strip().str.lower()
print("Columnes llegides:", df.columns.tolist())

# Convertim a les variables que ja tens
dates_str = df['dia'].astype(str).tolist()
# Convertim voltatges de coma a punt i després a float
voltages = df['voltatge'].str.replace(',', '.', regex=False).astype(float).tolist()

# Filtrem les dades vàlides (no NaN ni buides)
valid_data = []
for i, (date_str, voltage) in enumerate(zip(dates_str, voltages)):
    if (isinstance(date_str, str) and 
        date_str.strip() != "" and 
        date_str.lower() != "nan" and 
        pd.notnull(voltage) and 
        not pd.isna(voltage)):
        valid_data.append((date_str, voltage))

# Separar dates i voltatges vàlids
dates_str_valid = [item[0] for item in valid_data]
voltages_valid = [item[1] for item in valid_data]

print(f"\nDades vàlides: {len(valid_data)} punts")
print("Dates strings vàlids:", dates_str_valid)
print("Voltatges vàlids:", voltages_valid)

# Convertir les dates (ara manejant el format amb hora)
dates = []
for d in dates_str_valid:
    try:
        if ' ' in d:
            date_part = d.split()[0]
            dates.append(datetime.strptime(date_part, "%d/%m/%Y"))
        else:
            dates.append(datetime.strptime(d, "%d/%m/%Y"))
    except ValueError as e:
        print(f"Error convertint data '{d}': {e}")
        continue

# Usar només els voltatges que corresponen a dates vàlides
voltages = voltages_valid[:len(dates)]

print(f"\nDates processades: {len(dates)}")
print("Dates convertides:", [d.strftime('%d/%m/%Y') for d in dates])
print(f"Voltatges processats: {len(voltages)}")

# Verificar que tenim dades suficients
if len(dates) < 3:
    print("Error: No hi ha prou dades vàlides per fer la predicció")
    exit()

# Convertim les dates en dies des de la primera mesura
days = np.array([(d - dates[0]).days for d in dates])
voltages = np.array(voltages)

print(f"\nDies calculats: {days}")
print(f"Voltatges: {voltages}")

# Verificar el càlcul de dies
print(f"\nVerificació de dates:")
for i, (date, day) in enumerate(zip(dates, days)):
    print(f"  {date.strftime('%d/%m/%Y')} -> Dia {day}")

# Model realista per a bateria LiPo (30mAh)
# Les bateries LiPo tenen característiques específiques:
# - Voltatge nominal: 3.7V
# - Voltatge màxim: 4.2V
# - Voltatge mínim segur: 3.0V-3.3V
# - Descàrrega quasi-lineal en la majoria del rang

def lipo_discharge_model(t, V_max, k_linear, k_exp, t_transition):
    """
    Model específic per a bateries LiPo:
    - Fase 1: Descàrrega quasi-lineal (característic de LiPo)
    - Fase 2: Caiguda exponencial ràpida cap al final
    """
    V = np.zeros_like(t)
    
    # Fase 1: descàrrega quasi-lineal
    mask1 = t <= t_transition
    V[mask1] = V_max - k_linear * t[mask1]
    
    # Fase 2: caiguda exponencial ràpida
    mask2 = t > t_transition
    V_transition = V_max - k_linear * t_transition
    V[mask2] = V_transition * np.exp(-k_exp * (t[mask2] - t_transition))
    
    return V

def lipo_simple_linear(t, V_max, k_linear):
    """Model lineal simple per a LiPo"""
    return V_max - k_linear * t

def lipo_exponential(t, V_max, k_exp):
    """Model exponencial per a LiPo"""
    return V_max * np.exp(-k_exp * t)

# Paràmetres específics per a LiPo de 30mAh
# Estimació basada en característiques típiques de LiPo
LIPO_V_MAX = 4.2  # Voltatge màxim de LiPo
LIPO_V_MIN = 3.0   # Voltatge mínim segur
LIPO_V_NOMINAL = 3.7  # Voltatge nominal

# Ajust de diferents models
models_to_test = {}

# Model 1: Lineal simple
try:
    popt_linear, _ = curve_fit(lipo_simple_linear, days, voltages, 
                              p0=[max(voltages), 0.001], 
                              bounds=([4.0, 0.0001], [4.3, 0.01]))
    models_to_test['Linear'] = {
        'params': popt_linear,
        'func': lipo_simple_linear,
        'label': 'Model Lineal'
    }
except:
    print("Error ajustant model lineal")

# Model 2: Exponencial simple
try:
    popt_exp, _ = curve_fit(lipo_exponential, days, voltages, 
                           p0=[max(voltages), 0.001], 
                           bounds=([4.0, 0.0001], [4.3, 0.01]))
    models_to_test['Exponential'] = {
        'params': popt_exp,
        'func': lipo_exponential,
        'label': 'Model Exponencial'
    }
except:
    print("Error ajustant model exponencial")

# Model 3: LiPo realista
try:
    popt_lipo, _ = curve_fit(lipo_discharge_model, days, voltages, 
                            p0=[max(voltages), 0.001, 0.01, max(days)*2], 
                            bounds=([4.0, 0.0001, 0.001, max(days)], 
                                   [4.3, 0.01, 0.1, max(days)*5]))
    models_to_test['LiPo_Realistic'] = {
        'params': popt_lipo,
        'func': lipo_discharge_model,
        'label': 'Model LiPo Realista'
    }
except:
    print("Error ajustant model LiPo realista")

# Seleccionar el millor model basant-se en R²
best_model = None
best_r2 = -np.inf
best_name = ""

for name, model in models_to_test.items():
    fitted_values = model['func'](days, *model['params'])
    ss_res = np.sum((voltages - fitted_values) ** 2)
    ss_tot = np.sum((voltages - np.mean(voltages)) ** 2)
    r2 = 1 - (ss_res / ss_tot)
    
    print(f"\n{name}: R² = {r2:.4f}")
    
    if r2 > best_r2:
        best_r2 = r2
        best_model = model
        best_name = name

print(f"\nMillor model: {best_name} (R² = {best_r2:.4f})")

# Generar predicció amb el millor model
max_prediction_days = 2000  # Límit màxim per a prediccions
extended_days = np.linspace(0, max_prediction_days, max_prediction_days)
extended_voltages = best_model['func'](extended_days, *best_model['params'])

# Limitar voltatges a rangs realistes per a LiPo
extended_voltages = np.clip(extended_voltages, LIPO_V_MIN, LIPO_V_MAX)

# Trobar prediccions per a voltatges crítics de LiPo
target_voltages = [4.0, 3.8, 3.7, 3.5, 3.3, 3.0]  # Voltatges crítics per a LiPo
target_days = []
target_dates = []

for target_v in target_voltages:
    idx = np.argmin(np.abs(extended_voltages - target_v))
    target_day = extended_days[idx]
    target_days.append(target_day)
    target_date = dates[0] + pd.Timedelta(days=target_day)
    target_dates.append(target_date)

# Crear gràfic millorat per a LiPo
plt.figure(figsize=(15, 10))

# Gràfic principal
plt.subplot(2, 2, 1)
plt.plot(days, voltages, 'o', markersize=10, color='darkblue', label="Mesures reals", zorder=5)

# Mostrar tots els models per comparar
colors = ['red', 'green', 'orange']
for i, (name, model) in enumerate(models_to_test.items()):
    model_extended = model['func'](extended_days, *model['params'])
    model_extended = np.clip(model_extended, LIPO_V_MIN, LIPO_V_MAX)
    
    fitted = model['func'](days, *model['params'])
    ss_res = np.sum((voltages - fitted) ** 2)
    ss_tot = np.sum((voltages - np.mean(voltages)) ** 2)
    r2 = 1 - (ss_res / ss_tot)
    
    plt.plot(extended_days, model_extended, '--', linewidth=2, color=colors[i % len(colors)], 
             label=f"{model['label']} (R²={r2:.3f})", alpha=0.7)

# Zones de voltatge per a LiPo
plt.axhspan(4.0, 4.2, alpha=0.1, color='green', label='Zona plena càrrega')
plt.axhspan(3.7, 4.0, alpha=0.1, color='yellow', label='Zona nominal')
plt.axhspan(3.3, 3.7, alpha=0.1, color='orange', label='Zona baixa')
plt.axhspan(3.0, 3.3, alpha=0.1, color='red', label='Zona crítica')

plt.xlabel("Dies des de la primera mesura")
plt.ylabel("Voltatge (V)")
plt.title(f"Anàlisi de descàrrega LiPo 30mAh - Millor model: {best_name}")
plt.ylim(2.8, 4.3)
plt.xlim(0, min(800, max(target_days[:-1]) * 1.2))  # Excloure el últim punt si és massa lluny
plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
plt.grid(True, alpha=0.3)

# Zoom als primers dies
plt.subplot(2, 2, 2)
zoom_days = min(max(days) * 2, 200)
zoom_mask = extended_days <= zoom_days
plt.plot(days, voltages, 'o', markersize=8, color='darkblue', label="Mesures reals")
plt.plot(extended_days[zoom_mask], extended_voltages[zoom_mask], '-', linewidth=2, 
         color='red', label=f"Predicció ({best_name})")

plt.xlabel("Dies des de la primera mesura")
plt.ylabel("Voltatge (V)")
plt.title("Zoom: Primers dies de predicció")
plt.ylim(3.5, 4.3)
plt.legend()
plt.grid(True, alpha=0.3)

# Prediccions de voltatges crítics
plt.subplot(2, 2, 3)
valid_predictions = [(v, d, date) for v, d, date in zip(target_voltages, target_days, target_dates) 
                    if d < max_prediction_days/2]  # Només prediccions raonables

if valid_predictions:
    voltages_plot, days_plot, dates_plot = zip(*valid_predictions)
    
    plt.bar(range(len(voltages_plot)), days_plot, 
            color=['green', 'yellow', 'orange', 'red', 'darkred', 'black'][:len(voltages_plot)],
            alpha=0.7)
    
    plt.xlabel('Voltatge objectiu (V)')
    plt.ylabel('Dies predits')
    plt.title('Prediccions de voltatges crítics per a LiPo')
    plt.xticks(range(len(voltages_plot)), [f'{v:.1f}V' for v in voltages_plot])
    
    # Afegir etiquetes amb dates
    for i, (days_pred, date_pred) in enumerate(zip(days_plot, dates_plot)):
        plt.text(i, days_pred + max(days_plot)*0.02, f'{date_pred.strftime("%d/%m/%Y")}', 
                ha='center', va='bottom', rotation=45, fontsize=8)

plt.grid(True, alpha=0.3)

# Característiques de la bateria LiPo
plt.subplot(2, 2, 4)
info_text = f"""
INFORMACIÓ DE LA BATERIA LiPo

Capacitat: 30mAh
Voltatge nominal: 3.7V
Voltatge màxim: 4.2V
Voltatge mínim segur: 3.0V

ESTAT ACTUAL:
Voltatge actual: {voltages[-1]:.3f}V
Última mesura: {dates[-1].strftime('%d/%m/%Y')}
Dies de funcionament: {max(days)}

PREDICCIONS:
Model utilitzat: {best_name}
Qualitat ajust (R²): {best_r2:.3f}

TEMPS DE VIDA RESTANT:
Fins a 3.5V: {target_days[3]:.0f} dies
Fins a 3.3V: {target_days[4]:.0f} dies
Fins a 3.0V: {target_days[5]:.0f} dies
"""

plt.text(0.05, 0.95, info_text, transform=plt.gca().transAxes, 
         verticalalignment='top', fontfamily='monospace', fontsize=10)
plt.axis('off')

plt.tight_layout()
plt.show()

# Resum detallat
print(f"\n{'='*60}")
print(f"ANÀLISI DETALLAT - BATERIA LiPo 30mAh")
print(f"{'='*60}")
print(f"Període d'anàlisi: {dates[0].strftime('%d/%m/%Y')} - {dates[-1].strftime('%d/%m/%Y')}")
print(f"Dies de funcionament: {max(days)}")
print(f"Voltatge inicial: {voltages[0]:.3f}V")
print(f"Voltatge actual: {voltages[-1]:.3f}V")
print(f"Caiguda total: {voltages[0] - voltages[-1]:.3f}V")
print(f"Velocitat mitjana de descàrrega: {(voltages[0] - voltages[-1]) / max(days) * 1000:.2f} mV/dia")

print(f"\nMODEL SELECCIONAT: {best_name}")
print(f"Qualitat de l'ajust (R²): {best_r2:.4f}")

print(f"\nPREDICCIONS DE VOLTATGES CRÍTICS:")
for i, (target_v, target_day, target_date) in enumerate(zip(target_voltages, target_days, target_dates)):
    if target_day < max_prediction_days/2:
        estado = ""
        if target_v >= 3.7:
            estado = "(Nominal)"
        elif target_v >= 3.3:
            estado = "(Baixa)"
        else:
            estado = "(Crítica)"
        print(f"  {target_v:.1f}V {estado}: {target_day:.0f} dies ({target_date.strftime('%d/%m/%Y')})")