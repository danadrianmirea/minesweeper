# Raylib Minesweeper

A modern implementation of the classic Minesweeper game built with raylib, supporting both desktop and web platforms. This project combines the timeless gameplay of Minesweeper with modern graphics and responsive design.
Play on itch.io: https://adrianmirea.itch.io/

## Features

- **Classic Minesweeper Gameplay**: Traditional minesweeper mechanics with a modern twist
- **Cross-Platform Support**: Play on desktop or in your web browser
- **Responsive Design**: Dynamic resizing support for desktop, web, and mobile web
- **Modern Graphics**: Clean and polished visual design
- **Automated Builds**: Easy deployment to itch.io or other platforms

## Building the Project

### Desktop Build (CMake)

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Configure and build:
```bash
cmake ..
cmake --build . --config Release
```

The executable will be created in the `build` directory.

### Web Build (Emscripten)

To build for web platforms, simply run:
```bash
./build_web.sh
```

This will:
- Build the project using Emscripten
- Generate a web-compatible build
- Create a `web-build.zip` file ready for itch.io deployment

## Project Structure

- `src/`: Source code directory
- `lib/`: Library dependencies
- `Font/`: Font assets
- `build/`: Desktop build output
- `web-build/`: Web build output
- `CMakeLists.txt`: CMake build configuration
- `build_web.sh`: Web build script
- `custom_shell.html`: Custom HTML shell for web builds

## How to Play

1. Left-click to reveal a cell
2. Right-click to flag a potential mine
3. Clear all non-mine cells to win
4. Avoid clicking on mines!

## Technical Details

### Render to Texture Approach

The game uses a render-to-texture approach to ensure:
- Consistent visuals across different screen sizes
- Proper scaling on mobile devices
- Smooth resizing on desktop platforms

### Dynamic Resizing

The game automatically handles:
- Window resizing on desktop
- Browser window resizing
- Mobile device orientation changes
- Different screen resolutions

## License

This project is licensed under the terms specified in the `LICENSE.txt` file.
