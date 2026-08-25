# BR-Lua API

## Limitations

 - Each module is limited to **450 characters**.
 - Each execution tick is limited to **10,000** operations

To navigate around the character, **use modules** to compartmentalize logic and behavior

API's use short names to help with the character restriction

## Modules

 - Modules are how BR-Lua interacts with lua code.
 - Modules are stored on **TextBricks**.
 - Define a module by using the `=ModuleName` syntax on the **FIRST LINE** of your text brick.
 - Set a module to be ran every tick by setting a **Switch Brick's** **Switch Name** Parameter to the same name as your module `=ModuleName`.
 - This will run that module's `Tick(Delta)` function every Tick.
 - Modules ran by Switch Brick's need to **globally define** Tick, otherwise they do not.
 - Modules can be referenced by other Modules. Use the `require("ModuleName")` function.
 - Modules can be used as libraries and are suggested to use the `safe module` practice.

### Example of a module ran by a Switch Brick

```lua
--=Main

function Tick(Delta)
    print(Delta)
end
```

### Example of a module **NOT** ran by a Switch Brick and used as a library

This library is referenced in the module above. Its state is shared between modules.
This module uses the `safe module` practice.

```lua
--=MathLib --Remove the comment prefix (--) when using in BR
local M = {}

function M.clamp(x, lo, hi)
    if x < lo then return lo end
    if x > hi then return hi end
    return x
end

M.PI = 3.14159

return M
```

### Example of a module ran by a Switch Brick that references another module not ran by a switch brick

```lua
--=Main

local MathLib = require("MathLib")

function Tick(Delta)
    print(Delta * MathLib.PI)
end
```

## Behavior

- Program context is reset when entering a vehicle.
- Values do not persist in lua after exiting a vehicle.
- To store values when not in a vehicle, use the `Store` API
- Modules do not run at the same time
- Modules run on a switch brick's `TickBrick` function
- The **Current Brick** is the active brick being executed on.
- The **Current Brick** is synonymous with `this brick`
- The **Current Brick** is set before the script runs to the brick it will be executed on.
- The `In` and `Out` API's are relevant to the **Current Brick**
- The `Store` API is relevant to an associated brick in the vehicle

## `In` Namespace

Namespace relevant to dealing with the input channel of the **Current Brick**.

The way input channels work in Brick Rigs is they have a value (Computed from the bound output channels) and references to their bound output channels.
This API provides access to modify both.

These channels are accessed via index where to index is relevant to the order which the bricks are selected when selecting the output channels to bind.

Indexing starts at `0` for the first object and so on.

Use `-1` to access this bricks computed input channel value. (vanilla value)

---

### `Get(ChannelIndex)`

Gets the value of a channel based on index. (**See Above**)

**Parameters:**
*   `ChannelIndex` (**number**): The channel to get the value from by index. (**See Above**)

**Returns:**
*   `number`: The value of the channel.

**Example:**
```lua
local XY = In.Get(-1) --Reads the input channel value of the current brick
local X = In.Get(0) --Reads the first bound output channel value
local Y = In.Get(1) --Reads the second bound output channel value
```

---

### `Set(ChannelIndex, Value)`

Sets the value of a channel based on index. (**See Above**)

**Parameters:**
*   `ChannelIndex` (**number**): The channel to set the value from by index. (**See Above**)
*   `Value` (**number**): The value to set the channel to.

**Returns:**
*   None

**Example:**
```lua
In.Set(-1, 123) --Writes this bricks input channel value
In.Set(0, 45) -- Writes the first bound bricks output channel value
In.Set(1, 67) -- Writes the second bound bricks output channel value
```

---

## `Out` Namespace

Namespace relevant to dealing with the output channel of the **Current Brick**.

The way output channels work in Brick Rigs is they have a single value.

---

### `Get()`

Gets the value of this brick's output channel. (**See Above**)

**Parameters:**
*   None

**Returns:**
*   `number`: The value of the channel.

**Example:**
```lua
local Val = Out.Get() --Current value of this brick's output channel
```

---

### `Set(Value)`

Sets the value of this brick's output channel.

**Parameters:**
*   `Value` (**number**): The value to set the output channel to.

**Returns:**
*   None

**Example:**
```lua
Out.Set(123) --Writes this brick's output channel value
```

---

## `Store` Class

Class relevant to storing values in a Brick that will outlive the context of the program.

Store objects store values in **other** switch brick's output channel. 

Store objects find the associated switch brick by name (Has to be the same as the switch brick's `Switch Name`)

---

### `Store(SwitchName)`

Creates a new storage object associated to a switch of `SwitchName` in the active vehicle (**See Above**)

**Parameters:**
*   `SwitchName` (**string**) The Switch Name of the associated switch brick

**Returns:**
*   `Store`: A new Store object

**Example:**
```lua
local MyCustomVar = Store("MyCustomVar") --Store object linked to a switch brick that has a switch name of "MyCustomVar" output channel
```

---

---

### `Get()`

Gets the value of the other bricks output channel (**See Above**)

**Parameters:**
*   None

**Returns:**
*   `number`: The value of the output channel

**Example:**
```lua
local Val = MyCustomVar:Get() --Current value of "MyCustomVar"s output channel
```

---

### `Set(Value)`

Sets the value of the other brick's output channel

**Parameters:**
*   `Value` (**number**): The value to set the output channel to

**Returns:**
*   None

**Example:**
```lua
MyCustomVar:Set(123) --Writes "MyCustomVar"s output channel value
```

---

## Complete Example

```lua
--=Main
LDT = Store("LastDeltaTime") --Stored in another brick

function Tick(Delta)
    local X = In.Get(0)
    local Z = In.Get(1)
    local XZ = In.Get(-1)
    print("X" .. X)
    print("Z" .. Z)
    Out.Set(Delta)
    LDT:Set(Delta)
    print(LDT:Get())
end
```

---

## Lua std API's exposed

 - `math`
 - `table`
 - `string`

---

## Lua std global functions exposed

 - `print`
 - `tostring`
 - `tonumber`
 - `pairs`
 - `ipairs`
 - `type`
 - `assert`
 - `error`
 - `pcall`
 - `select`
 - `unpack`
 - `require` (Custom Implementation)
