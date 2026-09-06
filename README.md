# Victory Heat Rally VR — experimental 0.1.0

An unofficial Windows PC VR mod for the Steam version of Victory Heat Rally. Tested by the project owner with Quest 3, Virtual Desktop and a Logitech G29. Requires a purchased copy of the game and SteamVR.

## Features
- Automatic VR racing and a flat menu screen in VR.
- Cockpit and chase cameras with head tracking.
- Cockpit dashboard instruments, course-map display and rear-view mirror.
- Animated sprite hands and steering wheel; slanted windshield and colorful interior.
- Optional G29 steering, pedals, D-pad menus and impact feedback for cones, landings and walls.
- Standard gamepad controls when wheel input is absent or disabled.

## Status
Experimental single-player mod, not an official game update. VR driving and wheel controls have been tested by the project owner. The newest stronger centering and six-meter message panels still need broader testing. Compatibility with other headsets, wheels and game updates is unverified.

## Important: artwork is not bundled
The prototype uses reference artwork whose redistribution permission has not been confirmed. This upload package deliberately excludes it. Installation currently requires a compatible, legally obtained driver image; see assets/README.md. Until redistributable artwork is added, publish this as an experimental source preview, not a turnkey public release.

## Installation
1. Download and extract this repository, and download UndertaleModTool's Windows CLI separately from https://github.com/UnderminersTeam/UndertaleModTool/releases . Keep its entire distribution together.
2. Start with the original supported game. The installer accepts only data.win SHA-256 `2F7161D6250C42FE9AD72F078C2E41C035125C19694D5E41A9B456EB8E20F680`; it refuses modified or different builds.
3. Close the game. Open PowerShell in this folder and run the following, replacing the three paths:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1 -GameDirectory 'D:\SteamLibrary\steamapps\common\Victory Heat Rally' -UndertaleModCli 'D:\Tools\UndertaleModTool\UndertaleModCli.exe' -DriverArtwork 'D:\Assets\VHR-driver.png'
```

The execution-policy option applies to that process only. The installer patches your own data locally, verifies a backup, and then installs the bridge. It does not download the game. Do not distribute files generated in build/.

Start SteamVR and your PC headset connection, then launch the game through Steam. Game files in Program Files may require an elevated installer shell if Windows denies access.

## Controls
| Input | Action |
|---|---|
| Gamepad L3 / F7 / G29 Triangle | Cockpit / chase |
| G29 Circle | Drift |
| G29 D-pad | Menu directions |
| F6 | Recenter |
| F8 | Toggle racing VR / flat presentation |
| F9 | Toggle wheel input |
| F10 | Toggle custom wheel feedback |
| Page Up / Page Down | Seat height |

Wheel feedback starts enabled on supported hardware. Disconnect the wheel or disable it with F9 to use regular gamepad steering. Start with your hands resting lightly on the wheel when testing a new build.

## Restore
Close the game, then run `Restore.ps1 -GameDirectory 'your game folder'`. It restores the verified original data and removes only recognized mod files whose hashes match the installation manifest. The backup is preserved. Do not delete your original backup.

## Source and dependencies
src/ contains the custom native bridge and GML changes. Patch.csx obtains required original routines from the user's game at installation time; those routines are not distributed. See BUILD.md for rebuilding the native DLL. OpenVR and MinHook license notices are in licenses/. The game, trademarks and original game assets belong to their respective owners. No original-game ownership or endorsement is claimed.
