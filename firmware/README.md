# Mobile Measurement and Radio Node (ESP32-C6)

An embedded software project developed as part of a Master's Thesis. The system is built on the **ESP32-C6** SoC using the **FreeRTOS** kernel within the **ESP-IDF v5.3** framework. The device is responsible for real-time geolocation data acquisition (GPS), power profile monitoring (INA219), local data visualization (OLED), non-volatile logging (NVS/SD), and long-range wireless data transmission via LoRa and ESP-NOW protocols.

---

## Project Architecture

The firmware is designed with a highly modular component-based architecture complying with the ESP-IDF standard:

```text
├── main/                   # Main application subsystem
│   ├── CMakeLists.txt
│   └── main.c              # System initialization, core state machine, and FreeRTOS tasks
└── components/             # Custom hardware abstraction components
    ├── dev_config/         # Global hardware and pinout configurations
    ├── esp_now_c/          # Peer-to-peer 2.4 GHz ESP-NOW communication driver
    ├── gps/                # High-performance NMEA parser for GPS receivers (UART)
    ├── i2c/                # Shared I2C master bus initializer
    ├── ina219/             # Power monitor driver with real-time mA peak detection
    ├── lora/               # Semtech LoRa RF transceiver driver (SPI)
    ├── nvs_storage/        # Non-Volatile Storage manager for runtime metrics
    ├── oled/               # SSD1306 OLED display controller for local telemetry
    ├── sd_card/            # FATFS-backed microSD card logging interface (SPI)
    ├── shell_mng/          # Interactive CLI developer shell for debugging
    └── spi/                # Shared SPI master bus initializer
```
---

## Power Supply & Hardware Connection

The node features an on-board dual-power path managed by an **automatic MOSFET power switch**, allowing safe simultaneous connection of both sources (USB + Battery).

### 1. Battery Power (Field Deployment)
* **Hardware:** On-board battery holder for a single-cell (**1S Li-Ion**, 3.7V–4.2V).
* **Behavior:** Standalone field mode. The system loads boot configurations from NVS flash.

### 2. USB Power (Development & Diagnostics)
* **Hardware:** USB-C interface connected directly to the ESP32-C6 internal **USB-Serial-JTAG Controller**.
* **Behavior:** Interactive debug mode. Powers up the live logging transmission and enables full control via the developer CLI console shell.

---

## Interactive Shell Commands (`shell_mng`)

The device exposes an interactive Command Line Interface (CLI) via the built-in USB-Serial-JTAG controller. Connect to the ESP32-C6 via USB and open a serial terminal (e.g., using `idf.py monitor` or PuTTY) to use the following commands:

### 1. System Administration & Diagnostics
* **`mymac`**
  * **Description:** Reads and displays the factory-fused permanent MAC address of this specific ESP32-C6 Wi-Fi/Bluetooth station interface.
  * **Usage:** `mymac`
* **`tasks`**
  * **Description:** Generates a real-time diagnostic snapshot of all active FreeRTOS tasks. It displays the task name, operational state (Running, Ready, Blocked, Suspended), priority, runtime identifier, and crucially—the **stack high-water mark** (the minimum amount of free stack space remaining in bytes). Excellent for debugging stack overflows.
  * **Usage:** `tasks`
* **`reset`**
  * **Description:** Safely delays execution for 100ms to allow flush buffers to clear, then triggers a software forced CPU system restart (`esp_restart`).
  * **Usage:** `reset`

### 2. Global Logging Control
* **`log`**
  * **Description:** Dynamically modifies runtime logging verbosity without resetting the firmware. It can target all log tags globally or a specific code component tag.
  * **Usage:** `log <none|err|wrn|inf|dbg> [tag]`
  * **Examples:**
    * `log dbg` — Enables full verbose debugging output across the entire system.
    * `log none nmea_parser` — Mutes all logs coming only from the GPS NMEA parser module.

### 3. Wireless Protocol Selection & Routing
* **`radio`**
  * **Description:** Switches the active wireless networking stack technology on the fly. Executing it without parameters queries the current state.
  * **Usage:** `radio [lora|espnow]`
* **`dir`**
  * **Description:** Configures the operational role of the node inside the radio network topology. It shifts the transceiver logic between a continuous transmitter (TX) beacon or an continuous receiver (RX) listener.
  * **Usage:** `dir [rx|tx]`
* **`mac`**
  * **Description:** Sets the destination hardware MAC address for the ESP-NOW peer target. If the current active protocol is ESP-NOW, it instantly hot-plugs the new peer configuration into the network stack.
  * **Usage:** `mac XX:XX:XX:XX:XX:XX`

### 4. Hardware Parameters & Memory
* **`lora`**
  * **Description:** Offers fine-grained control over physical Semtech LoRa modulation parameters for link-budget optimization.
  * **Usage:** `lora <sf|bw|cr|pwr> <value>`
  * **Parameters:**
    * `sf (7-12)`: Spreading Factor (higher values increase range but lower data rate).
    * `bw (0-9)`: Bandwidth index selection.
    * `cr (1-4)`: Coding Rate for forward error correction.
    * `pwr (2-17)`: RF output transmission power configuration measured in dBm.
* **`config`**
  * **Description:** Main memory manager interface for the node's running configuration profile (Selected tech, role, target MAC, and LoRa registers).
  * **Usage:** `config <show|save|load>`
  * **Actions:**
    * `show` — Prints the currently active configurations stored in volatile RAM.
    * `save` — Commits the current RAM runtime settings permanently into Non-Volatile Storage (NVS) flash memory.
    * `load` — Pulls the saved configuration struct back from NVS flash and automatically re-initializes the active radio peripheral drivers.
---
