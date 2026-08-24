# BR-Lua API

#### Lua API's exposed

 - `math`
 - `table`
 - `string`

#### Lua global functions exposed

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

## BR-Lua Functions

### `GetInChannelVal(ChannelIndex)`

Gets the value of an output channel binded to this bricks input channel by index (Uses 0 indexing). 
Index starts based on selection order of bricks
Use `-1` to get the value of the input channel of this Brick (vanilla behavior)

**Parameters:**
*   `ChannelIndex` (**number**): The input channel to get the value from (**-1**) for the default vanilla behavior

**Returns:**
*   `number`: The value of the input channel.
  
---

### `SetOutChannelVal(Value)`

Sets the value of this bricks output channel

**Parameters:**
*   `Value` (**number**): The value to set the output channel to

**Returns:**
*   none

---

TODO: Finish

## Standard Modules (OOP abstraction over functions)
