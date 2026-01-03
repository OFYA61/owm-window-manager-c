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
