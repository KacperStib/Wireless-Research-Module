"""
Skrypt do generowania wykresów RSSI i SNR vs Odległość
dla DWÓCH plików, tylko dane LoRa, każdy na jednym wspólnym wykresie.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# =============================================================================
# KONFIGURACJA - WPISZ WSPÓŁRZĘDNE STACJI
# =============================================================================
#STACJA_LAT = 50.32378287488028
#STACJA_LON = 18.57256764033666

#pplot
#STACJA_LAT=50.323920527298405
#STACJA_LON =18.57283895677652

#POLE
#STACJA_LAT= 50.33091213361291
#STACJA_LON = 18.561157982799322

#OSIEDLE
#STACJA_LAT=50.30064674255885
#STACJA_LON =18.64296829817275

#MASZT
STACJA_LAT = 50.32369084758339
STACJA_LON = 18.572302242811713

# =============================================================================
# KONFIGURACJA - WPISZ ŚCIEŻKI DO DWÓCH PLIKÓW CSV
# =============================================================================
PLIK_1 = 'EVENTS_DOL.CSV'
PLIK_2 = 'EVENTS_HIGH.CSV'

# Etykiety, które pojawią się w legendzie (domyślnie nazwa pliku bez rozszerzenia)
ETYKIETA_1 = 'Stacja bazowa (1 m AGL)'
ETYKIETA_2 = 'Stacja bazowa (5 m AGL)'

# =============================================================================
# FUNKCJE POMOCNICZE
# =============================================================================
def haversine(lat1, lon1, lat2, lon2):
    R = 6371000  # Ziemia w metrach
    phi1, phi2 = np.radians(lat1), np.radians(lat2)
    dphi = np.radians(lat2 - lat1)
    dlambda = np.radians(lon2 - lon1)
    a = np.sin(dphi / 2)**2 + np.cos(phi1) * np.cos(phi2) * np.sin(dlambda / 2)**2
    return R * 2 * np.arctan2(np.sqrt(a), np.sqrt(1 - a))


def wczytaj_lora(sciezka):
    """Wczytuje plik CSV, filtruje tylko RX + GPS fix + technologia LoRa, liczy dystans."""
    df_raw = pd.read_csv(sciezka)
    df_raw.rename(columns={'Lat': 'lat', 'Lon': 'lon', 'RSSI': 'rssi', 'SNR': 'snr', 'Tech': 'tech'}, inplace=True)

    df = df_raw[(df_raw['Dir'] == 'RX') & (df_raw['GPS'] == 'Y')].copy()

    # Filtr tylko LoRa (bez rozróżniania wielkości liter, na wypadek "LoRa"/"LORA"/"Lora")
    df = df[df['tech'].astype(str).str.contains('lora', case=False, na=False)].copy()

    df['dist_m'] = haversine(df['lat'], df['lon'], STACJA_LAT, STACJA_LON)
    return df


# =============================================================================
# WCZYTANIE OBU PLIKÓW
# =============================================================================
df1 = wczytaj_lora(PLIK_1)
df2 = wczytaj_lora(PLIK_2)

for etykieta, df in [(ETYKIETA_1, df1), (ETYKIETA_2, df2)]:
    if df.empty:
        print(f"UWAGA: brak danych LoRa (RX, GPS=Y) w pliku '{etykieta}'")
        continue
    najdalszy = df.loc[df['dist_m'].idxmax()]
    print(f"\n=== {etykieta} ===")
    print(f"  - Liczba punktów LoRa: {len(df)}")
    print(f"  - Najdalsza odległość: {najdalszy['dist_m']:.2f} m")
    print(f"  - RSSI w tym punkcie: {najdalszy['rssi']} dBm")
    if 'snr' in df.columns:
        print(f"  - SNR w tym punkcie: {najdalszy['snr']} dB")

# =============================================================================
# WYKRES 1 - RSSI, OBA PLIKI NA JEDNYM WYKRESIE
# =============================================================================
plt.figure(figsize=(12, 6))

plt.scatter(df1['dist_m'], df1['rssi'], label=ETYKIETA_1, alpha=0.6, s=40, color='tab:blue')
plt.scatter(df2['dist_m'], df2['rssi'], label=ETYKIETA_2, alpha=0.6, s=40, color='tab:red')

plt.title('Zależność RSSI od odległości od stacji bazowej', fontsize=20)
plt.xlabel('Odległość [metry]', fontsize=20)
plt.ylabel('RSSI [dBm]', fontsize=20)
plt.legend(fontsize=20)
plt.grid(True, which='both', linestyle='--', alpha=0.5)

plt.savefig('wykres_lora_rssi_porownanie.png', dpi=300, bbox_inches='tight')
print("\nWykres RSSI zapisany jako: wykres_lora_rssi_porownanie.png")

# =============================================================================
# WYKRES 2 - SNR, OBA PLIKI NA JEDNYM WYKRESIE
# =============================================================================
plt.figure(figsize=(12, 6))

plt.scatter(df1['dist_m'], df1['snr'], label=ETYKIETA_1, alpha=0.6, s=40, color='tab:blue')
plt.scatter(df2['dist_m'], df2['snr'], label=ETYKIETA_2, alpha=0.6, s=40, color='tab:red')

plt.title('Zależność SNR od odległości od stacji bazowej', fontsize=20)
plt.xlabel('Odległość [metry]', fontsize=20)
plt.ylabel('SNR [dB]', fontsize=20)
plt.legend(fontsize=20)
plt.grid(True, which='both', linestyle='--', alpha=0.5)

plt.savefig('wykres_lora_snr_porownanie.png', dpi=300, bbox_inches='tight')
print("Wykres SNR zapisany jako: wykres_lora_snr_porownanie.png")

plt.show()