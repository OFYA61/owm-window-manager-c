```bash
# Run in TTY mode (Ctrl+Alt+F3)
mkdir build
cd build
cmake -S .. -B . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TYPE=RELEASE && make && sudo ./wm
```

Commands:
- ESC: quits
- W: creates a dummy window
- Q: closes the focused window


### Generating wayland protocal files for wayland backend
```bash
wayland-scanner client-header /usr/share/qt6/wayland/protocols/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
wayland-scanner private-code /usr/share/qt6/wayland/protocols/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
```
