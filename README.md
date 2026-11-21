# 🐳 Azen Sandbox

A tiny **toy desktop** that lives inside a window on your real desktop —
like Windows Sandbox, but made for fun. You get a wallpaper, desktop icons,
a taskbar with a start menu, and a little window manager — and the apps you
launch inside it are **real programs really running**, with their windows
pulled into the sandbox. Windows programs work too, through Wine.

## What it can do

- 🎬 **A 5-second boot animation**: 2 seconds of white-dominant code flooding
  most of the window (with jerky, bursty rhythm like a real boot log) around
  the NVazen Linux logo, then a 3-second macOS-style progress bar, then the
  desktop fades in.
- 🪟 **Self-drawn Windows 11 windows**: app windows are adopted the instant
  they appear (root-level map events) and reparented into the sandbox's own
  rounded, shadowed frames with XShape-clipped corners — no app window is
  ever visible outside the sandbox, and no original decoration is shown.
- 🪟 **Windows 11 look**: centred taskbar, rounded self-drawn window frames
  with soft shadows and caption buttons.
- 🖥️ **A toy desktop** styled after your KDE Plasma look: it reads your
  wallpaper, colors, font and icon theme automatically.
- 🚀 **Start menu**: fixed centred above the taskbar — search box, app grid,
  a Windows (Wine) section, Fullscreen / Borderless toggles and the
  "Shut Down Sandbox" button.
- ⛶ **Fullscreen** (F11) and **borderless** modes; in borderless mode drag
  the empty desktop to move the window.
- 🍷 **Wine inside the sandbox**: Notepad, Calculator, Regedit or any `.exe`
  you pick — the wine window lands inside the sandbox like any other app.
- ⏻ Closing the sandbox shuts down every app that lives in it.

Skip the boot animation with `AZS_NO_BOOT=1 azen-sandbox`.

## How it works

Azen Sandbox is a Qt6 Widgets app that runs inside its **own nested X
server** ([Xephyr](https://www.freedesktop.org/wiki/Software/Xephyr/)). On
startup it launches `Xephyr :101` as the sandbox's private display, then
re-runs itself fullscreen inside it. Every app you launch from the start
menu is spawned inside that display, so its windows exist **only inside the
sandbox** — they can never appear on your real desktop, and closing the
sandbox tears the whole little world down with it.

Inside the nested display the sandbox is the window manager: it finds each
app's window (`_NET_WM_PID`, with a friendly adoption fallback for old apps
that set no pid) and reparents it into its own rounded, shadowed Windows-11
frames. Reparenting is perfectly legal on a real X server — this is why the
nested display exists: doing the same on XWayland can wedge clients in an
unkillable state.

On an X11 session, or when Xephyr is missing, the sandbox falls back to
running on the real display (with a warning). Install Xephyr with
`sudo pacman -S xorg-server-xephyr` for the full sandbox experience.

## Build & run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build -j$(nproc)
./build/azen-sandbox
```

Or install it like a normal app (launcher + icon + app menu entry):

```bash
./install.sh            # or: ./install.sh --uninstall
```

## Windows apps (Wine)

Azen Sandbox detects Wine at startup. If it is missing, the start menu shows
a friendly hint. Install Wine with:

```bash
sudo pacman -S wine        # Arch / NVazen
```

Then open the start menu: Notepad, Calculator, Regedit and "Open a .exe…"
all run inside the sandbox.

## Project layout

```
src/
  main.cpp        entry point (forces the xcb platform)
  boot.cpp        5-second boot splash (code output, logo, progress bar)
  theme.cpp       reads the KDE Plasma look (colors, font, wallpaper, icons)
  apps.cpp        parses .desktop files into launchable entries
  x11.cpp         xcb helpers (reparent, focus, titles, XShape, WM_DELETE …)
  proc.cpp        runs apps in their own process group (setsid)
  embedded.cpp    self-drawn Win11 window frame hosting a foreign X window
  desktop.cpp     wallpaper + desktop icons
  taskbar.cpp     Win11 centred taskbar: start button, tasks, tray, clock
  startmenu.cpp   search + app grid + Wine section
  sandbox.cpp     the toy window manager: discovery, embedding, focus
```

## Notes

- Pure toy: no isolation or security boundary (it shares your session, like
  Wine does). Do not use it to contain anything you would not run normally.
- Some multi-process apps (e.g. Firefox) may show their first window slightly
  differently; most apps embed perfectly.
