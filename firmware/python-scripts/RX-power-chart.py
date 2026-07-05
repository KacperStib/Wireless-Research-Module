import sys
import pandas as pd
import matplotlib.pyplot as plt

def load_data(file_path, limit_ms=20):
    df = pd.read_csv(file_path)
    df.columns = df.columns.str.strip()
    
    # 1. Ucinamy do 100ms (licząc od pierwszej próbki)
    start_time = df['Timestamp(ms)'].iloc[0]
    df = df[(df['Timestamp(ms)'] >= start_time) & (df['Timestamp(ms)'] <= start_time + limit_ms)]
    
    # Przesuwamy czas tak, żeby zaczynał się od 0
    df['Time_norm_ms'] = df['Timestamp(ms)'] - start_time
    
    # Zamiana uA na mA
    df['Current(mA)'] = df['Current(uA)'] / 1000.0
    
    mean_noise = df['Current(mA)'].mean()
    std_noise = df['Current(mA)'].std()
    
    return df, mean_noise, std_noise

def plot_noise_comparison(datasets):
    fig, ax = plt.subplots(figsize=(11, 5))
    colors = ['#1f77b4', '#d62728']

    for (df, mean, std, label), color in zip(datasets, colors):
        # Rysujemy względem Time_norm_ms
        ax.plot(df['Time_norm_ms'], df['Current(mA)'], 
                linewidth=1.5, color=color, label=label, alpha=0.8)
        
        # Statystyki w pudełku
        stats_text = f"{label}\nŚr: {mean:.4f} mA\nStd: {std:.4f} mA"
        y_pos = 0.95 if 'PPK2' in label else 0.85
        ax.text(0.1, y_pos, stats_text, transform=ax.transAxes, verticalalignment='top', 
                horizontalalignment='right', fontsize=12, fontfamily='monospace', color=color,
                bbox=dict(boxstyle='round,pad=0.4', facecolor='white', alpha=0.85, edgecolor=color))

    ax.set_title('Profile prądowe podczas nasłuchu (RX)', fontsize=20, weight='bold')
    ax.set_xlabel('Czas (ms)', fontsize=20)
    ax.set_ylabel('Prąd (mA)', fontsize=20)
    ax.set_xlim(0, 20) # Wymuszamy oś X do 100ms
    ax.grid(True, linestyle=':', alpha=0.6)
    ax.legend(loc='upper right', fontsize=25)
    plt.tight_layout()
    return fig

def main():
    if len(sys.argv) < 3:
        print("Użycie: python skrypt.py <ppk2.csv> <esp32.csv>")
        return

    # Ładujemy i ucinamy oba do 100ms
    data1 = load_data(sys.argv[1], limit_ms=20)
    data2 = load_data(sys.argv[2], limit_ms=20)
    
    datasets = [
        (data1[0], data1[1], data1[2], 'PPK2'),
        (data2[0], data2[1], data2[2], 'ESP32')
    ]
    
    plot_comparison = plot_noise_comparison(datasets)
    plt.show()

if __name__ == "__main__":
    main()