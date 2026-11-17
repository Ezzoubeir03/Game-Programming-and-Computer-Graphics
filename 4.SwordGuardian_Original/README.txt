SWORD GUARDIAN - ORIGINAL GRAPHICS VERSION
Visual Studio 2010 / Win32 API / GDI

IMPORTANT: The supplied original images are preserved:
  assets\background.bmp  - original cloud background
  assets\warrior.bmp     - user's original warrior bitmap
  assets\sword.bmp       - user's original sword projectile bitmap
  assets\calm_theme.wav  - calm looping background music

Open SwordGuardian_Original.sln in Visual Studio 2010 and build Debug|Win32.
The project uses the Windows subsystem, so it does not open a CMD console.

Controls:
  Arrow keys or W/A/S/D : move the warrior
  SPACE or left mouse   : throw the sword
  M                     : toggle music
  ENTER                 : restart after game over
  Close window          : exit

The game keeps the supplied cloud background and uses the supplied warrior
and sword BMP files. The BMP sprite transparency key is black (RGB 0,0,0).
Set Project Properties > Debugging > Working Directory to $(ProjectDir)
if Visual Studio does not launch with the project folder as its working directory.
