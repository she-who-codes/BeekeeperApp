# BeekeeperApp
Program to track bee colony health by logging hive inspections.
##Run with Steps
1) cmake -S . -B build -G Ninja
2) cmake --build build
3) .\build\BeeKeeper.exe

## Where your data is stored
BeeKeeper keeps its database in a fixed, per-user location (it no longer depends on the folder the app is started from):
- Windows: `%LOCALAPPDATA%\BeeKeeper\beekeeper.db`

On first start after updating, an existing `beekeeper.db` from the start folder is copied there automatically; the original file is left unchanged as a backup. The path in use is printed to the console at startup.
