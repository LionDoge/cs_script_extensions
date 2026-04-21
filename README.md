## Extension system for scripting in CS2

> [!NOTE]
> This plugin is meant to be a system for extending cs_script's functionality, while also including some new functions by itself if needed. There's currently no public or stable API to interact from other plugins for now, however it should be easily possible to add new script functions to this plugin directly. There are still some features, and implementations that could be improved. The plugin has not been yet thoroughly tested in production.

cs_script uses the V8 JavaScript engine to run, if you want to implement your own functionality then you can read the following guide: https://v8.dev/docs/embed. This goes in hand with how JavaScript works with [objects](https://developer.mozilla.org/en-US/docs/Learn_web_development/Extensions/Advanced_JavaScript_objects). Maybe in the (ambitious) future, there could be a simplified API that can abstract some of this away for plugin makers.

## Usage

- [Releases page](https://github.com/LionDoge/cs_script_extensions/releases)

Latest development builds:
- [Linux](https://nightly.link/LionDoge/cs_script_extensions/workflows/build/master/Linux.zip)
- [Windows](https://nightly.link/LionDoge/cs_script_extensions/workflows/build/master/Windows.zip)

This plugin installs just like any other Metamod plugin. Extract the latest release to `game/csgo/addons/`

In a development environment, you will likely want to be able to use this plugin inside tools mode when working with scripts, the classic way of using gameinfo to load Metamod can cause some issues with tools. You can use this custom launcher to avoid issues: https://github.com/Poggicek/metamod-launcher

To run your addon in tools mode, you need to run it from the command line. eg: `metamod-launcher.exe -insecure -tools -addon youraddon -retail -gpuraytracing`. Of course make sure that this plugin is also installed.

If you wish you can also develop outside of tools, ypu can use some of the commands mentionted below.

## Console commands
This plugin additonally provides some commands to make developing and testing easier.

- `csscript_load <script_file> [script_entity_name]` - Create a script entity, and execute a raw .js file, skipping the asset system. If a name isn't provided the default name will be 'script'. Path is relative to the game directory.
- `csscript_reload <script_entity_name>` - Reloads a script loaded with csscript_load. Do note that currently this does not invoke the `OnScriptReload` callbacks!
- `script_summary` - Lists debug info for all scripts in the map.
- `remove_scripts` - Remove all point_script entities in the map.

## Other features
- Print line numbers and call stack during script exceptions
- Create a global script during loading of any map, that's called `mapspawn.js` in (game/scripts) ~~or `mapspawn.vjs_c` (in game/scripts/ of any addon) - in this priority, if available.~~ <- ((doesn't work now cause the engine is grumpy with loading scripts when it comes to precaching them or whatever.)) Do note, that a script from one addon will take presence over the other ones (this will likely change later to support multiple).
  - This feature can be disabled with `mm_enable_mapspawn_script 0`

## Included features
Below you'll find a definition of added funcions

They include extensions for existing types, a custom new class, and a custom callback function. Every one of those can be treated as experimental, as they have been tested only in some basic scenarios. Use at your own risk!

If you wish to disable some, or all of them, check out the `configs/features.jsonc` file

**For mapmakers:** It is recommended to handle the case where script extensions are unavailable (if possible). You can do so by checking this: `Instance["scriptExtensionsVersion"] !== undefined`. Check for it each time you're using one of the extension functions, and fallback to a different implementation if possible. This will ensure that your map will work at all if the server does not have script extensions, or if there's a period of breakage after an engine update. The same field can be used to check for the current script version in case of API changes.
```ts
class Domain {
  ShowHudHintAll(text: string, isAlert: boolean): void;
  /** Listens for a protobuf message originating from the server with the specified (partial) name,
    * callback can return false to block the message or true to let it through */
  OnUserMessage(config: { messageName: string, callback: (info: UserMessageInfo) => boolean }): void;
  CreateUserMessage(messageName: string): UserMessageInfo;
  /** Emits a specified soundevent by name. If source entity is not provided, it will be played globally. if recipients are not specified, it will be played for all players */
  EmitSound(config: { soundName: string, source?: Entity, volume?: number, pitch?: number, recipients?: CSPlayerController[] }): void;
  /** Retrieves value of a convar */
  GetConVarValue(cvar: string): CvarValue
  /** Prints a message to the chat, supports special characters for color codes */
  PrintToChatAll(message: string): void;
  /** Listens for a command from clients, callback can return false to block the command or true to let it through 
   * Players can invoke commands when not fully connected too, so make sure to check for that if necessary. */
  OnDispatchClientCommand(callback: (playerSlot: number, arguments: string[]) => boolean): void;
  /** Listens for a command from clients, unlike OnDispatchClientCommand, it allows to listen for unregistered commands.
   * However, this callback cannot be used to block any command */
  OnClientCommand(callback: (playerSlot: number, arguments: string[]) => void): void;
}

type CvarValue = string | number | boolean | Vector | QAngle | Color | undefined;

export class Entity {
  SetMoveType(moveType: number): void;
  /** @experimental Access value of a schema field for this entity
   * This is currently limited to only atomic fields, meaning that it does not support nested components */
  GetSchemaField(className: string, fieldName: string): Entity | number | string | boolean | undefined;
  /** Override the transmission of this entity to the target player 
   * @note Use responsibly, do not deny transmission of player pawn to it's owner, or weird things might happen, including players crashing. */
  SetTransmitState(controller: CSPlayerController, state: boolean): void;
  /** Set whether the entity will be transmitted to all players, will apply to newly connected clients too */
  SetTransmitStateAll(state: boolean): void;
}

export class CSPlayerController {
  ShowHudHint(text: string, isAlert: boolean): void;
  /** Shows a hud message supporting HTML syntax for text effects */
  ShowHudMessageHTML(htmlText: string, duration: number): void;
  /** Retrieve SteamID64 of this player */
  GetSteamID(): number;
  /** Respawns this player's pawn */
  Respawn();
  /** Prints a message to this player's chat */
  PrintToChat(text: string): void;
}

export class UserMessageInfo {
  /** @note accessing repeated fields is not supported yet */
  GetField<T>(fieldName: string): T;
  /** @note accessing repeated fields is not supported yet */
  SetField(fieldName: string, value: any): void;

  RemoveRecipient(playerSlot: number): void;
  AddRecipient(playerSlot: number): void;
  AddAllRecipients(): void;
  ClearRecipients(): void;

  Send(): void;
}
```

## Building
> [!IMPORTANT]
> There's likely a bug in Metamod:Source right now, which requires changing one line to build right now: In `metamod-source\core\sourcehook\sh_memfuncinfo.h` at **line 31**: edit `return input;` to `return (OutputClass)input;`. If using Docker this is handled in the dockerfile.

### Windows
Visual Studio with C++ tools, Python, and [AMBuild](https://wiki.alliedmods.net/Ambuild) are required. A copy of Metamod:Source's code is also required from: https://github.com/alliedmodders/metamod-source

Launch "x64 Native Tools Command Prompt for VS", which can be found through the start menu, assuming Visual Studio is installed. Then run the following:
```powershell
set HL2SDKCS2="\path\to\this\repo\sdks\hl2sdk-cs2"

mkdir build && cd build
python ../configure.py --enable-optimize -s cs2 --targets=x86_64 --mms_path="\path\to\metamod-source"
ambuild
```

Output can be found in the `build/package/` directory

### Linux
It's easiest to use the provided dockerfile, as it will ensure that the plugin gets built in the correct environment that is compatible with all servers. Docker needs to be installed on the system.
```
docker compose up
```
Output can be found in the `dockerbuild/package/` directory

## Info for developers
> [!NOTE]
> Exposed API is not yet available for other plugins, at this time, but you can fork this plugin directly for your own uses, or PR new functionality.

Documentation should be updated here over time, for now you can check out these files to help you get a look at the API:
- `scriptExtensions/scriptextensions.h`
- `scriptExtensions/csscript.h`

... and the corresponding cpp files, for the implementations

On how to actually register custom functions and templates, check `RegisterScriptFunctions` in `plugin.cpp`.

### Interactions with entity objects
- Entities have a control internal field, that's just a pointer to a global value inside the binary, which is checked in every native function by the pointer value itself. This makes it a bit inconvienient to create entities manually. However the API provides abstractions for sigscanned functions that assign them properly:
  - `CreateEntityObjectAuto(entity)` - will create the best matching JS entity object class to the entity instance that's passed in. Inside the game code it's just a few if checks, this obviously only works for native classes (not our custom ones). For example it will know to assign BaseModelEntity or CSPlayerController, etc. if these are the actual entity types.
  - `CreateEntityObjectFromTemplate(templateName, entity)` - will create a JS object based on the function template name, and will assign the entity data inside the internal fields. `CreateEntityObjectAuto`. calls this internally. Useful when you have custom type that inherits from Entity class. This function will also properly add the entity object to a hashtable which maps entity handles to JS objects, which is used for storing values inside entities, as explained in the official documentation.
  - Extracting entity handles from the object is a bit simpler, as a signature is not required. However it is recommended to check for the actual underlying type. There are  utility functions in `scriptcommon.h` that can verify and unwrap different argument types, including entities, it's highly recommended to use them.

### Script contexts, and invoking callbacks
- V8 has it's own context system, but CS additionally uses a global variable that stores the index of the currently running script. Native functions check for this too, so it's required to be set properly. This can be done with `SwitchScriptContext`. The currently active script can be retrived with `GetCurrentCsScriptInstance`. The functions for invoking callbacks take care of this automatically.
- Callbacks are registered inside a hashmap by name, to invoke all matching callbacks registered on all scripts, the `InvokeCallback` function can be used. However it is still recommended to invoke callbacks one by one per script, as it will let you create arguments for passing in their proper contexts. This can be done with a function of the same name on the `CSScript` object. See an example of this in `Hook_PostEvent` inside `plugin.cpp`

### Notes about how to reverse and find signatures yourself
There's some useful info in gamedata file. This section might get more filled in later.
