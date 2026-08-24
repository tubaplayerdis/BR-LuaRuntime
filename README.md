# BR-LuaRuntime

This project adds lua to Brick Rigs.

## How it works

 - Switch Bricks are used as Lua Bricks.
 - Switch Bricks run lua every tick.
 - Lua code is stored on TextBricks as "modules"
 - modules can reference each other or be referenced by TextBricks for execution

## IMPORTANT

There are two types of modules
 - Execution modules (Custom to BR-LuaRuntime)
 - Standard modules (generally standard behavior)

## Modules

Modules are defined by using the following syntax **ON THE FIRST LINE** of your TextBrick
`=ModuleName`

### Execution Modules

These modules can be executed by a Switch Brick. 
Execution modules need to globally define two functions:

 - Tick(DeltaTime)
 - Interact(Toggle)

These functions allow you to define the Switch Bricks behavior.

#### USAGE

Use a Execution Module by setting `=ModuleName` as the Switch Name of your Switch Brick

### Regular Modules

 - Act like regular modules (Expose API's via regular module syntax)
 - Live in the global namespace
 - Can reference eachother
 - Can be referenced by Execution modules

## Caveats

 - The global lua state is cleared upon entering a vehicle.
 - There is a limit of 10,000 operations per tick.
 - Persistent values should be stored by setting other SwitchBrick output and input channels (Storage API).

READ THE [API.MD](API REFERENCE)
