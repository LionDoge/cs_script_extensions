## Extension system for scripting in CS2

> [!NOTE]
> This plugin currently serves mostly as reference for extending cs_script's functionality. It is buildable, with a mostly usable API, and a few example scripting extensions. There's currently no public or stable API to interact from other plugins, however it should be possible to add new script functions easily to this plugin directly. There are definitely many things that could be still improved. The plugin has not been yet thoroughly tested in production.

cs_script uses the V8 JavaScript engine to run, if you want to implement your own functionality, you should ensure that you understand some basic concepts on how to work with it: https://v8.dev/docs/embed. Maybe in the (ambitious) future, there could be a simplified API that can abstract some of this away.

> [!WARNING]
> Current state: Builds, and works on Windows. Linux builds, but is completely untested for now.

## Usage
This plugin installs just like any other Metamod plugin.

In a development environment, you will likely want to be able to use this plugin inside tools mode when working with scripts, the classic way of using gameinfo to load Metamod can cause some issues with tools. You can use this custom launcher to avoid issues: https://github.com/Poggicek/metamod-launcher

To run your addon in tools mode, you need to run it from the command line. eg: `metamod-launcher.exe -insecure -tools -addon youraddon`. Of course make sure that this plugin is also installed.

If you wish you can also develop outside of tools, by using some of the commands mentionted below:

## Console commands
This plugin additonally provides some commands to make developing, and testing easier.

- `csscript_load <script_file> [script_entity_name]` - Create a script entity, and execute a raw .js file, skipping the asset system. If a name isn't provided the default name will be 'script'. Path is relative to the game directory.
- `csscript_reload <script_entity_name>` - Reloads a script loaded with csscript_load.
- `script_summary` - Lists debug info for all scripts in the map.

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
It's easiest to use the provided dockerfile, as it will ensure that the plugin gets built in the correct environment, that's compatible with all servers. Docker needs to be installed on the system.
```
docker compose up
```
Output can be found in the `dockerbuild/package/` directory

## Some basic docs and info...
Documentation should be updated here over time, for now you can check out these files to help you get a look at the API:
- `scriptExtensions/scriptextensions.h`
- `scriptExtensions/csscript.h`

... and the corresponding cpp files, for the implementations

On how to actually register custom functions and templates, check `RegisterScriptFunctions` in `plugin.cpp`.

### Interactions with entity objects
- Entities have a control internal field, that's just a pointer to a global value inside the binary, which is checked in every native function. This makes it a bit inconvienient to create entities manually. However the API provides abstractions for sigscanned functions that assign them properly:
  - `CreateEntityObjectAuto(entity)` - will create the best matching JS entity object class to the entity instance that's passed in. Inside the game code it's just a few if checks, this obviously only works for native classes (not our custom ones). For example it will know to assign BaseModelEntity or CSPlayerController, etc. if these are the actual entity types.
  - `CreateEntityObjectFromTemplate(templateName, entity)` - will create a JS object based on the function template name, and will assign the entity data inside the internal fields. `CreateEntityObjectAuto` calls this internally. Useful when you have custom type that inherits from Entity class.
- Extracting entity handles from the object is a bit simpler, we don't really have to care for the entity marker here, thus a signature is not required, the `GetEntityInstanceFromScriptObject` abstracts it away.

### Script contexts, and invoking callbacks
- V8 has it's own context system, but CS additionally uses a global variable that stores the index of the currently running script. Native functions check for this too, so it's required to be set properly. This can be done with `SwitchScriptContext`. The currently active script can be retrived with `GetCurrentCsScriptInstance`. The functions for invoking callbacks take care of this automatically.
- Callbacks are registered inside a hashmap by name, to invoke all matching callbacks registered on all script, the `InvokeCallback` function can be used. However it doesn't provide return values right now (as there could be multiple).
It is possible to manually get scripts and invoke callbacks on them individually, with a function of the same name. This one however provides access to the return value. See an example of this in `Hook_PostEvent` inside `plugin.cpp`

### Notes about how to reverse and find signatures yourself
... Coming soon, it's a bit to go through ...

## Included examples
These are the currently included functions.
These include extensions for existing types, a custom new class, and a custom callback function. Every one of those can be treated as experimental, as they have been tested only in some basic scenarios. Use at your own risk!

```ts
class Domain {
  ShowHudHintAll(text: string, isAlert: boolean): void;
  /** Listens for a protobuf message originating from the server with the specified (partial) name,
    * callback can return false to block the message or true to let it through */
  OnUserMessage(config: { messageName: string, callback: (info: UserMessageInfo) => boolean }): void;
  CreateUserMessage(messageName: string): UserMessageInfo;
}

export class Entity {
  SetMoveType(moveType: number): void;
  /** @experimental Access a schema field for this entity
   * This is currently limited to only atomic fields, implementation is also not very efficient */
  GetSchemaField(className: string, fieldName: string): Entity | number | string | boolean | undefined;
}

export class CSPlayerController {
  ShowHudHint(text: string, isAlert: boolean): void;
  /** Shows a hud message supporting HTML syntax for text effects */
  ShowHudMessageHTML(htmlText: string, duration: number): void;
}

export class UserMessageInfo {
  GetField<T>(fieldName: string): T;
  SetField(fieldName: string, value: any): void;

  RemoveRecipient(playerSlot: number): void;
  AddRecipient(playerSlot: number): void;
  AddAllRecipients(): void;
  ClearRecipients(): void;

  Send(): void;
}
```