###Latest news:
05/2026
added UEFI and TPM Support for more modern OS, Passtrough direct selector for any pci.e device and native blockdevice selector added
fixed a lot , really alot .

Example how to build using meson/ninja:
```
meson builddir
cd builddir
ninja
./aqemu
```


![ScreenShot](https://i.imgur.com/PkvFUEk.png)

Current stable release: https://github.com/tobimensch/aqemu/releases/tag/v0.9.2
See the CHANGELOG for details.

Upgrading from 0.8.2 is highly recommended.

---

Port of AQEMU 0.8.2 from Qt4 to Qt5.

---

Use cmake to build.

Dependencies: 
 - Qt5Core
 - Qt5Widgets 
 - Qt5Network
 - Qt5Test
 - Qt5PrintSupport
 - Qt5DBus
 - LibVNCServer


---

As an alternative to cmake the meson build system is also supported:
https://github.com/mesonbuild/meson

