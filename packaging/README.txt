HORIZON UNSEEN  v@VERSION@
A 2D side-scrolling shoot-'em-up.

Thanks for playing. This is an early build and feedback is the whole point.

RUNNING
  Double-click HorizonUnseen.exe.
  Keep this folder intact - the game loads shaders\ and assets\ from
  alongside the executable.

REQUIREMENTS
  Windows 64-bit, and a GPU with up-to-date drivers supporting Vulkan.
  Nothing else to install - no Visual C++ redistributable needed.

CONTROLS
  WASD / Arrow keys .... Move
  Space ................ Fire
  Left or Right Shift .. Superweapon
  [  and  ] ............ Cycle weapons
  Esc .................. Pause
  ` (backtick) ......... Debug console (stats, logs, playtest shortcuts)

AUDIO
  Volume sliders are on the Options screen (master, effects, music) and
  are remembered between sessions. If no sound device is found the game
  runs silently rather than refusing to start.

------------------------------------------------------------------------
READ THIS BIT - the core mechanic is not obvious
------------------------------------------------------------------------

Your five Energy Cells are BOTH your health AND your superweapon fuel.

Watch the damage bar under the cells. There is a threshold marker on it:

  * While incoming damage stays UNDER the threshold, your ship absorbs it
    and CHARGES your cells.
  * Go OVER the threshold and cells start BREAKING instead.

So taking light chip damage is how you fuel superweapons. Do NOT try to
dodge absolutely everything - you will never charge anything.

Cells break from 5 downward, so cell 1 is the tough one you lose last. A
broken cell only comes back from a Cell Repair drop.

The more cells you have fully charged, the stronger the superweapon that
Shift fires - from a single Piercing Lance at one cell up to the Energy
Bomb at five, which wipes every enemy AND every bullet off the screen.
Save that one; it is a genuine get-out-of-jail card.

GRAZING (matters most in Bullet Hell)
  Bullets that pass CLOSE to you without hitting award charge. In Bullet
  Hell your hitbox shrinks to a few pixels - much smaller than your ship -
  and is drawn as a bright pulsing dot with a faint ring around it. That
  ring is the graze band.

  This means dense patterns are fuel, not just death. Fly close on purpose.

WEAPONS
  Only the basic cannon is unlocked at the start. Kill things, grab drops
  (they are labelled with their name), and cycle with [ and ]. Repeat
  pick-ups of the same weapon level it up, to a maximum of 5.

BULLET HELL
  Every enemy keeps its identity but fires a far denser pattern - orbiter
  spirals, wave-rider ribbons, turret sweeps, mine double-detonations,
  splitter cascades, and a boss counter-spiral. Normally you unlock it by
  finding every secret in the game.

------------------------------------------------------------------------

WHAT FEEDBACK IS USEFUL
  - Does charging actually happen for you in a fight, or are you always
    over the threshold and just losing cells?
  - Is Bullet Hell fun-hard or unfair-hard? Which enemy feels worst?
  - Can you tell the enemy types apart, and read what each one is doing?
  - Anything that felt broken, unclear, or cheap.

  A log is written to logs\HorizonUnseen.log next to the executable -
  please attach it if you hit a bug.

TROUBLESHOOTING
  If it will not start, update your graphics drivers - the game needs
  Vulkan.

  No sound? Check the Options screen sliders first. The game will also
  report "No audio device was found" there if it could not open one.

------------------------------------------------------------------------
THIRD-PARTY SOFTWARE
------------------------------------------------------------------------

Horizon Unseen itself is MIT licensed.

This package includes OpenAL32.dll, which is OpenAL Soft, copyright (C)
1999-2024 the OpenAL Soft authors, licensed under the GNU Lesser General
Public License version 2 or later.

OpenAL Soft is dynamically linked and unmodified. Its complete source is
available at:

  https://github.com/kcat/openal-soft

The exact revision used to build this DLL is tag 1.25.2. You may replace
OpenAL32.dll with your own build of the same library, which is the
relinking right the LGPL grants you.

A copy of the LGPL is included with the OpenAL Soft source above.
