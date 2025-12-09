# 🛠️ Installation Guide for DevilutionX

This guide will walk you through the process of installing DevilutionX, starting with the required game files and then providing system-specific instructions.

## 1. Required MPQ Files

First, you will need the base game's files (`.mpq` files). You should place all the necessary files in the **same directory** for your specific platform's installation steps below.

### Base Game Files (Required)

* Locate **`DIABDAT.MPQ`** from your original Diablo CD or the [GoG installation](https://www.gog.com/game/diablo).
    * *Tip:* If you have the GoG installer, you can [extract the MPQ file](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer).

### DevilutionX Assets (Conditional)

For some platforms, these assets are packaged within the application (e.g., Android, macOS, Switch). When needed, this should normally have been included when you downloaded the application.

* **If you are somehow missing it and getting errors regarding missing files or the game not starting correctly, you can try downloading the latest bundle:**
    * [Download `devilutionx.mpq`](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/devilutionx.mpq)

### Diablo: Hellfire Expansion (Optional)

If you own the Hellfire expansion, include these additional files:

* **`hellfire.mpq`**
* **`hfmonk.mpq`**
* **`hfmusic.mpq`**
* **`hfvoice.mpq`**

### Language/Region Specific Files (Optional)

Include these files if you need special language support:

* **`fonts.mpq`**: Required for Chinese, Korean, and Japanese text.
    * [Download `fonts.mpq`](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/fonts.mpq)
* **`pl.mpq`**: For Polish voice support.
    * [Download `pl.mpq`](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/pl.mpq)
* **`ru.mpq`**: For Spanish voice support.
    * [Download `es.mpq`](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/es.mpq)
* **`ru.mpq`**: For Russian voice support.
    * [Download `ru.mpq`](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/ru.mpq)

## 2. Download DevilutionX

Download the latest [DevilutionX release](https://github.com/diasurgical/devilutionX/releases) for your system (if available) and extract the contents to a location of your choosing, or [build from source](building.md).

---

## 3. System-Specific Instructions

Follow the instructions for your specific operating system or device.

<details><summary>💻 Windows</summary>

**Note**: If you have the GoG version installed on your Windows machine, you do not need to manually copy the MPQ files.

1.  Copy the MPQ files (from section 1) to **one** of the following locations:
    * The folder containing the `devilutionx.exe` executable.
    * The data folder, usually located at: `%AppData%\diasurgical\devilution`
2.  Run `devilutionx.exe`.

</details>

<details><summary>🐧 Linux</summary>

1.  Install [SDL2](https://www.libsdl.org/download-2.0.php):
    * **Ubuntu/Debian/Rasbian**: `sudo apt install libsdl2-2.0-0 libsdl2-image-2.0-0`
    * **Fedora**: `sudo dnf install SDL2`
2.  Copy the MPQ files (from section 1) to **one** of the following locations:
    * The folder containing the DevilutionX executable.
    * The data folder, which will normally be: `~/.local/share/diasurgical/devilution/`
    * *Flatpak Installation Path:* `~/.var/app/org.diasurgical.DevilutionX/data/diasurgical/devilution/`
3.  Run `./devilutionx`.

</details>

<details><summary>🍎 MacOS X</summary>

1.  Copy the MPQ files (from section 1) to **one** of the following locations:
    * The folder containing the DevilutionX application.
    * The data folder, which will normally be: `~/Library/Application Support/diasurgical/devilution`
2.  Double-click `devilutionx`.

</details>

<details><summary>🤖 Android</summary>

1.  **Install the App** via one of these 3 methods:
    * [Google Play](https://play.google.com/store/apps/details?id=org.diasurgical.devilutionx)
    * Copy the APK file to the device and tap on it.
    * Install via `adb install` (if USB debugging is enabled).
2.  **Launch the App once** to create the necessary data folder.
3.  Connect the device to your computer via USB cable, and allow data access.
4.  Copy the MPQ files (from section 1) to the folder: `Android/data/org.diasurgical.devilutionx/files` on the device's internal storage.
5.  Disconnect your device and press **"Check again"** in the App to start the game.

> **Troubleshooting**: If you have trouble getting the MPQ files onto your device, refer to [our guide](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer#android) for extracting the MPQ files on the Android device itself.

</details>

<details><summary>📱 iOS & iPadOS</summary>

Use a sideloading application like [AltStore](https://altstore.io/) or [Sideloadly](https://sideloadly.io/) to install the `.ipa` file to your iDevice.

1.  **Launch the App once** after installation. It will state it cannot find the data file (`.MPQ`). This is required to create your Documents folder. Close the game.
2.  Copy the MPQ files (from section 1) using one of the following methods:

    **Method 1: Using Finder (MacOS)**
    * Connect your iDevice to your computer.
    * In Finder, click on your device and navigate to the **"Files"** tab.
    * Drag and drop the MPQ files onto the `devilutionx` directory.

    **Method 2: Using iTunes (Windows and older MacOS)**
    * Connect your iDevice to your computer and launch iTunes.
    * Click on your device, go to the **"File Sharing"** section, and drag and drop the MPQ files to the `devilutionx` directory.

</details>

<details><summary>🕹️ Nintendo Switch</summary>

1.  Copy **`devilutionx.nro`** into `/switch/devilutionx`.
2.  Copy the MPQ files (from section 1) to `/switch/devilutionx`.
3.  Launch `devilutionx.nro` by holding **R** on the installed game.
    > **Note:** Do not use the album to launch, as this limits memory and disables the touch keyboard for all homebrew.

</details>

<details><summary>🕹️ Nintendo 3DS</summary>

1.  Download **`devilutionx.cia`** from the [latest release](https://github.com/diasurgical/devilutionX/releases/latest) and place it on your SD card.
2.  Copy the MPQ files (from section 1) to the **`devilutionx`** subfolder under the **`3ds`** folder on your SD card (`/3ds/devilutionx`).
    > **Note:** All file and folder names should be lowercase. You do **not** need `devilutionx.mpq` on the SD card for 3DS.
3.  Put the SD card back into the console and power it on.
4.  Use a title manager like [FBI](https://github.com/Steveice10/FBI) to install `devilutionx.cia`.
    > *Optional:* You can use FBI's `Remote Install` option by scanning the QR code:
    > ![image](https://user-images.githubusercontent.com/9203145/144300019-e315c05f-515c-484d-975b-ce99da641585.png)

[Nintendo 3DS manual](/docs/manual/platforms/3ds.md)

</details>

<details><summary>🎮 Xbox One/Series</summary>

1.  Go to https://gamr13.github.io/ and follow the instructions in the Discord server.
2.  Install **DevilutionX** and **FTP-server** from the given URLs.
3.  Open DevilutionX, then open and start the FTP-server.
4.  Press `View` on DevilutionX and select `Manage game and add-ons`.
5.  Go to `File info` and note the `FullName`.
6.  Copy the MPQ files (from section 1) using an FTP-client on your PC to the path:
    `/LOCALFOLDER/*FullName*/LocalState/diasurgical/devilution`

</details>

<details><summary>🎮 Playstation 4</summary>

1.  Install `devilutionx-ps4.pkg`.
2.  Copy the MPQ files (from section 1) to `/user/data/diasurgical/devilution/` (e.g., using FTP).

</details>

<details><summary>🎮 Playstation Vita</summary>

1.  Install `devilutionx.vpk`.
2.  Copy the MPQ files (from section 1) to `ux0:/data/diasurgical/devilution/`.

</details>

<details><summary>🕹️ ClockworkPi GameShell</summary>

1.  Copy the `__init__.py` to a newly created folder under `/home/cpi/apps/Menu` and run it from the menu.
2.  From this menu, press **'X'** to clone the git repository and compile the code (dependencies are installed automatically).
3.  Once installed, pressing **'X'** will update and recompile.
4.  Copy the MPQ files (from section 1) to `/home/cpi/.local/share/diasurgical/devilution/`.
5.  You can now play the game from the same icon.

</details>

<details><summary>🕹️ GKD350h</summary>

1.  Copy [devilutionx-gkd350h.opk](https://github.com/diasurgical/devilutionX/releases/download/1.0.1/devilutionx-gkd350h.opk) to `/media/data/apps` or `/media/sdcard/apps/`.
2.  Copy the MPQ files (from section 1) to `/usr/local/home/.local/share/diasurgical/devilution/`.

</details>

<details><summary>🕹️ RetroFW</summary>

**Requires RetroFW 2.0+**

1.  Copy [devilutionx-retrofw.opk](https://github.com/diasurgical/devilutionX/releases/latest/download/devilutionx-retrofw.opk) to the apps directory.
2.  Copy the MPQ files (from section 1) to `~/.local/share/diasurgical/devilution`.
    > `~` is your home directory, `/home/retrofw` by default.

</details>

<details><summary>🕹️ RG350</summary>

**Requires firmware v1.5+**

1.  Copy [devilutionx-rg350.opk](https://github.com/diasurgical/devilutionX/releases/latest/download/devilutionx-rg350.opk) to `/media/sdcard/APPS/`.
2.  Copy the MPQ files (from section 1) to `/media/home/.local/share/diasurgical/devilution/`.
3.  **NOTE (Advanced):** You can create a symlink instead. SSH into your RG350 and run:
    ```bash
    ln -sf /media/sdcard/<path_to_MPQ> /media/home/.local/share/diasurgical/devilution/<MPQ>
    ```

</details>

<details><summary>🕹️ Miyoo Mini</summary>

**Requires OnionOS to be installed**

1.  Activate the ports collection using the Onion Installer on the device.
2.  Copy the contents of the released `.zip` file onto the root of your SD card.
3.  Copy the MPQ files (from section 1) to `/Emu/PORTS/Binaries/Diablo.port/FILES_HERE/`.

</details>
