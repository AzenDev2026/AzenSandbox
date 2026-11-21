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
- 🪟 **Fully self-drawn windows (compositor mode)**: app windows are
  redirected off-screen with XComposite the instant they appear, and the
  sandbox repaints their pixels itself into its own rounded, shadowed
  Windows-11 frames — no app window is ever visible outside the sandbox.
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

Azen Sandbox is a Qt6 Widgets app that speaks the X11 protocol. When you
launch an app from the start menu it is spawned inside its own process group
on the X display, and the sandbox then finds the app's window
(`_NET_WM_PID`, with a friendly adoption fallback for old apps that set no
pid) and **reparents it into the sandbox window** with `xcb`. Input still
goes to the app normally; the sandbox just draws the frame and the taskbar.

On a Wayland session the sandbox runs on XWayland automatically, so it works
on both session types.

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
