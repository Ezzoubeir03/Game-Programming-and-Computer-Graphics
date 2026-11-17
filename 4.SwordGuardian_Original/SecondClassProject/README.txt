SWORD GUARDIAN - SECOND CLASS PROJECT
====================================

IDE / compiler:
- Microsoft Visual Studio 2010
- Win32 C++ (GDI)
- Windows subsystem (no console window)

Open:
  SecondClassProject.sln

Build:
  Select Debug | Win32, then Build > Rebuild Solution.
  Run with F5 or Ctrl+F5.

Controls:
- ENTER: Start / restart
- A / D or LEFT / RIGHT: Move horizontally
- W / S or UP / DOWN: Move vertically
- SPACE: Throw a sword
- P: Pause / resume
- M: Toggle the original calm background music
- I: Instructions (from menu)
- ESC: Return to menu; ESC from menu exits

Gameplay:
- 10 lives.
- Hit shadow enemies with sword projectiles for 10 points.
- Enemies that reach the left side or collide with the warrior cost one life.
- Difficulty increases every 15 seconds.

Assets:
- assets/background.bmp: supplied background image
- assets/warrior.bmp: supplied warrior image (black background treated as transparent)
- assets/sword.bmp: supplied sword image (black background treated as transparent)
- assets/calm_theme.wav: original synthesized ambient music created for this project

Notes:
- Keep the assets folder beside the project files when running.
- When launching from Visual Studio, the project folder is the working directory.
- The game uses GDI double buffering and a 16 ms timer for smoother animation.
