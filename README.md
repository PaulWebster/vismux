# vismux

A low latency, real-time POSIX Shared Memory (SHM) replicator tailored specifically for **Squeezelite** audio visualizer data. It replicates the data over a UDP network pipeline to create SHM in same format as Squeezelite, allowing Jivelite and third-party tools like CAVA to run seamlessly on a system that is remote from the Squeezelite player.

## Table of Contents

- [Features](#features)
- [Usage](#usage)
	- [Source Server Mode (Run on your Squeezelite Host)](#1-source-server-mode-run-on-your-squeezelite-host)
	- [Destination Client Mode (Run on the Remote Visualizer Node)](#2-destination-client-mode-run-on-the-remote-visualizer-node)
	- [Downloading and unpacking](#3-downloading-and-unpacking)
	- [Running on piCorePlayer (pCP)](#4-running-on-picoreplayer-pcp)
	- [Global Configuration Flags](#global-configuration-flags)
	- [Interactive Controls](#interactive-controls)
- [Service Discovery Mode](#service-discovery-mode)
	- [Scanning the Subnet](#scanning-the-subnet)
	- [Discovery Output Format](#discovery-output-format)
- [Systemd Automation & Customisation](#systemd-automation--customisation)
	- [Customising the Source Service (`vismux-source.service`)](#1-customising-the-source-service-vismux-sourceservice)
	- [Customising the Destination Service (`vismux-dest.service`)](#2-customising-the-destination-service-vismux-destservice)
	- [Deploying and Activating the Units](#3-deploying-and-activating-the-units)
- [Logging & Diagnostics](#logging--diagnostics)
- [Systemd Automation](#systemd-automation)
- [Environment Preparation & Compilation](#environment-preparation--compilation)
	- [Host Option A: Compiling on a 64-bit Intel/AMD Workstation (Ubuntu / Debian)](#host-option-a-compiling-on-a-64-bit-intelamd-workstation-ubuntu--debian)
	- [Host Option B: Compiling on a 64-bit Raspberry Pi Host (Raspberry Pi OS 64-bit)](#host-option-b-compiling-on-a-64-bit-raspberry-pi-host-raspberry-pi-os-64-bit)
	- [Compilation Commands Matrix](#compilation-commands-matrix)

## Features
- **Replicate `Squeezelite` visualiser data** (for VU Meters and the like) from a "headless" system to a Linux system with a display
- **Multi-Client Multiplexing:** A single source engine can push synchronization packets to up to 16 remote destination nodes concurrently.
- **Pause State Synchronization:** Instantly pushes a silence frame to drop visualizer animations back down to baseline when audio playback pauses and suspends network traffic.
- **Out-of-Order Jitter Filter:** Automatically drops late UDP packets caused by network congestion.
- **Decentralised UDP Service Discovery:** Automatic subnet probing maps active `vismux` nodes.

*Note: This does not run on Windows because it does not have the same POSIX Shared Memory support as Linux. It has only been tested on Linux (including piCorePlayer)*

## Usage

Ensure that you select the correct binary for the platform that you run on. The examples below use `vismux` but it might need to be `vismux-armhf` or similar.
You need to run one or more `vismux` in Source mode on a system that runs `Squeezelite` plus one or more on a system with a display in Destination mode.

### 1. Source Server Mode (Run on your Squeezelite Host)
```bash
./vismux --source --mac b8:27:eb:01:02:03 --log-level 2
```
*Note: Defaults to listening for remote client visualizer requests on UDP port **`23483`**.*
The `--mac` parameter is optional. If omitted, `vismux` searches `/dev/shm` for Squeezelite visualizer segments.  
If exactly one is found, it is selected automatically.  
If multiple are found, `vismux` lists them and exits so the intended player can be selected with `--mac`.  
Squeezelite must be run with the visualiser enabled (-v command line parameter).  
This can be checked by listing the available SHM while Squeezelite is running.  

```bash
ls /dev/shm/squeezelite-*
```

If there is more than one Squeezelite on the source system then run one instance of `vismux` for each one that you want to share visualisation data for but give each one a different port number.

If no Squeezelite visualizer segment exists yet, source mode exits with an error by default. To wait for one to appear, use the interruptible polling mode:

```bash
./vismux --source --wait-for-shm --log-level 2
```

Press `q` or `Ctrl+C` to stop waiting. The wait option does not bypass the multiple-segment check.

### 2. Destination Client Mode (Run on the Remote Visualizer Node)
```bash
./vismux --destination --server 192.168.1.50 [--mac b8:27:eb:01:02:03] --log-level 2
```
*Note: the "--server" IP address is of the Source `vismux` not of **Lyrion Music Server**.*

If you omit the --mac parameter then `vismux` will use the MAC address that the remote `vismux` is using. This is the typical usage since Jivelite is looking for a MAC address that matches the selected player.
*Note: Jivelite-Vis from after September 2026 is required for this capability.*
**cava** can be configured to look for any particular Squeezelite MAC address - so in this case the -MAC can be used to create one of your choice. So you could create a fake one and then run ``vismux`` to target any remote `vismux` Source.

### 3. Downloading and unpacking
You can build `vismux` from the source code (instructions are below) or you can use a pre-built binary.

Pick the correct architecture for your system:
Intel/AMD 64-bit - x86_64  
Intel x86 32-bit - x86_32  
ARM 64-bit (examples - Raspberry Pi 3/4/5 running 64-bit OS) - aarch64  
ARMhf 32-bit (examples - Raspberry Pi 2/3/4/Zero 2 running 32-bit OS) - armhf  
ARMv6 32-bit (examples - Raspberry Pi 1 & Raspberry Pi Zero Classic) - armv6  

Collect a release from https://github.com/PaulWebster/vismux/releases

for example - to run in Source mode on a Raspberry Pi Zero 2W player you would  
(replace "version" below with the actual value from the release asset)

```bash
wget https://github.com/PaulWebster/vismux/releases/download/version/vismux-version-armv6.tar.gz
gunzip vismux-version-armhf.tar.gz
tar -xvf vismux-version-armhf.tar
chmod +x vismux-armhf
./vismux-armhf --version
```
You should see a final response similar to:
```bash
vismux version 0.0.9j
```
If you do not then did you download the correct initial file and replace the word "version" in all of the commands above?


### 4. Running on piCorePlayer (pCP)
Follow the download instructions above.
The commands line shown above can be included in the pCP Tweaks section to have `vismux` started on boot.
In that case pick the appropriate binary (vismux-aarch64, vismux-armhf, vismux-armv6) and specify the full path to it.

and then configure, in the pCP Tweaks page, a "User command" of (for a Raspberry Pi Zero 2W player)
```bash
/home/tc/vismux-armhf --source --wait-for-shm
```
(pick the correct binary name)

If you are updating from an older version where you have already configured the "Tweaks" then you should perform a manual backup to ensure that the update is saved.
```bash
pcp br
```


### Global Configuration Flags
- `-h, --help`          Display the help manual with usage instructions.
- `-v, --version`       Display application version details.
- `--port <p>`          Override the operational UDP data streaming port (default **`23483`**).
- `--wait-for-shm`      *(Source Only)* If no Squeezelite visualizer segment is found, keep polling `/dev/shm` until one appears. Without this option, source mode exits with an error.
- `--mac-timeout <sec>` *(Destination Only)* Time to wait for a valid source MAC before rotating the subscription port (default **`2`** seconds).
- `--log-level <0-3>`   Filter verbosity outputs (0=ERROR, 1=WARN, 2=INFO, 3=DEBUG). Info level is verbose on Destination so, once working, reduce the level or increase the stats interval.
- `--stats-int`        *(Destination Only)* Set the interval in seconds between "Processing updates" statistics log messages in Debug level (default **`10`**).
- `--remove-shm`       *(Destination Only)* Forces the deletion of the shared memory layout on exit. By default, `vismux` preserves the segment so external clients like Jivelite or CAVA stay connected continuously across restarts.
- `--no-discover`      Disable the background discovery listener thread, rendering the component invisible to --discover requests.
- `--discover-port <p>` Sets a custom network port to host or look up active service discovery sweeps, bypassing third-party `EADDRINUSE` resource blocks.

### Interactive Controls
 Do not require hitting Enter/CR
- Press `v`             Version - Display application version details
- Press `q`             Quit - close down ``vismux``
- Press `l`             Log level - Cycle console log levels dynamically mid-flight (0=ERROR -> 1=WARN -> 2=INFO -> 3=DEBUG).

## Service Discovery Mode

`vismux` features an integrated **UDP Broadcast Discovery Protocol** operating over a dedicated port (`DEFAULT_PORT + 1`, or **`23484`** by default). This allows nodes on the same local subnet to announce themselves or scan for peers automatically, outputting completely raw data records designed for direct ingestion by shell wrappers and deployment scripts.

*Note: This will only find instances using the requested port. If you have other instances using a different port that perform a discover request using that port.*

### Scanning the Subnet
To poll your local network for active `vismux` instances, pass the `--discover` parameter flag:

```bash
./vismux --discover [--discover-timeout <seconds>] [--discover-port <port>]
```
- `--discover-timeout <sec>`: Optional parameter defining how long the prober will listen to harvest network responses before exiting (Defaults to `2` seconds).
- `--discover-port <port>`: Optional override flag to redirect discovery queries to a custom port to bypass conflicts with third-party local daemons (Defaults to **`23484`**).

### Discovery Output Format
Responses are collected, fully deduplicated on an individual full-dataset basis (IP, Port, and MAC address combined), and printed to standard output using a strict, shell-parseable comma-separated template:

```text
ROLE,IP_ADDRESS,PORT,MAC_ADDRESS,APP_VERSION
```

**Example Multi-Node Output:**
```text
SOURCE,192.168.2.140,23483,2c:cf:67:82:cc:29,0.1.0
DESTINATION,192.168.2.227,23483,2c:cf:67:82:cc:29,0.1.0
SOURCE,192.168.2.140,23483,b8:27:eb:aa:bb:cc,0.1.0
```
*Note: Because deduplication assesses the entire combined dataset, a single host IP hosting or handling multiple separate MAC streams will have all discoverable entries listed.*

---

## Systemd Automation & Customisation

Production `.service` file templates are provided in the `/systemd` project directory. Because these background daemons require device-specific parameters, **users must edit the files before deployment to match their network topography.**

Running multiple instances on a single host is possible - but if using systemd this would require a distinct .service file for each with customised service names and ports

If port conflicts are expected on the network interface loops, remember to append the custom `--discover-port <port>` flag or `--no-discover` directive inside the unit's `ExecStart` execution command string.

### 1. Customising the Source Service (`vismux-source.service`)
Open the template file and modify the `ExecStart` line to include the actual hardware MAC address of your Squeezelite player:
```ini
ExecStart=/usr/local/bin/vismux --source --mac YOUR_PLAYER_MAC_HERE --log-level 2
```
The `--mac` argument can be omitted when exactly one Squeezelite visualizer segment is present in `/dev/shm`. Add `--wait-for-shm` if the service may start before Squeezelite creates its segment:
```ini
ExecStart=/usr/local/bin/vismux --source --wait-for-shm --log-level 2
```
Copy the finished configuration file to `/etc/systemd/system/vismux-source.service` on the Squeezelite player machine.

### 2. Customising the Destination Service (`vismux-dest.service`)
Open the template file and update the target server IP. The target MAC is optional:
```ini
ExecStart=/usr/local/bin/vismux --destination --server YOUR_SOURCE_IP_HERE [--mac YOUR_PLAYER_MAC_HERE] --log-level 2
```
Without `--mac`, the Destination waits for the Source subscription response to provide the MAC address before creating shared memory. Use `--mac-timeout <sec>` to control how long it waits before it retries.
*Optional: If you explicitly want the shared memory segment unlinked and wiped every time the daemon cycles, append the `--remove-shm` flag to the end of the command string.*

Copy the finished configuration file to `/etc/systemd/system/vismux-dest.service` on your remote viewer client systems.

### 3. Deploying and Activating the Units
Run these commands on the respective machines to register your changes and enable background booting:
```bash
# Reload systemd to parse the updated unit definitions
sudo systemctl daemon-reload

# Enable the service to boot automatically on startup
sudo systemctl enable vismux-source.service  # Run on Source
sudo systemctl enable vismux-dest.service    # Run on Destination

# Start the daemon process immediately
sudo systemctl start vismux-source.service
sudo systemctl start vismux-dest.service
```

*Note: Keep a backup of the edited files because a reinstall of `vismux` could overwrite them.*

## Logging & Diagnostics

`vismux` features an integrated timestamped logger. When running as a background service via systemd, follow the live diagnostics stream through `journalctl`:

```bash
sudo journalctl -u vismux-dest.service -f
```

**Example Output (Level 2 - INFO):**
```text
[2026-09-21 12:10:01] [INFO] New active visualiser subscription established from: 192.168.2.227:37179
[2026-09-21 12:10:02] [INFO] Processing updates! Total frames captured: 120 (Mode: Delta) [Total Data: 0.35 MB]
[2026-09-21 12:11:14] [INFO] Squeezelite playback paused. Sending silence flush to destination nodes.
```

## Environment Preparation & Compilation

Because `vismux` relies on strict memory-alignment maps, you must build the precise binary architecture matching your target device's operating system. Follow the steps below to prepare your host system toolchains.

### Host Option A: Compiling on a 64-bit Intel/AMD Workstation (Ubuntu / Debian)
To configure your desktop development machine to cross-compile for the entire matrix of x86 and ARM target systems, install the required toolchains via `apt`:

```bash
sudo apt update
sudo apt install build-essential gcc-multilib gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu
```

### Host Option B: Compiling on a 64-bit Raspberry Pi Host (Raspberry Pi OS 64-bit)
When using a modern 64-bit Raspberry Pi as your compilation host, it targets 64-bit ARM natively. To enable it to compile for older 32-bit Pi hardware targets, install the 32-bit cross-compiler:

```bash
sudo apt update
sudo apt install build-essential gcc-arm-linux-gnueabihf
```
*Note: A native ARM host cannot cross-compile Intel (`x86_64` / `x86-32`) binaries out-of-the-box via standard apt utilities.*

---

### Compilation Commands Matrix

Once your environment toolchains are installed, invoke the explicit architectural target required by your deployment profile from the project root:

```bash
# Clean out any previous build objects first
make clean

# Compile the target for your deployment machine:
make native    # Matches your current host CPU architecture configuration
make x86_64    # For modern Intel/AMD 64-bit systems (Desktops/Servers)
make x86-32    # For legacy Intel/AMD 32-bit hardware profiles
make aarch64   # For Raspberry Pi 3/4/5, Zero 2 W running a 64-bit OS
make armhf     # For Raspberry Pi 2/3/4 running a legacy 32-bit OS
make armv6     # For Raspberry Pi 1, Raspberry Pi Zero (Classic 32-bit)
```
*⚠️ Warning for Classic Pi Zero/Pi 1 Users: You must explicitly use `make armv6` to apply the required hardware down-tuning constraints. Running a native 32-bit build (`make native`) on a Pi 3/4 and copying it to a Pi Zero will cause an instant `Illegal Instruction` crash.*

---