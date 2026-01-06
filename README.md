
# 🚀 SPACEWAR

**SPACEWAR is a local 2 player spaceship shooting game!**

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

- [ ] Add window icon
- [ ] Add main menu exit button
- [ ] Add images section in README.md
