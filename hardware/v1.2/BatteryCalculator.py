import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
import pandas as pd

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

# Mostrem les dades resultants
print("dates_str =", dates_str)
print("voltages =", voltages)

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
print("Dates vàlides:", dates_str_valid)
print("Voltatges vàlids:", voltages_valid)

# Convertir les dates (ara manejant el format amb hora)
dates = []
for d in dates_str_valid:
    try:
        # Intentar amb format que inclou hora
        if ' ' in d:
            date_part = d.split()[0]  # Agafar només la part de la data
            dates.append(datetime.strptime(date_part, "%d/%m/%Y"))
        else:
            # Si no té hora, usar el format simple
            dates.append(datetime.strptime(d, "%d/%m/%Y"))
    except ValueError as e:
        print(f"Error convertint data '{d}': {e}")
        continue

# Usar només els voltatges que corresponen a dates vàlides
voltages = voltages_valid[:len(dates)]

print(f"\nDates processades: {len(dates)}")
print(f"Voltatges processats: {len(voltages)}")

# Verificar que tenim dades suficients
if len(dates) < 2:
    print("Error: No hi ha prou dades vàlides per fer la predicció")
    exit()

# Convertim les dates en dies des de la primera mesura
days = np.array([(d - dates[0]).days for d in dates])

# Ajust logarítmic i exponencial per veure quin encaixa millor
# Prova amb ajust lineal, després ajust exponencial
coeffs_exp = np.polyfit(days, np.log(voltages), 1)
a_exp, b_exp = coeffs_exp

# Funció exponencial ajustada: V(t) = exp(a*t + b)
fit_exp = np.exp(a_exp * days + b_exp)

# Predicció: quan arribarà a 3.5 V?
target_voltage = 3.5
target_day_exp = (np.log(target_voltage) - b_exp) / a_exp

# Corregim l'error: només accelerarem la baixada si hi ha algun valor sota 3.3V
def voltage_model(days):
    """Model de voltatge amb descàrrega suau i després sobtada."""
    v = np.exp(a_exp * days + b_exp)
    drop_zone = v < 3.3
    if np.any(drop_zone):
        start_day = days[drop_zone][0]
        v[drop_zone] = 3.3 * np.exp(-0.02 * (days[drop_zone] - start_day))
    return v

# Tornem a generar dies i voltatges fins a dia 1000
extended_days = np.linspace(0, 1200, 1200)
extended_voltages = voltage_model(extended_days)

# Gràfic amb baixada sobtada cap a 0V
plt.figure(figsize=(10, 6))
plt.plot(days, voltages, 'o', label="Mesures reals")
plt.plot(extended_days, extended_voltages, '--', label="Predicció amb baixada sobtada")
plt.axhline(3.5, color='red', linestyle=':', label="Límit 3.5V")
plt.axvline(target_day_exp, color='orange', linestyle=':', label=f"Dia 3.5V: {target_day_exp:.0f}")
plt.xlabel("Dies des de la primera mesura")
plt.ylabel("Voltatge (V)")
plt.title("Predicció de descàrrega amb caiguda sobtada")
plt.ylim(2.5, 4.1)
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()

# Informació adicional
print(f"\nResultats de la predicció:")
print(f"Coeficients exponencials: a={a_exp:.6f}, b={b_exp:.6f}")
print(f"Dia previst per arribar a 3.5V: {target_day_exp:.1f} dies")
print(f"Data aproximada: {(dates[0] + pd.Timedelta(days=target_day_exp)).strftime('%d/%m/%Y')}")
