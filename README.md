```bash
# Run in TTY mode
mkdir build
cd build
cmake -S .. -B . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && make && sudo ./wm
```

Commands:
- ESC: quits
- W: creates a dummy window
- Q: closes the focused window
