"""
Skrypt do generowania wykresu PER (Packet Error Rate) vs Odległość
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# =============================================================================
# KONFIGURACJA - WSPÓŁRZĘDNE STACJI
# =============================================================================
STACJA_LAT = 50.323920527298405
STACJA_LON = 18.57283895677652

# =============================================================================
# 1. WCZYTANIE DANYCH
# =============================================================================
# Zakładam, że plik nazywa się DATA.CSV
df = pd.read_csv('DATA.CSV')

# =============================================================================
# 2. OBLICZENIA (Wzór Haversine)
# =============================================================================
def haversine(lat1, lon1, lat2, lon2):
    R = 6371000  # Promień Ziemi w metrach
    phi1, phi2 = np.radians(lat1), np.radians(lat2)
    dphi = np.radians(lat2 - lat1)
    dlambda = np.radians(lon2 - lon1)
    a = np.sin(dphi/2)**2 + np.cos(phi1)*np.cos(phi2)*np.sin(dlambda/2)**2
    return R * 2 * np.arctan2(np.sqrt(a), np.sqrt(1-a))

df['dist_m'] = haversine(df['Lat'], df['Lon'], STACJA_LAT, STACJA_LON)

# =============================================================================
# 3. GENEROWANIE WYKRESU
# =============================================================================
plt.figure(figsize=(12, 6))

for tech in df['Tech'].unique():
    subset = df[df['Tech'] == tech]
    plt.scatter(subset['dist_m'], subset['PER'], label=tech, alpha=0.6, s=40)

plt.title('Zależność PER od odległości od stacji bazowej', fontsize=18)
plt.xlabel('Odległość [metry]', fontsize=14)
plt.ylabel('PER (Packet Error Rate)', fontsize=14)
plt.legend(title="Technologia")
plt.grid(True, linestyle='--', alpha=0.5)

# Zapis do pliku
plt.savefig('wykres_per_dystans.png', dpi=300, bbox_inches='tight')
print("Wykres PER vs Odległość został zapisany jako: wykres_per_dystans.png")
plt.show()