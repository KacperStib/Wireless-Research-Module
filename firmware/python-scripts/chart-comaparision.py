import sys
import pandas as pd
import matplotlib.pyplot as plt

#MY_LABELS = ["Pomiar INA219", "IDLE"]
#MY_LABELS = ["DUT1", "DUT2"]
MY_LABELS = ["160 MHz", "120 MHz", "80 MHz"]

def process_file(file_path):
    df = pd.read_csv(file_path)
    df.columns = df.columns.str.strip()
    df['Current(mA)'] = df['Current(uA)'] / 1000.0
    
    ### dodatek
    df['Time_norm'] = df['Timestamp(ms)'] - df['Timestamp(ms)'].iloc[0]

    total_len = len(df)
    # LORA
    tx_idx = (int(0.20 * total_len), int(0.25 * total_len))
    rx_idx = (int(0.85 * total_len), int(0.9 * total_len))
    
    # ESPNOW
    tx_idx = (int(0.1 * total_len), int(0.4 * total_len))
    rx_idx = (int(0.7 * total_len), int(0.95 * total_len))

    tx_data = df.iloc[tx_idx[0]:tx_idx[1]].copy()
    rx_data = df.iloc[rx_idx[0]:rx_idx[1]].copy()
    
    # Normalizacja czasu dla wycinków
    tx_data['Time_norm'] = tx_data['Timestamp(ms)'] - tx_data['Timestamp(ms)'].iloc[0]
    rx_data['Time_norm'] = rx_data['Timestamp(ms)'] - rx_data['Timestamp(ms)'].iloc[0]
    
    stats = {
        'TX_mean': tx_data['Current(mA)'].mean(), 'TX_max': tx_data['Current(mA)'].max(),
        'RX_mean': rx_data['Current(mA)'].mean(), 'RX_max': rx_data['Current(mA)'].max(),
        'tx_range': (df['Timestamp(ms)'].iloc[tx_idx[0]], df['Timestamp(ms)'].iloc[tx_idx[1]]),
        'rx_range': (df['Timestamp(ms)'].iloc[rx_idx[0]], df['Timestamp(ms)'].iloc[rx_idx[1]])
    }
    return df, tx_data, rx_data, stats

def plot_single(data_list, title, y_label, stat_key_mean, stat_key_max, is_full=False):
    plt.figure(figsize=(10, 5))
    for df, stats, label in data_list:
        #x_col = 'Timestamp(ms)' if is_full else 'Time_norm'
        x_col = 'Time_norm'
        if is_full:
            label_text = label
        else:
            label_text = f"{label} (Śr: {stats[stat_key_mean]:.2f}mA)"
            
        plt.plot(df[x_col], df['Current(mA)'], label=label_text)
    
    plt.title(title, fontsize=20)
    plt.xlabel('Czas (ms)', fontsize=20)
    plt.ylabel(y_label, fontsize=20)
    plt.legend(loc='upper right', fontsize=20)
    plt.grid(True, linestyle=':', alpha=0.6)
    plt.tight_layout()

def main():
    if len(sys.argv) < 2:
        print("Użycie: python script.py <plik1.csv> <plik2.csv> <plik3.csv>")
        return

    full_datasets, tx_datasets, rx_datasets = [], [], []
    
    for i, path in enumerate(sys.argv[1:]):
        df, tx, rx, stats = process_file(path)
        label = MY_LABELS[i] if i < len(MY_LABELS) else f"Seria {i+1}"
        
        full_datasets.append((df, stats, label))
        tx_datasets.append((tx, stats, label))
        rx_datasets.append((rx, stats, label))
    
    plot_single(full_datasets, 'Pełny profil energetyczny', 'Prąd (mA)', 'TX_mean', 'TX_max', is_full=True)
    plot_single(tx_datasets, 'Nadawanie', 'Prąd TX (mA)', 'TX_mean', 'TX_max')
    plot_single(rx_datasets, 'Nasłuch', 'Prąd RX (mA)', 'RX_mean', 'RX_max')
    
    plt.show()

if __name__ == "__main__":
    main()