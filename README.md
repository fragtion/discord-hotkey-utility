# Discord Hotkey Utility

A lightweight Windows utility that lets you toggle your Discord mute and deafen states using either keyboard hotkeys, a joystick, or both — without Discord needing to be in focus (unlike Discord's built-in hotkey support). Supports visual screen flash feedback on state change (with multi-monitor support), and can run silently and in the background on system startup.

---

## Features

- Toggle Discord **mute** and **deafen** via joystick button or global keyboard hotkey
- Short press / long press support on a single joystick button
- Dedicated separate buttons for mute and deafen if your joystick supports it
- Full-screen color flash feedback on a configurable monitor
- Flash overlay displays status text ("Discord Muted", "Discord Undeafened", etc.)
- Automatically reconnects if Discord restarts or the joystick is unplugged
- Runs silently in the background with autostart support
- All configuration via a single `config.ini` file — no recompile needed

---

## Discord API Setup

This utility communicates with Discord via its local RPC interface, which requires you to register a free Discord Developer Application. This is a one-time setup.

### Steps

1. Go to [https://discord.com/developers/applications](https://discord.com/developers/applications) and log in.
2. Click **New Application** and give it any name (e.g. "MuteController").
3. On the **General Information** page, copy the **Application ID** — this is your `client_id`.
4. Go to **OAuth2** in the left sidebar.
5. Click **Reset Secret** and copy the generated value — this is your `client_secret`.
   > ⚠️ Keep your `client_secret` private. Do not share it or commit it to source control.
6. Under **OAuth2 → Redirects**, click **Add Redirect** and enter exactly:
   ```
   http://localhost:7878
   ```
   Then click **Save Changes**.

### First-Run Authentication

On first launch, the utility will send an authorization request to Discord, which will display a popup in the Discord client asking you to approve access to **"pipecom"**. Click **Authorize**.

The utility includes a **built-in lightweight HTTP server** that listens on `http://localhost:7878` to handle the OAuth2 redirect from Discord. This means no browser or external web server is required — the authentication flow is handled entirely locally and automatically. Once authorized, the access token is saved to `discord_token.txt` and reused on all subsequent launches. You will not be prompted again unless the token expires.

---

## Configuration (`config.ini`)

All settings are in `config.ini`, which must be in the same folder as the `.exe`.

```ini
[general]
flash_ms=250
flash_color_muted=FF0000
flash_color_unmuted=00FF00
flash_color_deafened=0000FF
flash_color_undeafened=00C8FF
flash_display=1

[discord]
client_id=YOUR_APPLICATION_ID
client_secret=YOUR_CLIENT_SECRET
redirect_uri=http://localhost:7878
token_file=discord_token.txt
reconnect_delay_ms=3000

[joystick]
enabled=true
vid=0x10C4
pid=0x82C0
mute_button=1
deafen_button=
long_press_ms=1000
reconnect_delay_ms=3000

[hotkey]
enabled=true
mute=CTRL+SHIFT+F11
deafen=CTRL+SHIFT+F12
```

---

### [general]

| Key | Description |
|-----|-------------|
| `flash_ms` | How long the screen flash lasts in milliseconds |
| `flash_color_muted` | Flash color when microphone is muted (hex RGB, `#` optional) |
| `flash_color_unmuted` | Flash color when microphone is unmuted |
| `flash_color_deafened` | Flash color when deafened (mic + speaker muted) |
| `flash_color_undeafened` | Flash color when undeafened |
| `flash_display` | Which monitor to flash. `0` = disabled, `1` = primary monitor, `2` = second monitor, `3` = third, etc. |

Colors are specified as 6-digit hex RGB values (e.g. `FF0000` for red). A leading `#` is accepted but not required.

---

### [discord]

| Key | Description |
|-----|-------------|
| `client_id` | Your Discord Application ID (from the Developer Portal) |
| `client_secret` | Your Discord OAuth2 client secret (from the Developer Portal) |
| `redirect_uri` | Must match exactly what you entered in the Discord Developer Portal. Default: `http://localhost:7878` |
| `token_file` | Filename where the OAuth2 access token is cached after first auth. Default: `discord_token.txt` |
| `reconnect_delay_ms` | How long to wait (in ms) between attempts to reconnect to Discord if the connection is lost. Discord can take several seconds to fully restart its IPC pipe after a client restart, so a value of 2000–5000ms is recommended to avoid hammering it with failed connection attempts. |

---

### [joystick]

| Key | Description |
|-----|-------------|
| `enabled` | Set to `true` to enable joystick support, `false` to disable |
| `vid` | USB Vendor ID of your joystick/button device (hex, e.g. `0x10C4`) |
| `pid` | USB Product ID of your joystick/button device (hex, e.g. `0x82C0`) |
| `mute_button` | HID data byte index for the mute button. Leave blank to disable. |
| `deafen_button` | HID data byte index for the deafen button. Leave blank to disable. |
| `long_press_ms` | How long a button must be held to trigger a long press action (milliseconds) |
| `reconnect_delay_ms` | How long to wait between reconnect attempts when the joystick is unplugged. Joystick devices can take a moment to re-enumerate after being reconnected, so a value of 2000–4000ms avoids spamming failed open attempts. |

#### Finding your VID and PID

1. Open **Device Manager** (Win+X → Device Manager)
2. Expand **Human Interface Devices**
3. Right-click your joystick → **Properties** → **Details** tab
4. Select **Hardware IDs** from the dropdown
5. You will see values like `HID\VID_10C4&PID_82C0`*(example only)* — use those hex values

#### Button index

The `mute_button` and `deafen_button` values refer to the byte index in the raw HID input report. Index `1` is the most common for the primary button on simple devices. You can identify the correct index by running the utility with console output visible and watching which byte changes when you press your button.

#### Button behavior modes

The behavior depends on which of `mute_button` and `deafen_button` are set:

| Configuration | Short press | Long press (held ≥ `long_press_ms`) |
|---------------|-------------|--------------------------------------|
| Only `mute_button` set | Toggle mute | Toggle deafen |
| Only `deafen_button` set | Toggle deafen | Toggle mute |
| Both set | `mute_button` toggles mute immediately | `deafen_button` toggles deafen immediately (no long press) |

> If `mute_button` and `deafen_button` are set to the same index, the utility will log a warning and treat it as if only `mute_button` is set (falling back to short/long press mode).

---

### [hotkey]

| Key | Description |
|-----|-------------|
| `enabled` | Set to `true` to enable global keyboard hotkeys, `false` to disable |
| `mute` | Key combination to toggle mute |
| `deafen` | Key combination to toggle deafen |

#### Hotkey format

Combinations are written as `MODIFIER+MODIFIER+KEY`, for example:

```
mute=CTRL+SHIFT+F11
deafen=CTRL+SHIFT+F12
```

Supported modifiers: `CTRL`, `CONTROL`, `SHIFT`, `ALT`, `WIN`

Supported keys: `A`–`Z`, `0`–`9`, `F1`–`F24`, `SPACE`, `TAB`, `ENTER`, `ESC`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`, `UP`, `DOWN`, `LEFT`, `RIGHT`, `NUMPAD0`–`NUMPAD9`

Hotkeys are **global** — they work regardless of which application is in focus. Leave `mute` or `deafen` blank to disable that specific hotkey.

---

## Running

### Normal (with console)
```
discord-hotkey-x64.exe
```

### Silent / background mode
```
discord-hotkey-x64.exe -s
```
The utility launches with the console window hidden, and continues to runs in the background (use Task Manager to stop it, if required).

### Install to Windows startup (runs silently on login)
```
discord-hotkey-x64.exe -install
```

### Remove from Windows startup
```
discord-hotkey-x64.exe -uninstall
```

---

## Building from Source

While you can probably build this just fine with an IDE like Visual Studio, or using WSL, these are the methods we've tried and can recommend (for the purpose of this tutorial, the `chocolatey` package manager was used to obtain and install the required build tools, but you could probably do this with `winget` too, or manually instead).

### Option 1: Native MinGW-w64 (x64 only)
Installs a lightweight, standalone MinGW-w64 toolchain that builds only x64 binaries.

1. Install mingw: `choco install mingw -y`
*This installs MinGW-w64 to C:\ProgramData\mingw64\mingw64\. Close and reopen your terminal, or run refreshenv to update your PATH.*
2. Run `build-mingw-x64.cmd`

### Option 2: MSYS2 (x86, x64, and ARM64)
1. Install msys2: `choco install msys2 -y`
*Installs a full development environment that can build for all three architectures (x86, x64, ARM64).*
2. After installation, launch the MSYS2 terminal and install the required packages:
`pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-i686-gcc mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-lld mingw-w64-clang-aarch64-clang mingw-w64-clang-aarch64-headers mingw-w64-clang-aarch64-crt mingw-w64-clang-aarch64-compiler-rt`
3. Run `build-msys2-all.cmd`

---

## How It Works

1. On launch, the utility connects to Discord's local IPC pipe (`\\.\pipe\discord-ipc-0` through `discord-ipc-9`)
2. It performs an OAuth2 handshake using your `client_id` and `client_secret`
3. On first run, Discord shows an authorization popup — approve it once
4. The utility then monitors your joystick and/or keyboard hotkeys
5. On press, it queries Discord's current mute/deafen state in real time before sending the toggle command, ensuring it always reflects actual state even if you muted via Discord directly
6. If Discord restarts or the joystick is unplugged and then reconnected, the utility should automatically re-establish a connection accordingly

---

## License

MIT License. See `LICENSE.md` for details.

---

## Contributing

Pull requests, forks, issue and suggestion reports are all welcome.

## Coffee

Did this make you happy? I'd love to do more development like this! Please donate to show your support :)

**PayPal**: [![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/donate/?business=2CGE77L7BZS3S&no_recurring=0)

**BTC:** 1Q4QkBn2Rx4hxFBgHEwRJXYHJjtfusnYfy

**XMR:** 4AfeGxGR4JqDxwVGWPTZHtX5QnQ3dTzwzMWLBFvysa6FTpTbz8Juqs25XuysVfowQoSYGdMESqnvrEQ969nR9Q7mEgpA5Zm
