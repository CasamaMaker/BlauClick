import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.ticker import MultipleLocator
from scipy.interpolate import make_interp_spline
import requests
from io import StringIO
from datetime import datetime
import logging
from typing import Tuple, Optional, Dict, Any
import scipy

# Configuració del logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class BatteryAnalyzer:
    """Classe per analitzar la descàrrega de bateries Li-ion."""
    
    def __init__(self, capacitat_nominal: float = 40.0, v_max: float = 4.2, v_min: float = 3.0):
        """
        Inicialitza l'analitzador de bateries.
        
        Args:
            capacitat_nominal: Capacitat nominal en mAh
            v_max: Voltatge màxim en V
            v_min: Voltatge mínim en V
        """
        self.capacitat_nominal = capacitat_nominal
        self.v_max = v_max
        self.v_min = v_min
        self.df = None
        
    def carregar_dades_google_sheets(self, sheet_url: str) -> pd.DataFrame:
        """
        Carrega i processa dades des de Google Sheets.
        
        Args:
            sheet_url: URL del full de càlcul
            
        Returns:
            DataFrame amb les dades processades
        """
        try:
            logger.info("Descarregant dades de Google Sheets...")
            response = requests.get(sheet_url, timeout=30)
            response.raise_for_status()
            
            df = pd.read_csv(StringIO(response.text))
            logger.info(f"Columnes disponibles: {df.columns.tolist()}")
            
            # Neteja i processament
            df = self._processar_dades(df)
            
            if df.empty:
                raise ValueError("No hi ha dades vàlides després de la neteja")
                
            logger.info(f"Dades processades correctament: {len(df)} registres")
            return df
            
        except Exception as e:
            logger.error(f"Error al carregar dades: {e}")
            return self._generar_dades_exemple()
    
    def _processar_dades(self, df: pd.DataFrame) -> pd.DataFrame:
        """
        Processa i neteja les dades del DataFrame.
        
        Args:
            df: DataFrame amb dades en brut
            
        Returns:
            DataFrame processat
        """
        # Crear còpia per evitar warnings
        df = df.copy()
        
        # Neteja columnes
        df.columns = df.columns.str.strip().str.lower()
        
        # Debug: mostrar primeres files
        logger.info(f"Primeres files del DataFrame:\n{df.head()}")
        
        # Verificar columnes necessàries
        columnes_requerides = ['dia', 'voltatge']
        if not all(col in df.columns for col in columnes_requerides):
            raise ValueError(f"Columnes requerides no trobades: {columnes_requerides}")
        
        # Seleccionar només les columnes necessàries
        df = df[columnes_requerides].copy()
        
        # Eliminar files amb valors NaN o buits
        df = df.dropna(subset=columnes_requerides)
        df = df[df['dia'].astype(str).str.strip() != '']
        df = df[df['voltatge'].astype(str).str.strip() != '']
        
        logger.info(f"Files després d'eliminar NaN: {len(df)}")
        
        # Convertir dates
        df['dia'] = pd.to_datetime(df['dia'], dayfirst=True, errors='coerce')
        
        # Convertir voltatges (gestionar comes i punts)
        df['voltatge'] = df['voltatge'].astype(str).str.replace(',', '.')
        df['voltatge'] = pd.to_numeric(df['voltatge'], errors='coerce')
        
        # Eliminar files amb conversions fallides
        df = df.dropna(subset=['dia', 'voltatge'])
        
        logger.info(f"Files després de conversions: {len(df)}")
        
        if df.empty:
            logger.warning("No queden dades després del processament")
            return df
        
        # Ordenar per data i calcular temps en dies
        df = df.sort_values('dia')
        df['temps_dies'] = (df['dia'] - df['dia'].min()).dt.days
        
        # Renombrar per consistència
        df = df.rename(columns={'voltatge': 'voltaje'})
        
        logger.info(f"Dades finals processades: {len(df)} registres")
        logger.info(f"Rang de dates: {df['dia'].min()} a {df['dia'].max()}")
        logger.info(f"Rang de voltatges: {df['voltaje'].min():.2f}V a {df['voltaje'].max():.2f}V")
        
        return df
    
    def _generar_dades_exemple(self) -> pd.DataFrame:
        """Genera dades d'exemple per a proves."""
        logger.info("Generant dades d'exemple...")
        temps_dies = np.linspace(0, 30, 15)
        voltages = [4.2, 4.15, 4.1, 4.05, 4.0, 3.95, 3.9, 3.8, 3.7, 3.6, 3.5, 3.4, 3.3, 3.1, 3.0]
        
        return pd.DataFrame({
            'temps_dies': temps_dies,
            'voltaje': voltages
        })
    
    def voltaje_a_soc(self, voltaje: float) -> float:
        """
        Converteix voltatge a percentatge de càrrega (SOC) amb corba realista Li-ion.
        
        Args:
            voltaje: Voltatge en V
            
        Returns:
            SOC en percentatge (0-100)
        """
        # Corba de descàrrega més realista per Li-ion
        # Basada en la corba característica de descàrrega de bateries Li-ion
        if voltaje >= 4.2:
            return 100.0
        elif voltaje >= 4.1:
            return 95.0 + (voltaje - 4.1) * 50  # 95-100%
        elif voltaje >= 4.0:
            return 80.0 + (voltaje - 4.0) * 150  # 80-95%
        elif voltaje >= 3.9:
            return 60.0 + (voltaje - 3.9) * 200  # 60-80%
        elif voltaje >= 3.8:
            return 40.0 + (voltaje - 3.8) * 200  # 40-60%
        elif voltaje >= 3.7:
            return 20.0 + (voltaje - 3.7) * 200  # 20-40%
        elif voltaje >= 3.6:
            return 10.0 + (voltaje - 3.6) * 100  # 10-20%
        elif voltaje >= 3.4:
            return 5.0 + (voltaje - 3.4) * 25    # 5-10%
        elif voltaje >= 3.0:
            return 0.0 + (voltaje - 3.0) * 12.5  # 0-5%
        else:
            return 0.0
    
    def calcular_soc_real(self, voltaje: float) -> float:
        """
        Calcula el SOC basat únicament en el voltatge, sense assumir capacitat.
        Utilitza una corba de descàrrega genèrica per a bateries Li-ion.
        """
        # Corba de descàrrega genèrica ajustada (4.2V-3.0V)
        if voltaje >= 4.15: return 95 + (voltaje-4.15)*100 
        elif voltaje >= 4.0: return 80 + (voltaje-4.0)*75
        elif voltaje >= 3.9: return 60 + (voltaje-3.9)*100
        elif voltaje >= 3.7: return 20 + (voltaje-3.7)*50
        elif voltaje >= 3.3: return 5 + (voltaje-3.3)*30
        else: return 0

    def estimar_capacitat_real(self, df: pd.DataFrame) -> float:
        """
        Estima la capacitat real basant-se en:
        1. Pendent de descàrrega (mV/dia)
        2. Corrents típics per bateries similars
        3. Fracció de voltatge descarregat
        """
        if len(df) < 3:
            return self.capacitat_nominal
        
        # 1. Calcular pendent de descàrrega (mV/dia)
        pendent = (df['voltaje'].iloc[-1] - df['voltaje'].iloc[0]) / \
                 (df['temps_dies'].iloc[-1] - df['temps_dies'].iloc[0])
        
        # 2. Determinar corrent típic basat en el pendent
        if pendent > -0.005:   corrent = 0.02  # Descàrrega molt lenta
        elif pendent > -0.01:  corrent = 0.05
        elif pendent > -0.02:  corrent = 0.1
        else:                  corrent = 0.2    # Descàrrega ràpida
        
        # 3. Estimar capacitat basant-se en la fracció de voltatge descarregat
        delta_volt_total = 1.2  # 4.2V -> 3.0V
        delta_volt_observat = df['voltaje'].iloc[0] - df['voltaje'].iloc[-1]
        fraccio_descarregada = delta_volt_observat / delta_volt_total
        
        self.temps_total = df['temps_dies'].iloc[-1] - df['temps_dies'].iloc[0]
        
        # if fraccio_descarregada > 0.05:  # Filtrar canvis mínims
        capacitat_estimada = (corrent * self.temps_total) / fraccio_descarregada
        print(">>>>>>>>--- ", capacitat_estimada)
        return max(20, min(100, capacitat_estimada))  # Forçar rang raonable
        
        # return self.capacitat_nominal
    
    def calcular_parametres_bateria(self, df: pd.DataFrame) -> Dict[str, Any]:
        """
        Calcula paràmetres de la bateria basats en les dades.
        
        Args:
            df: DataFrame amb dades de voltatge i temps
            
        Returns:
            Diccionari amb paràmetres calculats
        """
        if df.empty:
            logger.warning("DataFrame buit, utilitzant paràmetres per defecte")
            return self._parametres_per_defecte()
        
        # Crear còpia per evitar warnings
        df = df.copy()
        
        # Calcular SOC amb corba realista
        df['soc'] = df['voltaje'].apply(self.voltaje_a_soc)
        
        # # Calcular mAh consumits
        # df['mah_consumits'] = self.capacitat_nominal * (100 - df['soc']) / 100
        
        # # 1. Calcular SOC real (sense assumir capacitat)
        # df['soc'] = df['voltaje'].apply(self.calcular_soc_real)
        
        # 2. Estimar capacitat real
        self.capacitat_nominal = self.estimar_capacitat_real(df)

        
        # 3. Recalcular mAh basant-se en la capacitat estimada
        df['mah_consumits'] = self.capacitat_nominal * (100 - df['soc']) / 100


        # Calcular corrent de descàrrega entre punts consecutius
        corrents_instantanis = []
        for i in range(1, len(df)):
            delta_temps = df.iloc[i]['temps_dies'] - df.iloc[i-1]['temps_dies']
            delta_mah = df.iloc[i]['mah_consumits'] - df.iloc[i-1]['mah_consumits']
            
            if delta_temps > 0:
                corrent_instantani = delta_mah / delta_temps  # mA/dia
                corrents_instantanis.append(corrent_instantani)
            else:
                corrents_instantanis.append(0)
        
        # Afegir corrent instantani al DataFrame
        df['corrent_instantani'] = [0] + corrents_instantanis
        
        # Calcular corrent promig global
        if len(df) > 1:
            temps_total = df['temps_dies'].iloc[-1] - df['temps_dies'].iloc[0]
            mah_total = df['mah_consumits'].iloc[-1] - df['mah_consumits'].iloc[0]
            corrent_promig_global = mah_total / temps_total if temps_total > 0 else 0
        else:
            corrent_promig_global = 0
        
        # Calcular mètriques
        ultim_punt = df.iloc[-1]
        primer_punt = df.iloc[0]
        
        # Estimació del temps fins 0% basada en el corrent promig
        if corrent_promig_global > 0 and ultim_punt['soc'] > 0:
            mah_restants = ultim_punt['soc'] * self.capacitat_nominal / 100
            dies_restants = mah_restants / corrent_promig_global
            temps_0_percent = ultim_punt['temps_dies'] + dies_restants
        else:
            temps_0_percent = ultim_punt['temps_dies'] + 30  # Estimació per defecte
        
        # Capacitat real basada en la descàrrega observada
        if len(df) > 1:
            soc_inicial = primer_punt['soc']
            soc_final = ultim_punt['soc']
            temps_transcorregut = ultim_punt['temps_dies'] - primer_punt['temps_dies']
            
            if temps_transcorregut > 0 and soc_inicial > soc_final:
                percentatge_descarregat = soc_inicial - soc_final
                mah_teorics_consumits = self.capacitat_nominal * percentatge_descarregat / 100
                capacitat_real = self.capacitat_nominal  # Mantenim la nominal com a referència
            else:
                capacitat_real = self.capacitat_nominal
        else:
            capacitat_real = self.capacitat_nominal
        
        # Corrent promig filtrat (sense outliers)
        if len(corrents_instantanis) > 0:
            corrents_valids = [c for c in corrents_instantanis if 0 < c < 10]  # Filtrar valors extrems
            corrent_promig = np.mean(corrents_valids) if corrents_valids else corrent_promig_global
        else:
            corrent_promig = corrent_promig_global
        
        # Càlcul d'autonomia restant
        if corrent_promig > 0 and ultim_punt['soc'] > 0:
            autonomia_restant = (ultim_punt['soc'] * self.capacitat_nominal / 100) / corrent_promig
        else:
            autonomia_restant = 0
        
        return {
            'df': df,
            'ultim_punt': ultim_punt,
            'primer_punt': primer_punt,
            'temps_0_percent': temps_0_percent,
            'capacitat_real': capacitat_real,
            'corrent_promig': corrent_promig,
            'corrent_promig_global': corrent_promig_global,
            'autonomia_restant': autonomia_restant,
            'temps_total': df['temps_dies'].iloc[-1] - df['temps_dies'].iloc[0] if len(df) > 1 else 0
        }
    
    def _parametres_per_defecte(self) -> Dict[str, Any]:
        """Retorna paràmetres per defecte quan no hi ha dades."""
        return {
            'df': pd.DataFrame(),
            'ultim_punt': pd.Series({'soc': 100, 'mah_consumits': 0, 'temps_dies': 0}),
            'primer_punt': pd.Series({'soc': 100, 'mah_consumits': 0, 'temps_dies': 0}),
            'temps_0_percent': 30,
            'capacitat_real': self.capacitat_nominal,
            'corrent_promig': 1.0,
            'corrent_promig_global': 1.0,
            'autonomia_restant': 30,
            'temps_total': 0
        }
    
    def crear_model_prediccio(self, df: pd.DataFrame, temps_0_percent: float) -> Tuple[np.ndarray, np.ndarray]:
        """
        Crea un model de predicció per a la descàrrega.
        
        Args:
            df: DataFrame amb dades
            temps_0_percent: Temps estimat fins descàrrega completa
            
        Returns:
            Tuple amb arrays (x_smooth, y_smooth) per graficar
        """
        if len(df) > 3:
            # Extendre la predicció fins a 0%
            x_real = df['temps_dies'].tolist()
            y_real = df['voltaje'].tolist()
            
            # Afegir punt final a 3.0V si no hi és
            if temps_0_percent > df['temps_dies'].max():
                x_real.append(temps_0_percent)
                y_real.append(3.0)

                
            
            # Spline basat en dades reals
            x_real = np.array(x_real)
            y_real = np.array(y_real)

            spl = make_interp_spline(x_real, y_real, k=min(3, len(x_real)-1))
            
            x_smooth = np.linspace(1, temps_0_percent, 300)
            y_smooth = spl(x_smooth)
            
        else:
            # Model per defecte basat en corba Li-ion típica
            punts_model = [
                (0, 4.20), (0.1*temps_0_percent, 4.15),
                (0.2*temps_0_percent, 4.10), (0.3*temps_0_percent, 4.05),
                (0.4*temps_0_percent, 4.00), (0.5*temps_0_percent, 3.95),
                (0.6*temps_0_percent, 3.90), (0.7*temps_0_percent, 3.80),
                (0.8*temps_0_percent, 3.70), (0.9*temps_0_percent, 3.50),
                (0.95*temps_0_percent, 3.30), (temps_0_percent, 3.00)
            ]
            x_model = np.array([p[0] for p in punts_model])
            y_model = np.array([p[1] for p in punts_model])
            spl = make_interp_spline(x_model, y_model, k=3)
            x_smooth = np.linspace(0, temps_0_percent, 300)
            y_smooth = spl(x_smooth)
        
        return x_smooth, y_smooth
    
    def graficar_resultats(self, parametres: Dict[str, Any]) -> None:
        """
        Versió simplificada amb un sol gràfic que manté l'estil original però amb:
        - Requadre de paràmetres a la dreta
        - Només etiqueta per l'últim valor
        - Eix de temps cada 10 dies
        """
        # Configuració mantenint l'estil original
        plt.style.use('seaborn-darkgrid')
        fig, ax = plt.subplots(figsize=(14, 8))
        plt.title(f'Curva de Descàrrega [{self.temps_total} dies anàlisis] - Batería Li-ion [3.7V - {self.capacitat_nominal:.0f}mAh]', fontsize=16, pad=20)

        df = parametres['df']
        temps_0_percent = parametres['temps_0_percent']
        
        # Crear model de predicció (igual que abans)
        x_smooth, y_smooth = self.crear_model_prediccio(df, temps_0_percent)
        
        # 1. Gràfic de voltatge (únic gràfic)
        ax.plot(x_smooth, y_smooth, color='royalblue', linewidth=2.5, 
               label='Predicció ajustada', zorder=5)

        # Dades reals (només línia i punts)
        if not df.empty:
            # Punts de dades
            ax.scatter(df['temps_dies'], df['voltaje'], color='red', s=60, 
                      label='Dades reals', zorder=10)
            
            # Línia de tendència
            ax.plot(df['temps_dies'], df['voltaje'], 'r--', alpha=0.5, linewidth=1.5)
            
            # 2. ETIQUETA NOMÉS PER L'ÚLTIM VALOR (SOC)
            ultim_punt = df.iloc[-1]
            ax.annotate(f'{ultim_punt["soc"]:.1f}%', 
                       (ultim_punt['temps_dies'], ultim_punt['voltaje']),
                       xytext=(10, 10), textcoords='offset points',
                       fontsize=11, ha='center', va='bottom',
                       bbox=dict(boxstyle='round,pad=0.3', 
                                fc='yellow', alpha=0.8,
                                ec='gold', lw=1))

        # 3. EIX DE TEMPS CADA 10 DIES
        ax.xaxis.set_major_locator(MultipleLocator(25))  # Cada 10 dies
        ax.xaxis.set_minor_locator(MultipleLocator(5))
        
        # Configuració dels eixos
        ax.set_xlabel('Temps (dies)', fontsize=12)
        ax.set_ylabel('Voltatge (V)', fontsize=12)
        ax.set_ylim(2.8, 4.3)
        
        # Límits eix X basats en dades o predicció
        x_max = max(temps_0_percent, df['temps_dies'].max() if not df.empty else 30)
        ax.set_xlim(-1, x_max + 1)

        # 4. REQUADRE DE PARÀMETRES A LA DRETA (mantenint estil original)
        info_text = (
            f"PARÀMETRES BATERIA:\n"
            f"• Capacitat: {self.capacitat_nominal:.0f}mAh\n"
            f"• Descàrrega: {parametres['corrent_promig']:.2f}mA/dia\n"
            f"• Dies fins 0%: {parametres['temps_0_percent']:.0f}\n"
            f"• SOC actual: {ultim_punt['soc']:.1f}%\n"
            f"• Voltatge: {ultim_punt['voltaje']:.2f}V\n"
            f"• mAh usats: {ultim_punt['mah_consumits']:.1f}"
        )
        
        ax.text(0.86, 0.80, info_text, transform=ax.transAxes,
               fontsize=10, ha='left', va='top',
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.9))

        # Línies de referència (mantenint estil original)
        ax.axhline(y=3.0, color='red', linestyle='--', linewidth=1, 
                  label='Voltatge de tall (3.0V)')
        ax.axhline(y=3.7, color='orange', linestyle=':', linewidth=1, 
                  label='Voltatge nominal (3.7V)')
        
        # Graella i llegenda
        ax.grid(True, which='major', linestyle='--', linewidth=0.5)
        ax.grid(True, which='minor', linestyle=':', linewidth=0.3)
        ax.legend(loc='upper right')

        plt.tight_layout()
        plt.show()
    
    def _configurar_grafic_voltatge(self, ax: plt.Axes, parametres: Dict[str, Any]) -> None:
        """Configura el gràfic de voltatge."""
        df = parametres['df']
        temps_0_percent = parametres['temps_0_percent']
        capacitat_real = parametres['capacitat_real']
        corrent_promig = parametres['corrent_promig']
        autonomia_restant = parametres['autonomia_restant']
        ultim_punt = parametres['ultim_punt']
        
        # Etiquetes i límits
        ax.set_xlabel('Temps (dies)', fontsize=12)
        ax.set_ylabel('Voltatge (V)', fontsize=12)
        ax.set_ylim(2.8, 4.3)
        
        temps_max = max(temps_0_percent, df['temps_dies'].max() if not df.empty else 30)
        ax.set_xlim(-0.5, temps_max + 0.5)
        
        # Línies de referència amb colors més clars
        ax.axhline(y=3.0, color='red', linestyle='--', linewidth=2, 
                  label='Voltatge de tall (3.0V)', alpha=0.8)
        ax.axhline(y=3.7, color='orange', linestyle=':', linewidth=1, 
                  label='Voltatge nominal (3.7V)', alpha=0.6)
        ax.axvline(x=temps_0_percent, color='purple', linestyle=':', 
                  linewidth=2, label=f'Descàrrega completa (~{temps_0_percent:.1f} dies)')
        
        # Zones de voltatge
        ax.axhspan(4.0, 4.3, alpha=0.1, color='green', label='Zona alta (>4.0V)')
        ax.axhspan(3.5, 4.0, alpha=0.1, color='yellow', label='Zona mitjana (3.5-4.0V)')
        ax.axhspan(2.8, 3.5, alpha=0.1, color='red', label='Zona baixa (<3.5V)')
        
        # Informació tècnica millorada
        info_text = f"""PARÀMETRES DE LA BATERIA:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
• Capacitat nominal: {self.capacitat_nominal:.1f} mAh
• Corrent de descàrrega: {corrent_promig:.2f} mA/dia
• Temps fins 0%: {temps_0_percent:.1f} dies
• Autonomia restant: {autonomia_restant:.1f} dies

ESTAT ACTUAL:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
• SOC actual: {ultim_punt.get('soc', 0):.1f}%
• Voltatge actual: {ultim_punt.get('voltaje', 0):.2f}V
• mAh consumits: {ultim_punt.get('mah_consumits', 0):.1f} mAh
• mAh restants: {self.capacitat_nominal - ultim_punt.get('mah_consumits', 0):.1f} mAh
"""
        ax.text(0.72, 0.98, info_text, transform=ax.transAxes, 
                fontsize=10, ha='left', va='top', family='monospace',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='white', alpha=0.9, edgecolor='gray'))
        
        # Configurar eixos
        ax.xaxis.set_major_locator(MultipleLocator(5))
        ax.xaxis.set_minor_locator(MultipleLocator(1))
        ax.yaxis.set_major_locator(MultipleLocator(0.2))
        ax.yaxis.set_minor_locator(MultipleLocator(0.1))
        
        # Graella i llegenda
        ax.grid(True, which='major', linestyle='-', linewidth=0.5, alpha=0.7)
        ax.grid(True, which='minor', linestyle=':', linewidth=0.3, alpha=0.5)
        ax.legend(loc='center right', bbox_to_anchor=(1.0, 0.5))
    
    def _configurar_grafic_soc(self, ax: plt.Axes, parametres: Dict[str, Any]) -> None:
        """Configura el gràfic de SOC."""
        df = parametres['df']
        temps_0_percent = parametres['temps_0_percent']
        
        # Etiquetes i límits
        ax.set_xlabel('Temps (dies)', fontsize=12)
        ax.set_ylabel('SOC (%)', fontsize=12)
        ax.set_ylim(0, 105)
        
        temps_max = max(temps_0_percent, df['temps_dies'].max() if not df.empty else 30)
        ax.set_xlim(-0.5, temps_max + 0.5)
        
        # Línies de referència
        for percent in [25, 50, 75]:
            ax.axhline(y=percent, color='gray', linestyle='--', alpha=0.5, linewidth=0.5)
        
        # Zones de SOC
        ax.axhspan(80, 100, alpha=0.1, color='green')
        ax.axhspan(20, 80, alpha=0.1, color='yellow')
        ax.axhspan(0, 20, alpha=0.1, color='red')
        
        # Configurar eixos
        ax.xaxis.set_major_locator(MultipleLocator(5))
        ax.xaxis.set_minor_locator(MultipleLocator(1))
        ax.yaxis.set_major_locator(MultipleLocator(20))
        ax.yaxis.set_minor_locator(MultipleLocator(10))
        
        # Graella i llegenda
        ax.grid(True, which='major', linestyle='-', linewidth=0.5, alpha=0.7)
        ax.grid(True, which='minor', linestyle=':', linewidth=0.3, alpha=0.5)
        ax.legend(loc='upper right')
    
    def mostrar_resum(self, parametres: Dict[str, Any]) -> None:
        """
        Mostra un resum detallat dels resultats de l'anàlisi.
        
        Args:
            parametres: Diccionari amb paràmetres calculats
        """
        capacitat_real = parametres['capacitat_real']
        temps_0_percent = parametres['temps_0_percent']
        corrent_promig = parametres['corrent_promig']
        autonomia_restant = parametres['autonomia_restant']
        ultim_punt = parametres['ultim_punt']
        primer_punt = parametres['primer_punt']
        temps_total = parametres['temps_total']
        df = parametres['df']
        
        print("\n" + "="*60)
        print("RESUM DETALLAT DE L'ANÀLISI DE LA BATERIA")
        print("="*60)
        print(f"Model: 501012 Bateria Li-ion [3.7V - {self.capacitat_nominal:.0f}mAh]")
        print("-"*60)
        
        print("\n📊 PARÀMETRES PRINCIPALS:")
        print(f"   • Capacitat nominal: {self.capacitat_nominal:.1f} mAh")
        print(f"   • Corrent de descàrrega promig: {corrent_promig:.2f} mA/dia")
        print(f"   • Temps total d'anàlisi: {temps_total:.1f} dies")
        print(f"   • Temps estimat fins descàrrega completa: {temps_0_percent:.1f} dies")
        
        print("\n🔋 ESTAT DE LA BATERIA:")
        if isinstance(ultim_punt, pd.Series):
            print(f"   • SOC inicial: {primer_punt.get('soc', 0):.1f}%")
            print(f"   • SOC actual: {ultim_punt.get('soc', 0):.1f}%")
            print(f"   • Voltatge actual: {ultim_punt.get('voltaje', 0):.2f}V")
            print(f"   • mAh consumits: {ultim_punt.get('mah_consumits', 0):.1f} mAh")
            print(f"   • mAh restants: {self.capacitat_nominal - ultim_punt.get('mah_consumits', 0):.1f} mAh")
            print(f"   • Autonomia restant estimada: {autonomia_restant:.1f} dies")
        
        print(f"\n📈 DADES D'ANÀLISI:")
        print(f"   • Nombre de mesures: {len(df)}")
        
        if len(df) > 0:
            print(f"   • Rang de dates: {df['dia'].min().strftime('%d/%m/%Y') if 'dia' in df.columns else 'N/A'}")
            print(f"     fins {df['dia'].max().strftime('%d/%m/%Y') if 'dia' in df.columns else 'N/A'}")
            print(f"   • Rang de voltatges: {df['voltaje'].min():.2f}V - {df['voltaje'].max():.2f}V")
            print(f"   • Rang de SOC: {df['soc'].min():.1f}% - {df['soc'].max():.1f}%")
        
        # Mostrar taula de dades
        if 0 < len(df) <= 15:
            print("\n📋 DADES PROCESSADES:")
            print("-"*80)
            columnes_mostrar = ['temps_dies', 'voltaje', 'soc', 'mah_consumits', 'corrent_instantani']
            df_mostrar = df[columnes_mostrar].copy()
            df_mostrar.columns = ['Dies', 'Voltatge(V)', 'SOC(%)', 'mAh Consumits', 'Corrent(mA/dia)']
            print(df_mostrar.round(2).to_string(index=False))
        elif len(df) > 15:
            print(f"\n📋 MOSTRA DE DADES (primeres 5 i últimes 5 de {len(df)} total):")
            print("-"*80)
            columnes_mostrar = ['temps_dies', 'voltaje', 'soc', 'mah_consumits', 'corrent_instantani']
            df_mostrar = pd.concat([df.head(5), df.tail(5)])[columnes_mostrar]
            df_mostrar.columns = ['Dies', 'Voltatge(V)', 'SOC(%)', 'mAh Consumits', 'Corrent(mA/dia)']
            print(df_mostrar.round(2).to_string(index=False))
            print("\n... (mostrant només primeres i últimes files)")
        
        print("\n" + "="*60)
        print("FI DEL RESUM")
        print("="*60)


    def executar_analisi(self, sheet_url: str) -> None:
        """
        Executa tot el flux d'anàlisi des de la càrrega de dades fins a la visualització.
        
        Args:
            sheet_url: URL del full de càlcul de Google Sheets
        """
        try:
            # 1. Carregar i processar dades
            df = self.carregar_dades_google_sheets(sheet_url)
            
            # 2. Calcular paràmetres de la bateria
            parametres = self.calcular_parametres_bateria(df)
            
            # 3. Mostrar resum per terminal
            self.mostrar_resum(parametres)
            
            # 4. Generar gràfics
            self.graficar_resultats(parametres)
            
        except Exception as e:
            logger.error(f"Error durant l'execució de l'anàlisi: {e}")
            raise

if __name__ == "__main__":
    # Configuració inicial
    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger(__name__)
    
    try:
        # URL del Google Sheet (públic o accessible)
        sheet_url = "https://docs.google.com/spreadsheets/d/1WHqHPniyA06FTHolcYKQ4djUFw6aU1ZD4E3WRfT_PN0/export?format=csv&gid=1518918342"
        
        # Crear analitzador amb paràmetres per defecte
        analitzador = BatteryAnalyzer(
            capacitat_nominal=30.0,  # 40mAh
            v_max=4.2,              # Voltatge màxim
            v_min=3.0               # Voltatge mínim
        )
        
        # Executar l'anàlisi complet
        analitzador.executar_analisi(sheet_url)
        
    except Exception as e:
        logger.error(f"Error no controlat: {e}")
        print("\n⚠️ S'ha produït un error durant l'execució. Revisa els logs per a més detalls.")
        print("Assegura't que:")
        print("1. L'URL del Google Sheet és correcte i accessible")
        print("2. El format de les dades és el correcte (columnes 'dia' i 'voltatge')")
        print("3. Tens accés a internet per descarregar les dades")