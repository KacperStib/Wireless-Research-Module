"""
Skrypt do generowania wykresu PER (Packet Error Rate) vs Odległość
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# =============================================================================
# KONFIGURACJA - WSPÓŁRZĘDNE STACJI
# =============================================================================
#pole
STACJA_LAT= 50.33091213361291
STACJA_LON = 18.561157982799322

# =============================================================================
# 1. WCZYTANIE DANYCH
# =============================================================================
# Zakładam, że plik nazywa się DATA.CSV
df = pd.read_csv('PER.CSV')

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

plt.title('Zależność utraconych pakietów od odległości od stacji bazowej', fontsize=20)
plt.xlabel('Odległość [metry]', fontsize=20)
plt.ylabel('PL (Packet Loss)', fontsize=20)
plt.legend(fontsize=20)
plt.grid(True, linestyle='--', alpha=0.5)

# Zapis do pliku
plt.savefig('wykres_per_dystans.png', dpi=300, bbox_inches='tight')
print("Wykres utraconych pakietów vs Odległość został zapisany jako: wykres_per_dystans.png")
plt.show()