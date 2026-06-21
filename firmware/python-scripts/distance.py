"""
Skrypt do generowania wykresu RSSI vs Odległość
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# =============================================================================
# KONFIGURACJA - WPISZ WSPÓŁRZĘDNE STACJI
# =============================================================================
STACJA_LAT = 50.32378287488028
STACJA_LON = 18.57256764033666
# =============================================================================
# 1. WCZYTANIE I PRZYGOTOWANIE DANYCH
# =============================================================================
df_raw = pd.read_csv('EVENTS.CSV')
df_raw.rename(columns={'Lat': 'lat', 'Lon': 'lon', 'RSSI': 'rssi', 'Tech': 'tech'}, inplace=True)

# Tylko RX z GPS fixem
df = df_raw[(df_raw['Dir'] == 'RX') & (df_raw['GPS'] == 'Y')].copy()

# =============================================================================
# 2. OBLICZENIA (Wzór Haversine)
# =============================================================================
def haversine(lat1, lon1, lat2, lon2):
    R = 6371000  # Ziemia w metrach
    phi1, phi2 = np.radians(lat1), np.radians(lat2)
    dphi = np.radians(lat2 - lat1)
    dlambda = np.radians(lon2 - lon1)
    a = np.sin(dphi/2)**2 + np.cos(phi1)*np.cos(phi2)*np.sin(dlambda/2)**2
    return R * 2 * np.arctan2(np.sqrt(a), np.sqrt(1-a))

df['dist_m'] = haversine(df['lat'], df['lon'], STACJA_LAT, STACJA_LON)

# =============================================================================
# 3. GENEROWANIE WYKRESU
# =============================================================================
plt.figure(figsize=(12, 6))

for tech in df['tech'].unique():
    subset = df[df['tech'] == tech]
    plt.scatter(subset['dist_m'], subset['rssi'], label=tech, alpha=0.6, s=40)

plt.title('Zależność RSSI od odległości od stacji bazowej', fontsize = 20)
plt.xlabel('Odległość [metry]', fontsize = 20)
plt.ylabel('RSSI [dBm]', fontsize = 20)
plt.legend(fontsize = 20)
plt.grid(True, which='both', linestyle='--', alpha=0.5)

# Zapis do pliku
plt.savefig('wykres_rssi_dystans.png', dpi=300, bbox_inches='tight')
print("Wykres RSSI vs Odległość został zapisany jako: wykres_rssi_dystans.png")
plt.show()