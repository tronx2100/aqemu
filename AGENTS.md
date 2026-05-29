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

### Convenience scripts
```
./compile.sh [builddir]
./create-deb.sh [builddir] [dist]
```
- `compile.sh` uses Meson when `libvncclient` is available, otherwise CMake with `-DWITHOUT_EMBEDDED_DISPLAY=on`.
- `create-deb.sh` builds a Debian package into `dist/` (uses Meson when `libvncclient` is available, otherwise CMake fallback).

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

### VFIO PCIe Passthrough Editor

**New files**:
- `src/VM_PCI_Device.h/.cpp` — Data model for VFIO passthrough device configuration (Host_Address, Bus, Addr, Multifunction, XVGA, ROMFile, Disable_VGA, Disable_Idle, Additional_Flags). Includes `PCI_Host_Device` struct for runtime-detected info (IOMMU group, driver, device name). `To_QEMU_Device_Arg()` builds the `-device vfio-pci,host=...` string. `Build_Root_Port_Args()` auto-generates `ioh3420` root port devices based on unique bus IDs.
- `src/VFIO_PCI_Editor_Window.h/.cpp` — QDialog with:
  - Filter text field + "Show All Devices" checkbox
  - QTableWidget showing PCI devices with columns: selection checkbox, IOMMU group, status LED (green = alone in group, yellow = shared), address, device name, vendor:device ID, driver
  - Flag configuration panel (shown for selected device): Bus, Addr, Multifunction, x-vga, Disable VGA, Disable Idle, ROM file browse, additional custom flags list
  - Reads `/sys/bus/pci/devices/` for device list, IOMMU groups, driver bindings; parses `lspci -nns` output for human-readable names

**Modified files**:
- `src/VM.h` / `VM.cpp` — Added `QList<VM_PCI_Device> PCI_Devices` member with getter/setter, copy constructor, operator==, operator=, destructor cleanup, XML save/load (numbered element names like `PCI_Device_0`), QEMU arg generation in `Build_QEMU_Args()` (both vfio-pci and ioh3420 root port devices)
- `src/Main_Window.h/.cpp` — Added `on_TB_Show_VFIO_PCI_Editor_Window_clicked()` slot; toolbar button "VFIO PCI" in `Tool_Bar_VM_Manage`; copies PCI devices in `Create_VM_From_Ui()`
- `CMakeLists.txt` — Added new source/header files

**QEMU output example**:
```
-device vfio-pci,host=04:00.0,bus=root.1,addr=00.0,multifunction=on,x-vga=on
-device ioh3420,bus=pcie.0,addr=10.0,multifunction=on,port=1,chassis=1,id=root.1
```

## Fixes applied (May 2026)

### VM state never propagated to Main_Window after Power On
**Root cause**: `AQEMU_Service::start()` connected the VM's `State_Changed` signal to `vm_state_changed` slot *after* calling `vm->Start()`. Because `QEMU_Process::waitForStarted()` consumes the startup-pipe notification, the `started()` signal and thus `QEMU_Started()` → `Set_State(VMS_Running)` → `State_Changed` could fire before the `connect()` was established, losing the state transition.

**Fix (Service.cpp)**: Moved `connect(vm, State_Changed, ...)` to *before* `vm->Start()`.

**Secondary issue**: The DBus-based state callback (`AQEMU_Service::vm_state_changed` → DBus `org.aqemu.main_window` → `Main_Window::VM_State_Changed(const QString&, int)`) can fail if the Main_Window isn't properly registered on the session bus.

**Fix (Service.h + Main_Window.cpp)**: Added direct Qt signal `vm_state_changed_signal(Virtual_Machine*, VM::VM_State)` from `AQEMU_Service` to `Main_Window::VM_State_Changed_Direct`, bypassing DBus entirely.

**Reliability fix (Main_Window.cpp)**: `VM_State_Changed_Direct` matches VMs by `Get_UID()` (always set from Main_Window) instead of `QFileInfo` path comparison, which can fail on non-normalized paths.

### Files changed
- `src/Service.h:99` — added `vm_state_changed_signal`
- `src/Service.cpp:124` — emits `vm_state_changed_signal` before DBus call
- `src/Service.cpp:401` — moved `connect(State_Changed)` before `vm->Start()`
- `src/Main_Window.h:82` — added `VM_State_Changed_Direct` slot
- `src/Main_Window.cpp:196-199` — connects to `vm_state_changed_signal` in constructor
- `src/Main_Window.cpp:299-311` — `VM_State_Changed_Direct` implementation (UID matching)

### Layout: right column in VM area not vertically aligned with left column
**Problem**: The "Advanced" button in `gridLayout_8` appeared above the "Name:" label in `gridLayout_7` due to different widget heights (push button vs line edit).

**Attempted fix (reverted)**: Shifting items inside `gridLayout_8` by 1 row had no effect because both child grids (`gridLayout_7` / `gridLayout_8`) share the same parent row of `gridLayout_12`.

**Fix (current)**: Added a 28px fixed vertical spacer at `row="0" column="2"` of the parent `gridLayout_12`, moved `gridLayout_8` to `row="1" column="2"`, and made `gridLayout_7`, `Widget_for_General_Tab`, and `horizontalSpacer_4` span both rows (`rowspan="2"`). This pushes the entire right column down by one grid row.

**File**: `src/Main_Window.ui` — `gridLayout_12`
