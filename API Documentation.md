***

# Yield v1.1.0 — Language API Documentation

## Overview
Yield is a beginner-friendly programming language designed around readable syntax, simple data handling, and safe shared environments. It focuses on explicit variable declaration, unified block structures, and a built-in system for safe multi-script data sharing.

Yield files use the `.yd` extension and run using the `yield` command.

---

## 1. General Syntax Rules

* **File Extension:** `.yd`
* **Blocks:** Blocks are created using a colon `:` and must be closed with the `end` keyword.
* **Comments:** Comments start with `//` and ignore the rest of the line.

```yield
// This is a comment
if condition:
    // code block
end
```

**Valid Blocks:**
* `if` / `elseif` / `else`
* `func`
* `class`
* `run` (Loops)
* `error` (Error handling)

---

## 2. Variables & Constants

### Creating Variables
Variables are created using the `var` keyword. 

```yield
var health = 100
var target = nil
```

### Changing Variables
Variables are modified using the `set` keyword. Writing `name = value` without `var` or `set` is invalid.

```yield
set health = 50
```

### Dynamic Types
Variables do not have fixed types. A variable can change from a number to a string dynamically.

```yield
var value = 100
set value = "Hello"
```

### Constants
Constants are created using the `const` keyword.
* Constants cannot be changed using `set`.
* Constant names automatically become uppercase.

```yield
const max_health = 100
// Becomes MAX_HEALTH internally
```

### nil
`nil` represents an empty or absent value.

---

## 3. Operators

### Assignment vs Comparison
The `=` symbol has different meanings depending on context:
* **Variable Creation/Change:** `var x = 10` or `set x = 20` (Assigns value)
* **Conditions:** `if x = 20:` (Compares values)

### Math Operators
Yield uses standard math operators:
* `+` (Add)
* `-` (Subtract)
* `*` (Multiply)
* `/` (Divide)

### Comparison Operators
* `=` (Equal)
* `-=` (Not Equal)
* `>` (Greater than)
* `<` (Less than)
* `>=` (Greater or equal)
* `<=` (Less or equal)
* `in(a, b)` (Between values)

### Logical Operators
* `and`
* `or`
* `not`

```yield
if alive = true and health > 0:
    // do something
end
```

---

## 4. Control Flow (Conditions)

Conditions use `if`, `elseif`, and `else`. Every block requires a `:` and ends with `end`.

```yield
if health > 0:
    out("Alive")
elseif health = 0:
    out("Dead")
else:
    out("Error")
end
```

---

## 5. Loops

`run` is the only loop keyword. The `:` at the end starts the loop block.

### Infinite Loop
Runs forever until `stop` is used.
```yield
run:
    // code
end
```

### Repeat Loop
Runs a specific number of times.
```yield
run(5):
    // runs 5 times
end
```

### Indexed Repeat Loop
`i` starts at 0 and ends at `number - 1`.
```yield
run(i, 5):
    // i is 0, 1, 2, 3, 4
end
```

### While Loop
Runs while the condition is true. No `while` keyword is needed.
```yield
run(health > 0):
    // code
end
```

### List Loop
The variable receives each value inside the list.
```yield
run(item, inventory):
    out(item)
end
```

### Loop Controls
* `stop` — Exits the current loop entirely.
* `skip` — Skips the current iteration and moves to the next.

---

## 6. Functions

### Definition
Functions are created using `func` and a colon `:`.

```yield
func add_numbers(a, b):
    yield a + b
end
```

### Calling
Functions use parentheses.

```yield
var result = add_numbers(5, 10)
```

### Returning Values
Functions return values using the `yield` keyword.

### Arguments (Pass-by-Reference)
Function arguments are references to the original value. If a function uses `set` on an argument, the original variable is changed.

```yield
func modify_value(val):
    set val = 100
end

var my_num = 5
modify_value(my_num)
// my_num is now 100
```

---

## 7. Classes

Classes create objects.

### Definition & Constructor
```yield
class Player:
    func init(self):
        set self.health = 100
    end
end
```

### Self Rule
Every class function requires `self` as the first argument. Objects store data using `self`.

### Instantiation
Objects are created using the `new` keyword.
```yield
var player1 = new Player()
out(player1.health)
```

---

## 8. Lists

Lists are Yield's universal collection type, denoted by `[]`. They can store indexed values, named values, or a mix of both.

### Indexed Values
Indexes start at 0.
```yield
var my_list = ["Sword", "Shield", "Potion"]
out(my_list[0]) // "Sword"
```

### Named Values
```yield
var player = [
    name = "Alex",
    level = 5
]
out(player.name) // "Alex"
```

### Mixed Values
```yield
var data = [
    "Sword",
    "Shield",
    owner = "Alex"
]
out(data[0])     // "Sword"
out(data.owner)  // "Alex"
```

### Missing Values
Accessing an index or name that does not exist creates an error.

---

## 9. Error Handling

### Throwing Errors
```yield
error("Something went wrong")
```

### Catching Errors
```yield
error:
    // risky code
end
catch e:
    // handle error
    out(e)
end
```

---

## 10. Input & Output

### Output
`out()` displays values to the screen.
```yield
out("Hello World")
```

### Input
`input()` receives user input. **Input always returns a string.**

### Conversion Functions
* `int()`
* `float()`
* `str()`

```yield
// Direct conversion
var age = int(input("Enter age: "))

// Separate conversion
var name = input("Enter name: ")
set name = str(name)
```

---

## 11. Built-in Functions

* `wait()` — Pauses execution.
* `chance()` — Probability/random generation.
* `upper()` — Converts string to uppercase.
* `lower()` — Converts string to lowercase.
* `length()` — Returns length of a string or list.
* `reverse()` — Reverses a string or list.
* `int()`, `float()`, `str()` — Type conversions.

---

## 12. Universal Containers

Universal Containers are global shared storage accessible by every script in the environment. 
Structure: **Container → Entry → List → Values**

### Creating & Removing
* `cont.new(Name)` — Creates a new container.
* `cont.remove(Entry)` — Removes an entry (and its contents) from a container.

**Rules:**
* Multiple containers are allowed.
* Containers cannot contain other containers.
* Containers store Entries. Entries are Lists.

---

## 13. Entry Reservation System

Entries can be owned by scripts to prevent race conditions. Reservation only affects the specific Entry mentioned, not the whole Container.

### Taking Ownership
* `cont.reserve(Entry)` — Immediately takes ownership of an Entry.

### Requesting Ownership
* `cont.request(Entry)` — Requests ownership from another script.
* *Note: Requests have a maximum runtime of 10 seconds.*

### Reservation Checks
* `cont:IsReserved(Entry)` — Returns true/false if the Entry is currently locked.
* `cont:GetReserveOwner(Entry)` — Returns the name of the script currently owning the Entry.

---

## 14. Script Object & Priority

### Script Object
`Script` represents the currently running script.
* `Script.Name` — Returns the script's identity (e.g., `SaveData.yd`).

### Reservation Behavior
If a script tries to edit a reserved Entry:
1. **Default:** The change is cancelled, a warning is shown, and the owner script is displayed.
2. **Wait Mode:** A script can choose to wait until ownership becomes available:
   ```yield
   set Script.Action = Wait
   ```

### Container Priority
Scripts can have priorities to help decide ownership conflicts.
```yield
set Container.Priority(Script, Value)
```

---

## 15. Plugins

External libraries can be loaded using the `plugin` keyword.

```yield
plugin "MathLibrary"
```
This is going to make the `Script` object incredibly powerful. You now have a fully-fledged concurrency management system built directly into the language. 

Here is the updated and highly detailed section for the API documentation. You can swap this out with the old Section 14 in your `.md` file.

***

## 16. The Script Object & Priority

The `Script` object represents the currently running script. It allows scripts to identify themselves, track their execution, and dictate how they behave when interacting with locked Universal Container Entries.

### 16.1 Script Properties

* **`Script.Name`**: Returns the file name of the running script (e.g., `SaveData.yd`).
* **`Script.Id`**: Returns a unique integer ID given to the script when it started running. Useful for debugging and tracking specific instances.
* **`Script.Priority`**: Gets or sets the global priority level of the script. Higher numbers mean higher priority (Default is 0).
* **`Script.Time`**: Returns how long the script has been running, in seconds.

```yield
out("Running script: " + Script.Name)
out("Script ID: " + Script.Id)
out("Uptime: " + Script.Time + "s")
```

### 16.2 Script Actions (`Script.Action`)

When a script attempts to edit a Reserved (locked) Entry, Yield checks `Script.Action` to determine what to do next. 

You can change this behavior at any time during execution:

```yield
set Script.Action = Wait
```

**Available Actions:**

1. **`Cancel` (Default)**
   The change is cancelled. A warning is printed to the console, and the script continues running normally.
2. **`Wait`**
   The script pauses execution and waits in line. The moment the Entry becomes unlocked, the script wakes up and applies its changes.
3. **`Skip`**
   The change is cancelled silently. No warning is printed to the console. Ideal for loops running 60 times a second to prevent console spam.
4. **`Error`**
   The script immediately throws a Yield error. This is useful when a script *must* access the data to function, allowing you to catch the failure using `error:` / `catch e:` blocks.
5. **`Force`**
   The script attempts to steal ownership. If the script's `Priority` is higher than the current owner's priority, it kicks the owner out and takes the Entry. If its priority is lower or equal, it acts like `Cancel`.

### 16.3 Container Priority

Priority helps decide ownership conflicts, especially when using `Script.Action = Force` or when multiple scripts call `cont.request()` at the same time.

You can set priority directly on the script, or apply it to a specific container.

```yield
// Give this script a high priority globally
set Script.Priority = 10

// Or set priority for a specific script on a specific container
set PlayerData.Priority(Script, 10)
```

**Priority Rules:**
* Default priority for all scripts is `0`.
* When two scripts request an entry simultaneously, the one with the higher priority wins.
* If a script with priority `5` uses `Force` on an entry owned by a script with priority `2`, the steal succeeds. If the priorities were reversed, the steal would fail and default to `Cancel`.

***
