# Installing

First, you will need access to the game's MPQ files.

- Locate `DIABDAT.MPQ` on your CD, or in the [GoG](https://www.gog.com/game/diablo) installation (or [extract it from the GoG installer](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer)).
- For the Diablo: Hellfire expansion you will also need `hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq`, `hfvoice.mpq`.
- DevilutionX comes with [devilutionx.mpq](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/devilutionx.mpq) which is required to run the game properly.
- Chinese, Korean, and Japanese users will also need [fonts.mpq](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/fonts.mpq) or the text will be missing.
- For Polish voice support you need [pl.mpq](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/pl.mpq)
- For Russian voice support you need [ru.mpq](https://github.com/diasurgical/devilutionx-assets/releases/latest/download/ru.mpq)

Download the latest [DevilutionX release](https://github.com/diasurgical/devilutionX/releases) for your system (if available) and extract the contents to a location of your choosing, or [build from source](building.md). Then follow the system-specific instructions below.

<details><summary>Android</summary>

First install the App via one of these 3 methods:

- [Google Play](https://play.google.com/store/apps/details?id=org.diasurgical.devilutionx)
- Copy the APK file to the device and tap on it in the device's file explorer and follow the instructions
- Install via `adb install` (if USB debugging is enabled on the device)

Then launch the App, this will let it create the folder where you need to place the MPQ files.

Connect the device to your computer via USB cable, and allow data access from your device:

![image](https://user-images.githubusercontent.com/204594/139543023-3c45bb22-35f7-41af-8b3d-c714a9542d23.png)

Open the device's internal storage, and navigate to `Android/data/org.diasurgical.devilutionx/files`, then copy the MPQ-files to this folder.

![image](https://user-images.githubusercontent.com/204594/139542962-4e776854-6ca4-4872-8ed6-6303fc4bf040.png)

When the transfer is done you can disconnect your device and press "Check again" in the App to start the game.

![image](https://user-images.githubusercontent.com/204594/139541657-d8c1197d-fbef-42b6-a34f-2b17f1ceab5f.png)

**Troubleshooting**: If you encounter some trouble getting the MPQ files onto your Android device, you can refer to
[our guide](https://github.com/diasurgical/devilutionX/wiki/Extracting-MPQs-from-the-GoG-installer#android) for extracting the MPQ files on the Android device itself.
Note that Inno Setup Extractor is only available for Android 8+ and the Import Data feature is only available for Android 7.1.1+.

</details>

<details><summary>Windows</summary>

**Note**: if you have the GoG version installed on your Windows machine, you do not need to copy the MPQ files as described in the instructions.
- Copy the MPQ files to the folder containing the DevilutionX exe, or to the data folder. The data folder path may differ, but will normally be `%AppData%\diasurgical\devilution`
- Run `devilutionx.exe`

</details>

<details><summary>Linux</summary>

- Copy the MPQ files to the folder containing the DevilutionX executable, or to the data folder. The data folder path may differ depending on distro, version, and security settings, but will normally be `~/.local/share/diasurgical/devilution/`, for flatpak installation the path is `~/.var/app/org.diasurgical.DevilutionX/data/diasurgical/devilution/`
- Install [SDL2](https://www.libsdl.org/download-2.0.php):
- Ubuntu/Debian/Rasbian `sudo apt install libsdl2-2.0-0 libsdl2-image-2.0-0`
- Fedora `sudo dnf install SDL2`
- Run `./devilutionx`

</details>

<details><summary>MacOS X</summary>

- Copy the MPQ files to the folder containing the DevilutionX application, or to the data folder. The data folder path may differ, but will normally be `~/Library/Application Support/diasurgical/devilution`
- Double-click `devilutionx`

</details>

<details><summary>FreeBSD</summary>

- Copy the MPQ files to the folder containing the DevilutionX application, or to the data folder `~/.local/share/diasurgical/devilution/`
- To install the port: `cd /usr/ports/games/devilutionX/ && make install clean`
- To add the package, run one of these commands:
  `pkg install games/devilutionX` || `pkg install devilutionX`
- Run `devilutionx`

</details>

<details><summary>iOS & iPadOS</summary>

Certain sideloading applications exist which can let you install IPA packages to your device such as AltStore (https://altstore.io/) and Sideloadly (https://sideloadly.io/). Using such a sideloading application, install the .ipa file to your iDevice.

Once the App is installed, launch it once. It will say that it cannot find the data file (.MPQ). This is ok. Close the game. Do not skip this step as it will create your Documents folder that you will need to access in later steps.

There are 3 way to get install the MPQs. Make sure to have installed the app and have launched it once before attempting to copy over the MPQ files.

~~Method 1~~ (***May no longer work***): ~~Using the files.app~~

~~- Using the files.app, navigate to "On My iPhone / On My iPad"~~

![ios_mpq_files01](https://user-images.githubusercontent.com/1339414/145088910-5664aed6-3a53-43f4-8088-834796792f68.png)

~~- Copy the MPQs to the devilutionx directory. Your directory should look like on the picture below~~

![ios_mpq_files02](https://user-images.githubusercontent.com/1339414/145089068-2c87887b-58c3-4759-9270-9d8934fe90bb.png)

~~- Go back to your home screen and launch the game~~

Method 2: Using Finder (MacOS)

On more recent versions of MacOS, iTunes is no more. Instead you can use Finder to directly copy data to and from your iDevices

- Connect your iDevice to your computer and click on the "Files" tab.

  ![ios_mpq_finder01](https://user-images.githubusercontent.com/1339414/145089218-f5424196-f345-45da-aca6-9c2b2e06cdf0.png)

- Drag and drop the MPQ files on the devilutionx directory

Method 3: Using iTunes (Windows and older MacOS)

- Connect you iDevice to your computer and launch iTunes

- Click on your device and in the files tab drag and drop the MPQ files to the devilutionx directory

</details>

<details><summary>Nintendo Switch</summary>

- Copy `devilutionx.nro` in into `/switch/devilutionx`
- Copy the MPQ files to `/switch/devilutionx`.
- Launch `devilutionx.nro` by holding R the installed game. Do not use the album to launch, if you use the album, the homebrew will only have a small amount of memory available, and the touch keyboard won't work. This is true for all homebrew, not just DevilutionX.

</details>

<details><summary>Nintendo 3DS</summary>

Download `devilutionx.cia` from the [latest release](https://github.com/diasurgical/devilutionX/releases/latest) and place it on your SD card.
This file will be used later to install the game on the 3DS console.

Copy the MPQ files to the `devilutionx` subfolder under the `3ds` folder on your SD card (`/3ds/devilutionx`). The game will read these files from this location.
You do not need a copy of `devilutionx.mpq` on your SD card to run the game on 3DS.
To avoid errors when you run the game, if you have an old version of `devilutionx.mpq` in the folder, you should remove it now. Note, all file and folder names should be lowercase, as shown in the example screenshot below.

![image](https://user-images.githubusercontent.com/9203145/141046152-61be39dc-d14e-4b10-accf-1755a93972f7.png)

Put the SD card back into the 3DS console and then power it on.
Use a title manager such as [FBI](https://github.com/Steveice10/FBI) to install `devilutionx.cia`.
You will no longer need `devilutionx.cia` after this step, and so it can be removed after the installation.

![image](https://user-images.githubusercontent.com/9203145/141047377-789e5f1c-d049-4a25-82bb-8b3572d791cf.png)

FBI provides the `Remote Install` option to download and install a cia in one step by scanning a QR code using the Nintendo 3DS camera.
This is most useful when upgrading from a previous version so that you can perform the installation without removing the SD card from the console.
If you'd like to use this option, scan the QR code below.

![image](https://user-images.githubusercontent.com/9203145/144300019-e315c05f-515c-484d-975b-ce99da641585.png)

[Nintendo 3DS manual](/docs/manual/platforms/3ds.md)

</details>

<details><summary>Xbox One/Series</summary>

- Go to https://gamr13.github.io/ and follow the instructions in the Discord server
- Install DevilutionX and FTP-server from the given URLs
- Open DevilutionX
- Open and start the FTP-server
- Press `View` on DevilutionX and select `Manage game and add-ons`
- Go to `File info` and note `FullName`
- Copy the MPQ files to `/LOCALFOLDER/*FullName*/LocalState/diasurgical/devilution` using a FTP-client on your PC

![image](https://user-images.githubusercontent.com/204594/187104388-fc5648da-5629-4335-ae8b-403600721e2a.png)

</details>

<details><summary>Playstation 4</summary>

- Install devilutionx-ps4.pkg
- Copy the MPQ files (e.g., using ftp) to `/user/data/diasurgical/devilution/`

</details>

<details><summary>Playstation Vita</summary>

- Install devilutionx.vpk
- Copy the MPQ files to `ux0:/data/diasurgical/devilution/`.

</details>

<details><summary>ClockworkPi GameShell</summary>

- Copy the `__init__.py` to a newly created folder under /home/cpi/apps/Menu and run it from the menu. The folder then symbolizes the devilutionX icon.
- From this menu, you can press 'X' to clone the git repository for devilutionX and compile the code. Dependencies are installed automatically (cmake and SDL development packages).
- Once installed, 'X' pulls the updated code and does the compiling. Note that any changes made locally to the source are reverted before pulling.
- When the compile is finished, Copy the MPQ files to `/home/cpi/.local/share/diasurgical/devilution/`
- You can now play the game from the same icon.

</details>

<details><summary>GKD350h</summary>

- Copy [devilutionx-gkd350h.opk](https://github.com/diasurgical/devilutionX/releases/download/1.0.1/devilutionx-gkd350h.opk) to `/media/data/apps` or `/media/sdcard/apps/`.
- Copy the MPQ files to `/usr/local/home/.local/share/diasurgical/devilution/`

</details>

<details><summary>RetroFW</summary>

**Requires RetroFW 2.0+.**

- Copy [devilutionx-retrofw.opk](https://github.com/diasurgical/devilutionX/releases/latest/download/devilutionx-retrofw.opk) to the apps directory.
- Copy the MPQ files to `~/.local/share/diasurgical/devilution`

`~` is your home directory, `/home/retrofw` by default.

</details>

<details><summary>RG350</summary>

**Requires firmware v1.5+**

- Copy [devilutionx-rg350.opk](https://github.com/diasurgical/devilutionX/releases/latest/download/devilutionx-rg350.opk) to `/media/sdcard/APPS/`.
- Copy the MPQ files to `/media/home/.local/share/diasurgical/devilution/`
- **NOTE:** You can copy the MPQ files to sdcard instead and create a symlink at the expected location. To do this, SSH into your RG350 and run:

 ~~~bash
 ln -sf /media/sdcard/<path_to_MPQ> /media/home/.local/share/diasurgical/devilution/<MPQ>
 ~~~

</details>

<details><summary>Miyoo Mini</summary>

**Requires OnionOS to be installed**

- Activate the ports collection by using the onion installer on the device
- Copy the contents of the released .zip-file onto the root of your SD card
- Copy the MPQ files to `/Emu/PORTS/Binaries/Diablo.port/FILES_HERE/`
</details>
