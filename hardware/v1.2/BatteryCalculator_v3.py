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
print(f"Voltatges processats: {len(voltages)}")

# Verificar que tenim dades suficients
if len(dates) < 3:
    print("Error: No hi ha prou dades vàlides per fer la predicció")
    exit()

# Convertim les dates en dies des de la primera mesura
days = np.array([(d - dates[0]).days for d in dates])
voltages = np.array(voltages)

# Paràmetres específics per a LiPo de 30mAh
LIPO_V_MAX = 4.2  # Voltatge màxim de LiPo
LIPO_V_MIN = 3.0   # Voltatge mínim segur
LIPO_V_NOMINAL = 3.7  # Voltatge nominal

# Funció per calcular el percentatge de bateria segons temps d'ús
def calculate_battery_percentage(current_day, total_life_days):
    """Calcula el percentatge de bateria restant segons el temps d'ús"""
    if current_day >= total_life_days:
        return 0
    else:
        return max(0, 100 - (current_day / total_life_days) * 100)

# Model realista per a bateria LiPo
def lipo_discharge_model(t, V_max, k_linear, k_exp, t_transition):
    """Model específic per a bateries LiPo"""
    V = np.zeros_like(t)
    
    # Fase 1: descàrrega quasi-lineal
    mask1 = t <= t_transition
    V[mask1] = V_max - k_linear * t[mask1]
    
    # Fase 2: caiguda exponencial ràpida
    mask2 = t > t_transition
    V_transition = V_max - k_linear * t_transition
    V[mask2] = V_transition * np.exp(-k_exp * (t[mask2] - t_transition))
    
    return V

# Ajust del model LiPo realista
try:
    popt_lipo, _ = curve_fit(lipo_discharge_model, days, voltages, 
                            p0=[max(voltages), 0.001, 0.01, max(days)*2], 
                            bounds=([4.0, 0.0001, 0.001, max(days)], 
                                   [4.3, 0.01, 0.1, max(days)*5]))
    
    # Calcular R²
    fitted_values = lipo_discharge_model(days, *popt_lipo)
    ss_res = np.sum((voltages - fitted_values) ** 2)
    ss_tot = np.sum((voltages - np.mean(voltages)) ** 2)
    r2 = 1 - (ss_res / ss_tot)
    
    print(f"Model LiPo Realista: R² = {r2:.4f}")
    
except:
    print("Error ajustant model LiPo realista")
    exit()

# Generar predicció
max_prediction_days = 2000
extended_days = np.linspace(0, max_prediction_days, max_prediction_days)
extended_voltages = lipo_discharge_model(extended_days, *popt_lipo)
extended_voltages = np.clip(extended_voltages, LIPO_V_MIN, LIPO_V_MAX)

# Trobar prediccions per a voltatges crítics
target_voltages = [4.0, 3.8, 3.7, 3.5, 3.3, 3.0]
target_days = []
target_dates = []

for target_v in target_voltages:
    idx = np.argmin(np.abs(extended_voltages - target_v))
    target_day = extended_days[idx]
    target_days.append(target_day)
    target_date = dates[0] + pd.Timedelta(days=target_day)
    target_dates.append(target_date)

# Crear gràfic amb informació
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 8))

# Gràfic principal
ax1.plot(days, voltages, 'o', markersize=10, color='darkblue', label="Mesures reals", zorder=5)
ax1.plot(extended_days, extended_voltages, '--', linewidth=2, color='orange', 
         label=f"Model LiPo Realista (R²={r2:.3f})", alpha=0.8)

# Calcular vida útil total estimada (fins a 3.0V)
total_life_days = target_days[5]  # Dies fins a 3.0V

# Zones de voltatge per a LiPo
zones = [
    (4.0, 4.2, 'green', 'Zona plena càrrega'),
    (3.7, 4.0, 'yellow', 'Zona nominal'),
    (3.3, 3.7, 'orange', 'Zona baixa'),
    (3.0, 3.3, 'red', 'Zona crítica')
]

for v_min, v_max, color, label in zones:
    ax1.axhspan(v_min, v_max, alpha=0.15, color=color, label=label)

# Afegir línies de referència amb voltatges
reference_voltages = [4.2, 4.0, 3.8, 3.7, 3.5, 3.3, 3.0]
for v in reference_voltages:
    ax1.axhline(y=v, color='gray', linestyle=':', alpha=0.3)
    ax1.text(ax1.get_xlim()[1] * 0.02, v + 0.02, f'{v:.1f}V', 
             fontsize=9, color='gray')

# Afegir línies verticals de percentatge cada 20%
percentage_marks = [100, 80, 60, 40, 20, 0]
for pct in percentage_marks:
    # Calcular el dia corresponent a aquest percentatge
    day_at_percentage = (100 - pct) / 100 * total_life_days
    
    if day_at_percentage <= ax1.get_xlim()[1]:
        # Trobar el voltatge corresponent a aquest dia
        idx = np.argmin(np.abs(extended_days - day_at_percentage))
        voltage_at_day = extended_voltages[idx]
        
        # Línea vertical
        ax1.axvline(x=day_at_percentage, color='blue', linestyle='--', alpha=0.6, linewidth=1)
        
        # Etiqueta a la part superior
        ax1.text(day_at_percentage, ax1.get_ylim()[1] - 0.1, f'{pct}%', 
                ha='center', va='top', fontsize=10, color='blue', fontweight='bold',
                bbox=dict(boxstyle='round,pad=0.3', facecolor='white', alpha=0.8))

ax1.set_xlabel("Dies des de la primera mesura")
ax1.set_ylabel("Voltatge (V)")
ax1.set_title("Anàlisi de descàrrega LiPo 30mAh - Model Realista")
ax1.set_ylim(2.8, 4.3)
ax1.set_xlim(0, min(800, max(target_days[:-1]) * 1.2))

# Crear una segona escala X per als percentatges
ax1_twin = ax1.twiny()
ax1_twin.set_xlim(ax1.get_xlim())
# Etiquetes de percentatge a la part superior
pct_positions = []
pct_labels = []
for pct in [100, 80, 60, 40, 20, 0]:
    day_at_pct = (100 - pct) / 100 * total_life_days
    if day_at_pct <= ax1.get_xlim()[1]:
        pct_positions.append(day_at_pct)
        pct_labels.append(f'{pct}%')

ax1_twin.set_xticks(pct_positions)
ax1_twin.set_xticklabels(pct_labels)
ax1_twin.set_xlabel("Percentatge de bateria restant", color='blue')
ax1_twin.tick_params(axis='x', colors='blue')

ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
ax1.grid(True, alpha=0.3)

# Informació de la bateria
current_percentage = calculate_battery_percentage(max(days), total_life_days)
info_text = f"""
INFORMACIÓ DE LA BATERIA LiPo

Capacitat: 30mAh
Voltatge nominal: 3.7V
Voltatge màxim: 4.2V
Voltatge mínim segur: 3.0V

ESTAT ACTUAL:
Voltatge actual: {voltages[-1]:.3f}V
Percentatge restant: {current_percentage:.0f}%
Última mesura: {dates[-1].strftime('%d/%m/%Y')}
Dies de funcionament: {max(days)}

PREDICCIONS:
Model utilitzat: LiPo Realista
Qualitat ajust (R²): {r2:.3f}
Vida útil total estimada: {total_life_days:.0f} dies

TEMPS DE VIDA RESTANT:
Fins a 3.5V: {target_days[3]:.0f} dies ({calculate_battery_percentage(target_days[3], total_life_days):.0f}% restant)
Fins a 3.3V: {target_days[4]:.0f} dies ({calculate_battery_percentage(target_days[4], total_life_days):.0f}% restant)
Fins a 3.0V: {target_days[5]:.0f} dies (0% restant)

VELOCITAT DE DESCÀRREGA:
Mitjana: {(voltages[0] - voltages[-1]) / max(days) * 1000:.2f} mV/dia
Caiguda total: {voltages[0] - voltages[-1]:.3f}V
"""

ax2.text(0.05, 0.95, info_text, transform=ax2.transAxes, 
         verticalalignment='top', fontfamily='monospace', fontsize=11)
ax2.axis('off')

plt.tight_layout()
plt.show()

# Resum detallat
print(f"\n{'='*60}")
print(f"ANÀLISI DETALLAT - BATERIA LiPo 30mAh")
print(f"{'='*60}")
print(f"Període d'anàlisi: {dates[0].strftime('%d/%m/%Y')} - {dates[-1].strftime('%d/%m/%Y')}")
print(f"Dies de funcionament: {max(days)}")
print(f"Voltatge inicial: {voltages[0]:.3f}V ({calculate_battery_percentage(0, total_life_days):.0f}%)")
print(f"Voltatge actual: {voltages[-1]:.3f}V ({calculate_battery_percentage(max(days), total_life_days):.0f}%)")
print(f"Caiguda total: {voltages[0] - voltages[-1]:.3f}V")
print(f"Velocitat mitjana de descàrrega: {(voltages[0] - voltages[-1]) / max(days) * 1000:.2f} mV/dia")

print(f"\nMODEL SELECCIONAT: LiPo Realista")
print(f"Qualitat de l'ajust (R²): {r2:.4f}")
print(f"Vida útil total estimada: {total_life_days:.0f} dies")

print(f"\nPREDICCIONS DE VOLTATGES CRÍTICS:")
for i, (target_v, target_day, target_date) in enumerate(zip(target_voltages, target_days, target_dates)):
    if target_day < max_prediction_days/2:
        percentage_remaining = calculate_battery_percentage(target_day, total_life_days)
        estado = ""
        if target_v >= 3.7:
            estado = "(Nominal)"
        elif target_v >= 3.3:
            estado = "(Baixa)"
        else:
            estado = "(Crítica)"
        print(f"  {target_v:.1f}V ({percentage_remaining:.0f}% restant) {estado}: {target_day:.0f} dies ({target_date.strftime('%d/%m/%Y')})")