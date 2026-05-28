# AGENTS.md

## Project overview
AQEMU is a Qt5-based graphical frontend for QEMU with optional embedded VNC display support. It also exposes a DBus-backed service to control VMs from the command line (start/stop/etc.) while the UI can exit and leave VMs running (see `src/main.cpp`).

## Key directories
- `src/`: C++ Qt application code, UI controllers, VM model, and service logic.
- `src/Embedded_Display/`: embedded VNC display implementation (libvncclient).
- `src/docopt/`: bundled docopt parser for CLI.
- `resources/`: Qt resources (`*.qrc`), icons, templates, translations, man page, desktop/appdata.

## Build & run
### Meson (recommended in README.md)
```
meson builddir
cd builddir
ninja
./aqemu
```

### Convenience build script
```
./compile.sh [builddir]
```
- If `libvncclient` is available, the script uses Meson.
- Otherwise it falls back to CMake with `-DWITHOUT_EMBEDDED_DISPLAY=on` and a policy minimum for newer CMake.

### CMake (from README)
- Configure options: `DEBUG`, `WITHOUT_EMBEDDED_DISPLAY`, `INSTALL_MAN`, `MAN_PAGE_COMPRESSOR`.
- Example:
```
cmake -DCMAKE_INSTALL_PREFIX=/usr -DMAN_PAGE_COMPRESSOR=bzip2
make
```

## Runtime entry points
- GUI + service entry: `AQEMU_Main::main` in `src/main.cpp`.
- CLI commands are parsed via docopt (`USAGE` in `src/main.cpp`) and dispatched to `AQEMU_Service` (DBus singleton, `src/Service.h`).

## Architecture notes
- **VM model**: `Virtual_Machine` in `src/VM.h` encapsulates VM configuration, persistence (AQEMU XML files), and QEMU argument building.
- **Main window**: `Main_Window` in `src/Main_Window.*` coordinates widgets, menus, and VM state; many UI actions are connected via Qt slots.
- **Service/daemon behavior**: `AQEMU_Service` is a DBus singleton (`SERVICE_NAME`), used for CLI control and for keeping VMs alive after UI exit.

## UI & resource patterns
- UI forms are `.ui` files compiled via Qt’s uic (CMake uses `AUTOUIC`; Meson uses `qt5.preprocess`).
- Icons and shared images are bundled via `resources/*.qrc`.
- Translations are `.ts` files under `resources/translations/`.

## Conventions
- Qt signal/slot style is used extensively (`Q_OBJECT`, `on_*` slots).
- C++11 is the Meson default (`cpp_std=c++11`).

## Testing
No test targets were found in `CMakeLists.txt` or `meson.build`.

## Gotchas
- Embedded VNC display is optional and can be disabled with `WITHOUT_EMBEDDED_DISPLAY` (CMake) but Meson always links `libvncclient` (see `meson.build`).
- CLI control relies on DBus service state; some commands expect an already running VM (see `USAGE` in `src/main.cpp`).
