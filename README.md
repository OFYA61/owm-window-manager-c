# OWM toy window manager

A small project to learn how to interact with the linux DRM and create a wayland client from scratch. It can spawn dummy windows, which can be moved around and resized.

```bash
mkdir build
cd build
cmake -S .. -B . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TYPE=RELEASE && make
# Run in TTY mode (Ctrl+Alt+F3)
sudo ./wm linux
# Run as wayland client
sudo ./wm wayland
```
Commands:
- ESC: quits
- W: creates a dummy window
- Q: closes the focused window

### Generating wayland protocal files for wayland backend
The `xdg-shell-client-protocol.h` and `xdg-shell-protcol.c` files are created with the following code.
```bash
find / -name "xdg-shell.xml" # If your xdg-shell.xml is in a different spot
wayland-scanner client-header /usr/share/qt6/wayland/protocols/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
wayland-scanner private-code /usr/share/qt6/wayland/protocols/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
```
