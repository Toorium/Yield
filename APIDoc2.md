# Language Specification API Documentation

## Overview

This language is a strict prototype-based OOP scripting language designed for game and application development.

The main philosophy:

- Objects define behavior and type.
- Configurations define variations.
- Objects have fixed structures.
- Missing properties cause errors.
- `nil` is allowed, but missing values are not.
- `{}` is reserved for data.
- `[]` is reserved for code blocks.

---

# Comments

```
// This is a comment
```

---

# Values

## Booleans

```text
true
false
```

## Nil

```text
nil
```

`nil` represents an intentional empty value.

A missing variable/property is different and causes an error.

---

# Operators

## Comparison

```text
==
!=
<
>
<=
>=
```

## Logic

```text
and
or
not
```

## Arithmetic

```text
+
-
*
/
%
```

## Assignment

```text
=
+=
-=
*=
/=
```

## Constants

Constants are created using:

```text
-=-
```

Example:

```text
GRAVITY -=- 9.8
```

A constant cannot be changed afterward.

---

# Variables

Variables require declaration before use.

Example:

```text
health = 100
name = "Player"
```

Inside functions:

- New variables are local.
- Existing declared variables can be modified.

Example:

```text
health = 100

[function Damage():

    health -= 10

]
```

---

# Code Blocks

All control-flow and function structures use:

```text
[keyword:
    code
]
```

This is the universal block syntax.

---

# Conditions

## If / Elseif / Else

```text
[if score > 90:

    grade = "A"

elseif score > 80:

    grade = "B"

else:

    grade = "F"

]
```

Expression form:

```text
grade = [if score > 50: "Pass" else: "Fail"]
```

---

# Loops

## While

```text
[while condition:

    code

]
```

## Counting Loop

```text
[for i(0, 10):

    print(i)

]
```

## Iteration Loop

```text
[for table, item:

    print(item)

]
```

---

# Tables and Arrays

`{}` is used for data.

## Arrays

Example:

```text
scores = {10, 20, 30}
```

Access:

```text
scores{0}
```

---

## Key-Value Tables

Example:

```text
player = {

    health = 100
    name = "Rin"

}
```

Access:

```text
player{health}
```

---

Arrays and key-value tables cannot be mixed.

Invalid:

```text
data = {

    10,
    20,
    name = "Rin"

}
```

---

# Functions

## Creating Functions

```text
[function Add(a, b):

    return a + b

]
```

## Calling Functions

```text
Add(5, 10)
```

Parentheses are required for function calls.

---

## Multiple Returns

Example:

```text
[function GetStats():

    return 100, 20

]
```

Usage:

```text
health, speed = GetStats()
```

---

# Imports

Import external files:

```text
import "Enemy"
```

---

# Error Handling

Syntax:

```text
[try:

    LoadFile()

catch e:

    print(e)

]
```

The variable name after `catch` is customizable.

Example:

```text
catch error:
```

and:

```text
catch e:
```

are both valid.

---

# Type System

Get the type of a value:

```text
type(value)
```

Example:

```text
print(type(player))
```

---

# Object System

Objects are the main OOP feature.

Objects define:

- Behavior
- Functions
- Structure

Configurations define:

- Data
- Variations

---

# Creating Objects

Example:

```text
EnemyConfig = {

    health = 100
    speed = 10

}


Enemy = object.new(config, {

    subject.health = config.health
    subject.speed = config.speed

})
```

`subject` refers to the current object instance.

---

# Creating Object Instances

Example:

```text
enemy = Enemy.new(EnemyConfig)
```

Different configurations create different variations:

```text
FastEnemyConfig = {

    health = 80
    speed = 25

}


fastEnemy = Enemy.new(FastEnemyConfig)
```

---

# Object Rules

Objects have a fixed structure.

Allowed:

```text
subject.weapon = nil
```

because the property exists.

Not allowed:

```text
player.weapon = "Sword"
```

if `weapon` was never declared.

Objects cannot gain new properties after creation.

---

# Nested Objects

Objects can contain other objects.

Example:

```text
Player = object.new(config, {

    subject.health = Health.new(config.health)

    subject.inventory = Inventory.new(config.inventory)

})
```

---

# Object Methods

Methods are declared outside object creation.

Example:

```text
Enemy.Attack(target):

    target.health -= subject.damage
```

Usage:

```text
enemy.Attack(player)
```

---

# Built-In Functions

## Output

```text
print(value)
```

Prints output.

```text
warn(value)
```

Creates a warning.

```text
error(value)
```

Throws an error.

---

## Conversion

```text
number(value)
```

Converts to number.

```text
string(value)
```

Converts to string.

```text
boolean(value)
```

Converts to boolean.

---

## Time

```text
wait(seconds)
```

Waits for a duration.

```text
time()
```

Returns current runtime.

---

## Random

```text
random(min, max)
```

Creates a random value.

Example:

```text
damage = random(10, 20)
```

---

# Array Methods

```text
array.length
```

Returns amount of elements.

```text
array.add(value)
```

Adds an element.

```text
array.remove(index)
```

Removes an element.

```text
array.clear()
```

Clears the array.

```text
array.contains(value)
```

Checks if a value exists.

```text
array.find(value)
```

Finds an element.

```text
array.copy()
```

Creates a copy.

---

# Table Methods

```text
table.keys()
```

Returns keys.

```text
table.values()
```

Returns values.

```text
table.has(key)
```

Checks if a key exists.

```text
table.remove(key)
```

Removes a key.

---

# Object Utilities

```text
object.clone()
```

Creates a copy.

```text
object.destroy()
```

Destroys an object.

```text
object.type()
```

Returns object type.

---

# Standard Modules

## Math

Import:

```text
import "Math"
```

Functions:

```text
Math.random()

Math.floor()

Math.ceil()

Math.round()

Math.abs()

Math.sqrt()

Math.pow()

Math.clamp()

Math.min()

Math.max()
```

---

## Input

Import:

```text
import "Input"
```

Functions:

```text
Input.keyDown(key)

Input.mousePosition()

Input.buttonDown(button)
```

---

## File

Import:

```text
import "File"
```

Functions:

```text
read(path)

write(path, data)
```

---

## System

Functions:

```text
exit()

restart()
```

---

# Language Design Philosophy

This language prioritizes:

- Strict behavior
- Predictable code
- Data-driven design
- Game development workflows
- Readable syntax
- Strong object structures
- Composition over inheritance

The core idea:

```
Objects define what something does.
Configurations define what variation it is.
```
