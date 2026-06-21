import sys
import os
import pandas as pd
import matplotlib.pyplot as plt

def load_and_process(file_path, is_esp32=False, external_threshold=None):
    df = pd.read_csv(file_path)
    df.columns = df.columns.str.strip()
    
    # UWAGA: Dla ESP32 nie usuwamy duplikatów timestampów ms, 
    # bo przy ~8kHz kilka próbek ma tę samą milisekundę!
    if not is_esp32:
        df = df.drop_duplicates(subset=['Timestamp(ms)'])

    time_col, current_col = 'Timestamp(ms)', 'Current(uA)'
    
    # --- NOWOŚĆ: Obliczanie liczby próbek i częstotliwości kHz ---
    total_samples = len(df)
    time_span_ms = df[time_col].iloc[-1] - df[time_col].iloc[0]
    
    if time_span_ms > 0:
        khz = (total_samples / time_span_ms)  # próbki / ms to bezpośrednio kHz
    else:
        khz = 0.0
    
    # Drukowanie statystyk w konsoli
    label = "ESP32" if is_esp32 else "PPK2"
    print(f"[{label}] Plik: {os.path.basename(file_path)}")
    print(f"  -> Liczba próbek w pliku: {total_samples}")
    print(f"  -> Częstotliwość próbkowania: {khz:.2f} kHz (Średnio co {1000/khz if khz > 0 else 0:.1f} µs)")
    print("-" * 50)
    # -------------------------------------------------------------

    # Wyznaczenie tła i progu
    baseline = df.tail(20)[current_col].mean() if is_esp32 else df.head(10)[current_col].mean()
    maximum = df[current_col].max()
    fwhm_calc = baseline + (maximum - baseline) / 2
    threshold = external_threshold if (not is_esp32 and external_threshold is not None) else fwhm_calc

    # Detekcja piku
    pulse = df[df[current_col] > threshold]
    if not pulse.empty:
        start_time = pulse[time_col].iloc[0]
        end_time = pulse[time_col].iloc[-1]
        
        # Obliczenia statystyk tylko dla piku
        df_zoom = df[(df[time_col] >= start_time) & (df[time_col] <= end_time)]
        stats = {
            'duration_ms': end_time - start_time,
            'mean_mA': df_zoom[current_col].mean() / 1000.0,
            'peak_mA': maximum / 1000.0,
            'total_samples': total_samples,  # dorzucamy do statystyk wykresu
            'khz': khz                        # dorzucamy do statystyk wykresu
        }
        
        # PRZESKALOWANIE: normalizujemy czas całego pliku względem startu piku
        df = df.copy()
        df['time_norm'] = df[time_col] - start_time
    else:
        print(f"Uwaga: Nie znaleziono piku w {file_path}")
        return df, {'duration_ms': 0, 'mean_mA': 0, 'peak_mA': 0, 'total_samples': total_samples, 'khz': khz}, threshold

    return df, stats, threshold

def plot_comparison(datasets):
    fig, ax = plt.subplots(figsize=(11, 5))
    colors = ['#1f77b4', '#d62728']

    for (df, stats, label), color in zip(datasets, colors):
        # Rysujemy cały przebieg z przesuniętą osią czasu
        ax.plot(df['time_norm'], df['Current(uA)'] / 1000.0,
                linewidth=1.5, color=color, label=label, alpha=0.8)

        # Uaktualniona ramka o info o próbkach i kHz
        stats_text = (
            f"{label}\n"
            f"Pik Czas: {stats['duration_ms']:.2f} ms\n"
            f"Pik Śr: {stats['mean_mA']:.2f} mA\n"
            f"Peak: {stats['peak_mA']:.2f} mA\n"
            f"Próbki: {stats['total_samples']}\n"
            f"Częst: {stats['khz']:.2f} kHz"
        )
        y_pos = 0.75 if 'PPK2' in label else 0.55
        ax.text(0.98, y_pos, stats_text, transform=ax.transAxes, verticalalignment='top', 
                horizontalalignment='right', fontsize=12, fontfamily='monospace', color=color,
                bbox=dict(boxstyle='round,pad=0.4', facecolor='white', alpha=0.85, edgecolor=color))

    ax.set_title('Profile prądowe podczas nadawania (TX))', fontsize=20, weight='bold')
    ax.set_xlabel('Czas od początku piku (ms)', fontsize=20)
    ax.set_ylabel('Prąd (mA)', fontsize=20)
    ax.grid(True, linestyle=':', alpha=0.4)
    ax.legend(loc='upper right', fontsize=25)
    plt.tight_layout()
    return fig

def main():
    if len(sys.argv) < 3:
        print("Użycie: python skrypt.py <ppk2.csv> <esp32.csv>")
        return

    print("-" * 50)
    df_ppk2, stats_ppk2, threshold = load_and_process(sys.argv[1], is_esp32=False)
    df_esp32, stats_esp32, _ = load_and_process(sys.argv[2], is_esp32=True, external_threshold=threshold)
    
    datasets = [(df_ppk2, stats_ppk2, 'PPK2'), (df_esp32, stats_esp32, 'ESP32')]
    plot_comparison(datasets)
    plt.show()

if __name__ == "__main__":
    main()