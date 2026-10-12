# Native Loader

A native mod loader for Minecraft Dungeons II. It loads DLL mods into the game, gives them a debug console, and lets Blueprint mods call functions in them.

Downloads, and the Native Mod Template for making mods, are NOT on [Nexus Mods](https://www.nexusmods.com/minecraftdungeons2/mods) yet.

## Installing

Put `version.dll` next to the game's exe, in `Dungeons\Binaries\Win64` on Steam or `Dungeons\Binaries\WinGDK` on the Minecraft Launcher. Mods go in `Dungeons\Content\Paks\~mods`.

## Building

Run `build.bat`. It needs Visual Studio 2022, or the Visual Studio 2022 Build Tools, with the Desktop development with C++ workload. It builds `version.dll`, with intermediate files in `obj`.

## How it works

- The game loads `version.dll` from its own folder, so Native Loader takes its place and forwards all x Version functions to the real one in `System32`.
- For every folder in `~mods`, it loads `<Mod>\<Mod>.dll` if there is one.
- The debug console is its own window, opened when a mod folder has a `console` file in it.
- For the Blueprint bridge, it finds the engine's object list, name table and bytecode handlers in memory by their shape, then replaces the native function of the stock **Get Console Variable String Value** node. Calls whose name starts `NL|` go to the registered DLL function, and all other calls reach the original.
