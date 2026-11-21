# 🐳 Azen Sandbox

A tiny **toy desktop** that lives inside a window on your real desktop —
like Windows Sandbox, but made for fun. You get a wallpaper, desktop icons,
a taskbar with a start menu, and a little window manager — and the apps you
launch inside it are **real programs really running**, with their windows
pulled into the sandbox. Windows programs work too, through Wine.

## What it can do

- 🖥️ **A toy desktop** styled after your KDE Plasma look: it reads your
  wallpaper, colors, font and icon theme automatically.
- 🚀 **Start menu**: search box, app grid, a Windows (Wine) section and a
  "Shut Down Sandbox" button.
- 🪟 **A mini window manager**: move windows by their title bar, minimize,
  maximize and close them; every window gets a button in the taskbar.
- 🍷 **Wine inside the sandbox**: Notepad, Calculator, Regedit or any `.exe`
  you pick — the wine window lands inside the sandbox like any other app.
- ⏻ Closing the sandbox shuts down every app that lives in it.

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
  theme.cpp       reads the KDE Plasma look (colors, font, wallpaper, icons)
  apps.cpp        parses .desktop files into launchable entries
  x11.cpp         xcb helpers (reparent, focus, titles, WM_DELETE …)
  proc.cpp        runs apps in their own process group (setsid)
  embedded.cpp    window frame that hosts a foreign X window
  desktop.cpp     wallpaper + desktop icons
  taskbar.cpp     bottom panel: start button, tasks, tray, clock
  startmenu.cpp   search + app grid + Wine section
  sandbox.cpp     the toy window manager: discovery, embedding, focus
```

## Notes

- Pure toy: no isolation or security boundary (it shares your session, like
  Wine does). Do not use it to contain anything you would not run normally.
- Some multi-process apps (e.g. Firefox) may show their first window slightly
  differently; most apps embed perfectly.
