
# 🚀 SPACEWAR

**SPACEWAR is a local 2 player spaceship shooting game!**

SPACEWAR is written in C with cool game library: [Raylib](https://www.raylib.com/).

## 🖼️  Images

![SPACEWAR main menu screen](docs/main-menu.png)
![SPACEWAR in-game](docs/in-game.png)
![SPACEWAR win dialog](docs/win.png)

## 👷🏻 Building

Raylib files in this project is for Windows. So, if you building in Unix,
make sure to have Raylib installed.

### Windows

```powershell
gcc main.c -o spacewar.exe -O3 -Iinclude -Llib -lraylib -lopengl32 -lgdi32 -lwinmm
```

### Linux

```bash
gcc main.c -o spacewar -O3 -lraylib
```

## ⌨️ Controls

- W/A/S/D to **move** left spaceship
- SPACE to **shoot** for left spaceship
- ARROW KEYS UP/LEFT/DOWN/RIGHT to **move** right spaceship
- COMMA to **shoot** for right spaceship
- ESC To **pause** game

## 📝 Todo

- [x] Add window icon
- [x] Add main menu exit button
- [x] Add images section in README.md
