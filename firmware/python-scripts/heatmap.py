"""
Skrypt do generowania mapy heatmapy z danych pomiarowych RSSI + SNR
Autor: [Twoje Imię i Nazwisko]
"""

import numpy as np
import pandas as pd
import folium
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# =============================================================================
# FUNKCJE KOLORÓW (dyskretne progi, identyczne z legendą — używane wszędzie:
# na mapie w siatce mozaikowej ORAZ na wykresach)
# =============================================================================

def rssi_to_color(rssi):
    if rssi >= -60: return 'red'
    elif rssi >= -80: return 'yellow'
    elif rssi >= -100: return 'lime'
    elif rssi >= -110: return 'cyan'
    else: return 'blue'

def snr_to_color(snr):
    if snr >= 5: return 'red'
    elif snr >= 0: return 'yellow'
    elif snr >= -10: return 'lime'
    elif snr >= -15: return 'cyan'
    else: return 'blue'

# =============================================================================
# FUNKCJE SIATKI (grid binning) — zamiast leaflet.heat, który sumuje
# nakładające się punkty i zniekształca kolory w gęsto próbkowanych miejscach
# =============================================================================

GRID_SIZE_M = 10  # rozmiar komórki siatki w metrach — dostosuj wg potrzeb

def przelicz_krok_siatki(lat_ref, rozmiar_m=GRID_SIZE_M):
    lat_step = rozmiar_m / 111320.0
    lon_step = rozmiar_m / (111320.0 * np.cos(np.radians(lat_ref)))
    return lat_step, lon_step

def zbuduj_siatke(df_tech, wartosc_col, lat_step, lon_step):
    df_bin = df_tech.copy()
    df_bin['lat_bin'] = (df_bin['lat'] / lat_step).round() * lat_step
    df_bin['lon_bin'] = (df_bin['lon'] / lon_step).round() * lon_step
    return (df_bin.groupby(['lat_bin', 'lon_bin'])[wartosc_col]
            .mean().reset_index())

# =============================================================================
# 1. WCZYTANIE DANYCH
# =============================================================================

df_raw = pd.read_csv('stare/data_with_snr.CSV')

df_raw.rename(columns={'Lat': 'lat', 'Lon': 'lon', 'RSSI': 'rssi',
                        'SNR': 'snr', 'Tech': 'tech'}, inplace=True)

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

print(f"\n=== STATYSTYKI RSSI ===")
print(f"Średni RSSI: {df['rssi'].mean():.2f} dBm")
print(f"Minimalny RSSI: {df['rssi'].min()} dBm")
print(f"Maksymalny RSSI: {df['rssi'].max()} dBm")

print(f"\n=== STATYSTYKI SNR ===")
print(f"Średni SNR: {df['snr'].mean():.2f} dB")
print(f"Minimalny SNR: {df['snr'].min()} dB")
print(f"Maksymalny SNR: {df['snr'].max()} dB")

for tech in df['tech'].unique():
    df_t = df[df['tech'] == tech]
    print(f"\n  [{tech}] pomiarów: {len(df_t)}, "
          f"śr. RSSI: {df_t['rssi'].mean():.2f} dBm (min {df_t['rssi'].min()} / max {df_t['rssi'].max()}), "
          f"śr. SNR: {df_t['snr'].mean():.2f} dB (min {df_t['snr'].min()} / max {df_t['snr'].max()})")

# =============================================================================
# 2. TWORZENIE MAPY Z SIATKĄ MOZAIKOWĄ (kolory 1:1 z legendą) I FILTROWANIEM
# =============================================================================

srodek_mapy = [df['lat'].mean(), df['lon'].mean()]
mapa = folium.Map(location=srodek_mapy, zoom_start=15, tiles='OpenStreetMap')

technologie = df['tech'].unique()

for tech in technologie:
    df_tech = df[df['tech'] == tech]
    lat_step, lon_step = przelicz_krok_siatki(df_tech['lat'].mean())

    # --- Warstwa "heatmapy" RSSI (mozaika komórek) ---
    warstwa_heat_rssi = folium.FeatureGroup(name=f"Heatmapa RSSI — {tech}", show=True)
    siatka_rssi = zbuduj_siatke(df_tech, 'rssi', lat_step, lon_step)
    for _, wiersz in siatka_rssi.iterrows():
        folium.Circle(
            location=[wiersz['lat_bin'], wiersz['lon_bin']],
            radius=GRID_SIZE_M * 0.75,
            color=None,
            fill=True,
            fill_color=rssi_to_color(wiersz['rssi']),
            fill_opacity=0.6,
            weight=0,
            popup=f"RSSI (śr. w komórce): {wiersz['rssi']:.1f} dBm"
        ).add_to(warstwa_heat_rssi)
    warstwa_heat_rssi.add_to(mapa)

    # --- Warstwa "heatmapy" SNR (mozaika komórek) ---
    warstwa_heat_snr = folium.FeatureGroup(name=f"Heatmapa SNR — {tech}", show=False)
    siatka_snr = zbuduj_siatke(df_tech, 'snr', lat_step, lon_step)
    for _, wiersz in siatka_snr.iterrows():
        folium.Circle(
            location=[wiersz['lat_bin'], wiersz['lon_bin']],
            radius=GRID_SIZE_M * 0.75,
            color=None,
            fill=True,
            fill_color=snr_to_color(wiersz['snr']),
            fill_opacity=0.6,
            weight=0,
            popup=f"SNR (śr. w komórce): {wiersz['snr']:.1f} dB"
        ).add_to(warstwa_heat_snr)
    warstwa_heat_snr.add_to(mapa)

    # --- Warstwa markerów (RSSI + SNR w popupie) ---
    warstwa_markery = folium.FeatureGroup(name=f"Pomiary — {tech}", show=True)
    for _, wiersz in df_tech.iterrows():
        folium.CircleMarker(
            location=[wiersz['lat'], wiersz['lon']],
            radius=5,
            color='black',
            fill=True,
            fillOpacity=0.7,
            popup=f"Tech: {wiersz['tech']}<br>RSSI: {wiersz['rssi']} dBm<br>SNR: {wiersz['snr']} dB"
        ).add_to(warstwa_markery)
    warstwa_markery.add_to(mapa)

folium.LayerControl(collapsed=False).add_to(mapa)

# =============================================================================
# 3. DODANIE LEGEND (RSSI + SNR)
# =============================================================================

legend_style = """
    position: fixed; top: 10px; left: 50px; width: 300px; height: auto;
    background-color: white; border: 2px solid grey; z-index: 9999;
    font-size: 14px; padding: 10px; border-radius: 5px;
    box-shadow: 0 0 10px rgba(0,0,0,0.5);
"""

legend_rssi_html = f"""
<div id="legend_rssi" style="{legend_style} display: none;">
    <p><strong>Legenda - Siła sygnału RSSI</strong></p>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: red; margin-right: 10px; border: 1px solid black;"></div><span>Bardzo dobry (-60 do -40 dBm)</span></div>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: yellow; margin-right: 10px; border: 1px solid black;"></div><span>Dobry (-80 do -60 dBm)</span></div>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: lime; margin-right: 10px; border: 1px solid black;"></div><span>Średni (-100 do -80 dBm)</span></div>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: cyan; margin-right: 10px; border: 1px solid black;"></div><span>Słaby (-110 do -100 dBm)</span></div>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: blue; margin-right: 10px; border: 1px solid black;"></div><span>Bardzo słaby (poniżej -110 dBm)</span></div>
</div>
"""

legend_snr_html = f"""
<div id="legend_snr" style="{legend_style} display: none;">
    <p><strong>Legenda - Jakość sygnału SNR</strong></p>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: red; margin-right: 10px; border: 1px solid black;"></div><span>Bardzo dobry (od 5 dB)</span></div>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: yellow; margin-right: 10px; border: 1px solid black;"></div><span>Dobry (0 do 5 dB)</span></div>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: lime; margin-right: 10px; border: 1px solid black;"></div><span>Średni (-10 do 0 dB)</span></div>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: cyan; margin-right: 10px; border: 1px solid black;"></div><span>Słaby (-15 do -10 dB)</span></div>
    <div style="display: flex; align-items: center; margin: 5px 0;"><div style="width: 20px; height: 20px; background: blue; margin-right: 10px; border: 1px solid black;"></div><span>Bardzo słaby (poniżej -15 dB)</span></div>
</div>
"""

js_logic = """
<script>
function updateLegends() {
    var layers = document.querySelectorAll('.leaflet-control-layers-overlays input');
    var rssiActive = false;
    var snrActive = false;

    layers.forEach(function(input) {
        if (input.checked) {
            var label = input.nextElementSibling.innerText;
            if (label.indexOf('RSSI') !== -1) rssiActive = true;
            if (label.indexOf('SNR') !== -1) snrActive = true;
        }
    });

    document.getElementById('legend_rssi').style.display = rssiActive ? 'block' : 'none';
    document.getElementById('legend_snr').style.display = snrActive ? 'block' : 'none';
}

document.addEventListener('click', updateLegends);
setTimeout(updateLegends, 500);
</script>
"""

mapa.get_root().html.add_child(folium.Element(legend_rssi_html + legend_snr_html + js_logic))

# =============================================================================
# 4. DODATKOWE INFORMACJE NA MAPIE
# =============================================================================

tech_lines = ""
for tech in technologie:
    df_t = df[df['tech'] == tech]
    tech_lines += (f"{tech}: {len(df_t)} pomiarów, "
                    f"śr. RSSI {df_t['rssi'].mean():.1f} dBm, "
                    f"śr. SNR {df_t['snr'].mean():.1f} dB<br>")

mapa.save('heatmap_zasieg.html')
print(f"\nMapa zapisana jako: heatmap_zasieg.html")

# =============================================================================
# 5. WYKRESY Z LEGENDĄ (RSSI i SNR)
# =============================================================================

technologie_list = list(technologie)
n_tech = len(technologie_list)
markers = ['o', 's', '^', 'D']

rssi_legend = [
    Patch(facecolor='red',    label='Bardzo dobry (-60 do -40 dBm)'),
    Patch(facecolor='yellow', label='Dobry (-80 do -60 dBm)'),
    Patch(facecolor='lime',   label='Średni (-100 do -80 dBm)'),
    Patch(facecolor='cyan',   label='Słaby (-110 do -100 dBm)'),
    Patch(facecolor='blue',   label='Bardzo słaby (< -110 dBm)')
]

snr_legend = [
    Patch(facecolor='red',    label='Bardzo dobry (>= 5 dB)'),
    Patch(facecolor='yellow', label='Dobry (0 do 5 dB)'),
    Patch(facecolor='lime',   label='Średni (-10 do 0 dB)'),
    Patch(facecolor='cyan',   label='Słaby (-15 do -10 dB)'),
    Patch(facecolor='blue',   label='Bardzo słaby (< -15 dB)')
]

# --- Wykresy RSSI: per tech + zbiorczy ---
fig, axes = plt.subplots(1, n_tech + 1, figsize=(6 * (n_tech + 1), 6))
if n_tech + 1 == 1:
    axes = [axes]

for i, tech in enumerate(technologie_list):
    df_t = df[df['tech'] == tech].reset_index(drop=True)
    colors = [rssi_to_color(r) for r in df_t['rssi']]
    axes[i].scatter(range(len(df_t)), df_t['rssi'], c=colors, s=50, alpha=0.7)
    axes[i].set_title(f'RSSI — {tech}')
    axes[i].set_xlabel('Nr pomiaru')
    axes[i].set_ylabel('RSSI [dBm]')
    axes[i].grid(True, alpha=0.3)
    axes[i].legend(handles=rssi_legend, loc='upper right', fontsize=8)

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

# --- Wykresy SNR: per tech + zbiorczy ---
fig2, axes2 = plt.subplots(1, n_tech + 1, figsize=(6 * (n_tech + 1), 6))
if n_tech + 1 == 1:
    axes2 = [axes2]

for i, tech in enumerate(technologie_list):
    df_t = df[df['tech'] == tech].reset_index(drop=True)
    colors = [snr_to_color(s) for s in df_t['snr']]
    axes2[i].scatter(range(len(df_t)), df_t['snr'], c=colors, s=50, alpha=0.7)
    axes2[i].set_title(f'SNR — {tech}')
    axes2[i].set_xlabel('Nr pomiaru')
    axes2[i].set_ylabel('SNR [dB]')
    axes2[i].grid(True, alpha=0.3)
    axes2[i].legend(handles=snr_legend, loc='upper right', fontsize=8)

for i, tech in enumerate(technologie_list):
    df_t = df[df['tech'] == tech].reset_index(drop=True)
    colors = [snr_to_color(s) for s in df_t['snr']]
    axes2[-1].scatter(range(len(df_t)), df_t['snr'], c=colors, s=50, alpha=0.7,
                       marker=markers[i % len(markers)], label=tech)
axes2[-1].set_title('SNR — wszystkie technologie')
axes2[-1].set_xlabel('Nr pomiaru')
axes2[-1].set_ylabel('SNR [dB]')
axes2[-1].grid(True, alpha=0.3)
axes2[-1].legend(loc='upper right', fontsize=8)

plt.tight_layout()
plt.savefig('wykres_snr.png', dpi=300, bbox_inches='tight')
plt.show()

print("\nAnaliza zakończona pomyślnie!")
print("Wygenerowane pliki:")
print("  - heatmap_zasieg.html (mapa RSSI + SNR, mozaika komórek per tech)")
print("  - wykres_rssi.png    (wykresy RSSI per technologia + zbiorczy)")
print("  - wykres_snr.png     (wykresy SNR per technologia + zbiorczy)")