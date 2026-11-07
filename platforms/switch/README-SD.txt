Party Board (Mario Party 4) for Nintendo Switch - homebrew
==========================================================

Requirements
- A Switch running custom firmware (Atmosphere).
- Your own copy of Mario Party 4 (GMPE01 USA or GMPP01 Europe) as .iso, .gcm, .ciso or .gcz.
  RVZ/WIA will not work: convert it to ISO in Dolphin (right-click > Convert File).

Install
1. Extract the zip to the root of your SD card (this creates sd:/switch/partyboard/).
2. Copy your disc image to sd:/switch/partyboard/ (beside partyboard.nro).
3. Open the Homebrew Menu in TITLE MODE: hold R while opening any installed game.
   From the Album (applet mode) there is too little memory, and the port will close.

Notes
- The game starts directly. To see the launcher menu (language, graphics backend, save file type)
  at startup, turn off "Skip Party Board Main Menu" in the - menu > Interface. In the game, the -
  button opens the other settings.
- The first launch compiles shaders and takes longer; later launches use the cache in cache/.
- Saves and settings: sd:/switch/partyboard/user/
- On first launch, a large memory card file is converted to a 512 KiB one so that saving is fast.
  The original stays as user/MemoryCardA.USA.raw.bak.
- Log for bug reports: sd:/switch/partyboard/partyboard-log.txt. This package turns it on; delete
  that file to turn it off. If the game closes with an error, also include the newest file in
  sd:/atmosphere/crash_reports.
- Controls: A confirms, B cancels (by Switch label); X/Y by label; R = Z; ZL/ZR = analog L/R;
  + = Start. Change them in the port's menu (the - button).
- If it runs slowly: lower "Internal Resolution" in the - menu > Video.
- The picture is 4:3 with bars on the sides. Turning off "Lock 4:3 Aspect Ratio" (- menu > Video)
  stretches it to fill the screen.
