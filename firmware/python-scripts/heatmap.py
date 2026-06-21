"""
Skrypt do generowania mapy heatmapy z danych pomiarowych RSSI
Autor: [Twoje Imię i Nazwisko]
"""

import pandas as pd
import folium
from folium.plugins import HeatMap
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# =============================================================================
# 1. WCZYTANIE DANYCH
# =============================================================================

# Wczytanie danych z pliku CSV
df_raw = pd.read_csv('EVENTS.CSV')

# Ujednolicenie nazw kolumn
df_raw.rename(columns={'Lat': 'lat', 'Lon': 'lon', 'RSSI': 'rssi', 'Tech': 'tech'}, inplace=True)

# Tylko RX z GPS fixem
df = df_raw[(df_raw['Dir'] == 'RX') & (df_raw['GPS'] == 'Y')].copy()

print("=== PODSTAWOWE INFORMACJE O DANYCH ===")
print(f"Wszystkich rekordów w pliku: {len(df_raw)}")
print(f"Rekordów RX z GPS fixem: {len(df)}")
print(f"Technologie: {df['tech'].unique()}")
print("\nPrzykładowe rekordy:")
print(df.head())

if len(df) == 0:
    print("BŁĄD: Brak danych RX z GPS fixem! Sprawdź plik CSV.")
    exit()

# Statystyki RSSI
print(f"\n=== STATYSTYKI RSSI ===")
print(f"Średni RSSI: {df['rssi'].mean():.2f} dBm")
print(f"Minimalny RSSI: {df['rssi'].min()} dBm")
print(f"Maksymalny RSSI: {df['rssi'].max()} dBm")

# Statystyki per technologia
for tech in df['tech'].unique():
    df_t = df[df['tech'] == tech]
    print(f"\n  [{tech}] pomiarów: {len(df_t)}, śr. RSSI: {df_t['rssi'].mean():.2f} dBm, "
          f"min: {df_t['rssi'].min()} dBm, max: {df_t['rssi'].max()} dBm")

# =============================================================================
# 2. TWORZENIE MAPY HEATMAP Z LEGENDĄ I FILTROWANIEM
# =============================================================================

# Środek mapy ustawiamy na średnią współrzędnych pomiarów
srodek_mapy = [df['lat'].mean(), df['lon'].mean()]

# Tworzenie mapy Folium
mapa = folium.Map(location=srodek_mapy, zoom_start=15, tiles='OpenStreetMap')

# Parametry heatmapy
GRADIENT = {0.0: 'blue', 0.3: 'cyan', 0.5: 'lime', 0.7: 'yellow', 1.0: 'red'}
HEATMAP_PARAMS = dict(
    radius=50,        # większy zasięg wpływu punktu
    blur=25,          # łagodniejsze przejścia kolorów
    max_val=1.0,      # normalizacja
    min_opacity=0.3,  # lekko przezroczyste miejsca o niskim sygnale
    gradient=GRADIENT,
    max_zoom=1
)

technologie = df['tech'].unique()

# Osobne warstwy dla każdej technologii — widoczne/ukrywane przez LayerControl
for tech in technologie:
    df_tech = df[df['tech'] == tech]

    # --- Warstwa heatmapy ---
    warstwa_heat = folium.FeatureGroup(name=f"Heatmapa — {tech}", show=True)
    dane_heatmap = []
    for _, wiersz in df_tech.iterrows():
        # Konwersja RSSI na wartość dodatnią (im wyższa wartość, tym silniejszy sygnał)
        waga = max(0.1, wiersz['rssi'] + 120) / 100
        dane_heatmap.append([wiersz['lat'], wiersz['lon'], waga])
    HeatMap(dane_heatmap, **HEATMAP_PARAMS).add_to(warstwa_heat)
    warstwa_heat.add_to(mapa)

    # --- Warstwa markerów ---
    warstwa_markery = folium.FeatureGroup(name=f"Pomiary — {tech}", show=True)
    for _, wiersz in df_tech.iterrows():
        folium.CircleMarker(
            location=[wiersz['lat'], wiersz['lon']],
            radius=5,
            color='black',
            fill=True,
            fillOpacity=0.7,
            popup=f"Tech: {wiersz['tech']}<br>RSSI: {wiersz['rssi']} dBm"
        ).add_to(warstwa_markery)
    warstwa_markery.add_to(mapa)

# Kontrolka warstw — checkboxy do filtrowania per technologia
folium.LayerControl(collapsed=False).add_to(mapa)

# =============================================================================
# 3. DODANIE LEGENDY
# =============================================================================

legend_html = """
<div style="
    position: fixed; 
    top: 10px; 
    left: 50px; 
    width: 300px; 
    height: auto; 
    background-color: white; 
    border:2px solid grey; 
    z-index:9999; 
    font-size:14px;
    padding: 10px;
    border-radius: 5px;
    box-shadow: 0 0 10px rgba(0,0,0,0.5);
">
    <p><strong>Legenda - Siła sygnału RSSI</strong></p>
    <div style="display: flex; align-items: center; margin: 5px 0;">
        <div style="width: 20px; height: 20px; background: red; margin-right: 10px; border: 1px solid black;"></div>
        <span>Bardzo dobry (-60 do -40 dBm)</span>
    </div>
    <div style="display: flex; align-items: center; margin: 5px 0;">
        <div style="width: 20px; height: 20px; background: yellow; margin-right: 10px; border: 1px solid black;"></div>
        <span>Dobry (-80 do -60 dBm)</span>
    </div>
    <div style="display: flex; align-items: center; margin: 5px 0;">
        <div style="width: 20px; height: 20px; background: lime; margin-right: 10px; border: 1px solid black;"></div>
        <span>Średni (-100 do -80 dBm)</span>
    </div>
    <div style="display: flex; align-items: center; margin: 5px 0;">
        <div style="width: 20px; height: 20px; background: cyan; margin-right: 10px; border: 1px solid black;"></div>
        <span>Słaby (-110 do -100 dBm)</span>
    </div>
    <div style="display: flex; align-items: center; margin: 5px 0;">
        <div style="width: 20px; height: 20px; background: blue; margin-right: 10px; border: 1px solid black;"></div>
        <span>Bardzo słaby (poniżej -110 dBm)</span>
    </div>
    <p style="margin-top: 10px; font-size: 12px; color: #666;">
    Cieplejsze kolory = lepszy sygnał
    </p>
</div>
"""
mapa.get_root().html.add_child(folium.Element(legend_html))

# =============================================================================
# 4. DODATKOWE INFORMACJE NA MAPIE
# =============================================================================

# Budowanie linii statystyk per tech do tytułu
tech_lines = ""
for tech in technologie:
    df_t = df[df['tech'] == tech]
    tech_lines += f"{tech}: {len(df_t)} pomiarów, śr. {df_t['rssi'].mean():.1f} dBm<br>"

title_html = """
<div style="
    position: fixed; 
    top: 50px; 
    right: 150px; 
    background-color: white; 
    border:2px solid grey; 
    z-index:9999; 
    font-size:14px;
    padding: 10px;
    border-radius: 5px;
    box-shadow: 0 0 10px rgba(0,0,0,0.5);
">
    <strong>Mapa zasięgu sygnału</strong><br>
    Pomiarów (z GPS fixem): {count}<br>
    Średni RSSI: {avg_rssi:.1f} dBm<br>
    <hr style="margin: 5px 0;">
    {tech_lines}
</div>
""".format(count=len(df), avg_rssi=df['rssi'].mean(), tech_lines=tech_lines)

mapa.get_root().html.add_child(folium.Element(title_html))

# Zapis mapy do pliku HTML
mapa.save('heatmap_zasieg.html')
print(f"\nMapa zapisana jako: heatmap_zasieg.html")

# =============================================================================
# 5. PROSTY WYKRES Z LEGENDĄ
# =============================================================================

def rssi_to_color(rssi):
    if rssi >= -60: return 'red'
    elif rssi >= -80: return 'yellow'
    elif rssi >= -100: return 'lime'
    elif rssi >= -110: return 'cyan'
    else: return 'blue'

# Osobny wykres dla każdej technologii + jeden zbiorczy
technologie_list = list(technologie)
n_tech = len(technologie_list)
fig, axes = plt.subplots(1, n_tech + 1, figsize=(6 * (n_tech + 1), 6))
if n_tech + 1 == 1:
    axes = [axes]

legend_elements = [
    Patch(facecolor='red',    label='Bardzo dobry (-60 do -40 dBm)'),
    Patch(facecolor='yellow', label='Dobry (-80 do -60 dBm)'),
    Patch(facecolor='lime',   label='Średni (-100 do -80 dBm)'),
    Patch(facecolor='cyan',   label='Słaby (-110 do -100 dBm)'),
    Patch(facecolor='blue',   label='Bardzo słaby (< -110 dBm)')
]

# Wykresy per technologia
for i, tech in enumerate(technologie_list):
    df_t = df[df['tech'] == tech].reset_index(drop=True)
    colors = [rssi_to_color(r) for r in df_t['rssi']]
    axes[i].scatter(range(len(df_t)), df_t['rssi'], c=colors, s=50, alpha=0.7)
    axes[i].set_title(f'RSSI — {tech}')
    axes[i].set_xlabel('Nr pomiaru')
    axes[i].set_ylabel('RSSI [dBm]')
    axes[i].grid(True, alpha=0.3)
    axes[i].legend(handles=legend_elements, loc='upper right', fontsize=8)

# Wykres zbiorczy (wszystkie technologie razem, różne markery)
markers = ['o', 's', '^', 'D']
for i, tech in enumerate(technologie_list):
    df_t = df[df['tech'] == tech].reset_index(drop=True)
    colors = [rssi_to_color(r) for r in df_t['rssi']]
    axes[-1].scatter(range(len(df_t)), df_t['rssi'], c=colors, s=50, alpha=0.7,
                     marker=markers[i % len(markers)], label=tech)
axes[-1].set_title('RSSI — wszystkie technologie')
axes[-1].set_xlabel('Nr pomiaru')
axes[-1].set_ylabel('RSSI [dBm]')
axes[-1].grid(True, alpha=0.3)
axes[-1].legend(loc='upper right', fontsize=8)

plt.tight_layout()
plt.savefig('wykres_rssi.png', dpi=300, bbox_inches='tight')
plt.show()

print("\nAnaliza zakończona pomyślnie!")
print("Wygenerowane pliki:")
print("  - heatmap_zasieg.html (interaktywna mapa z legendą i filtrowaniem po tech)")
print("  - wykres_rssi.png    (wykresy RSSI per technologia + zbiorczy)")