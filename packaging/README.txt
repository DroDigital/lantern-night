LANTERN KNIGHT
==============
A small knight, a dark crypt, and a lantern that is your life.

Your lantern's flame is your health and your light. Take damage and the world
gets darker. Rest at lantern shrines, die, and go back for your embers.

RUNNING THE GAME
----------------
Windows:  unzip, then double-click LanternKnight.exe.
          If SmartScreen warns about an unknown publisher, click
          "More info" > "Run anyway".
macOS:    unzip, then open "Lantern Knight.app".
          The app is not notarized unless the release notes say so.
          If macOS says it cannot verify the developer, right-click the app
          and choose Open (then Open again), or go to
          System Settings > Privacy & Security and click "Open Anyway".
Linux:    tar xzf the archive, then run ./LanternKnight
          (optional: copy lantern-knight.desktop and lantern-knight.png to
          ~/.local/share/applications and ~/.local/share/icons).

No installer or runtime is needed. Keep the assets/ and data/ folders next to
the executable (they are inside the .app on macOS).

CONTROLS (gamepad first)
------------------------
                 Gamepad (Xbox layout)    Keyboard + mouse
Move             Left stick               W A S D (Alt = walk)
Camera           Right stick              Mouse
Light attack     RB                       Left mouse
Heavy attack     RT (hold to charge)      F, or Ctrl + left mouse
Roll / sprint    B (tap / hold)           Space or Shift (tap / hold)
Block / parry    LB                       Right mouse
Oil flask        X                        R
Interact         A                        E
Lock on          R3 (flick to switch)     Q or middle mouse (Z / C or wheel to switch)
Pause            Start                    Esc

SAVES AND SETTINGS
------------------
Windows: %APPDATA%\LanternKnight
macOS:   ~/Library/Application Support/LanternKnight
Linux:   $XDG_DATA_HOME/lantern-knight (or ~/.local/share/lantern-knight)

See CREDITS.md for the artists and libraries that made this possible.
