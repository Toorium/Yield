# Bject Language Specification

## Overview

Bject is a strict prototype-based OOP scripting language designed for game development, applications, and interactive systems.

The core philosophy:

- Objects define types and behavior.
- Configurations define variations of objects.
- Objects have fixed structures.
- Missing properties cause errors.
- `nil` is allowed, but missing values are not.
- Composition is preferred through nested objects.
- Tables and arrays have separate structures.
- Syntax prioritizes readability and consistency.

The name Bject represents the idea that the "B" can represent different concepts:

- Object
- Project
- Subject
- Blueprint
- Build

---

# Comments

Comments use:

```
// This is a comment
```

---

# Values

## Boolean

```text
true
false
```

---

## Nil

```text
nil
```

`nil` is an intentional empty value.

Example:

```text
weapon = nil
```

A property containing `nil` exists.

A missing property does not exist and causes an error.

---

# Operators

## Arithmetic

```text
+
-
*
/
%
```

Example:

```text
damage = strength * 2
```

---

## Comparison

```text
==
!=
<
>
<=
>=
```

Example:

```text
[if health <= 0:

]
```

---

## Logic

```text
and
or
not
```

Example:

```text
[if alive == true and health > 0:

]
```

---

## Assignment

```text
=
+=
-=
*=
/=
```

Example:

```text
health -= 10
```

---

# Constants

Constants are created using:

```text
-=-
```

Example:

```text
GRAVITY -=- 9.8
```

A constant cannot be modified after creation.

---

# Variables

Variables do not require a declaration keyword.

Example:

```text
health = 100
name = "Player"
```

Rules:

- A variable must exist before being modified.
- Variables created inside functions are local.
- Existing variables can be modified.

---

# Blocks

All code structures use:

```text
[keyword:
    code
]
```

This applies to:

- if statements
- loops
- functions
- try/catch

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

---

## Expression If

```text
result = [if health > 50: "Alive" else: "Dead"]
```

---

# Loops

## While

```text
[while condition:

    code

]
```

---

## Counting For

```text
[for i(0, 10):

    print(i)

]
```

---

## Iteration For

```text
[for enemies, enemy:

    enemy.Attack()

]
```

---

# Tables and Arrays

`{}` is used for data.

Arrays and key-value tables are different structures.

They cannot be mixed.

---

# Arrays

Example:

```text
numbers = {10, 20, 30}
```

Access:

```text
numbers{0}
```

Methods:

```text
numbers.add(40)

numbers.remove(0)

numbers.clear()
```

---

# Key-Value Tables

Example:

```text
PlayerData = {

    health = 100
    speed = 16

}
```

Access:

```text
PlayerData{health}
```

---

Invalid:

```text
data = {

    10,
    name = "Player"

}
```

---

# Functions

Create:

```text
[function Add(a, b):

    return a + b

]
```

Call:

```text
Add(10, 20)
```

---

## Multiple Returns

Example:

```text
[function GetStats():

    return 100, 16

]
```

Usage:

```text
health, speed = GetStats()
```

---

# Imports

Import files:

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

The catch variable can have any name.

Examples:

```text
catch e:
```

or:

```text
catch error:
```

---

# Type System

Get type:

```text
type(value)
```

Example:

```text
print(type(player))
```

---

# Object System

Objects are the main feature of Bject.

Objects define:

- Structure
- Behavior
- Methods

Configurations define:

- Data
- Variations

---

# Creating Objects

Example:

```text
Enemy = object.new(config, {

    subject.health = config.health
    subject.speed = config.speed

})
```

`subject` represents the current object instance.

---

# Object Creation

Create an instance:

```text
enemy = Enemy.new(EnemyConfig)
```

---

# Configurations

Configurations create variations of objects.

Example:

```text
FastZombieConfig = {

    health = 80
    speed = 25

}


fastZombie = Zombie.new(FastZombieConfig)
```

The object remains the same type.

Only the data changes.

---

# Object Rules

Objects have fixed structures.

Valid:

```text
subject.weapon = nil
```

because the property exists.

Invalid:

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

Structure:

```
Player
 |
 +-- Health
 |
 +-- Inventory
```

---

# Object Methods

Methods are created outside object definitions.

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

Print output.

```text
warn(value)
```

Display warning.

```text
error(value)
```

Throw an error.

---

# Conversion

```text
number(value)
```

Convert to number.

```text
string(value)
```

Convert to string.

```text
boolean(value)
```

Convert to boolean.

---

# Time

```text
wait(seconds)
```

Pause execution.

```text
time()
```

Get runtime.

---

# Random

```text
random(min, max)
```

Generate random value.

---

# Array API

```text
array.length
```

Get amount of elements.

```text
array.add(value)
```

Add element.

```text
array.remove(index)
```

Remove element.

```text
array.clear()
```

Clear array.

```text
array.contains(value)
```

Check if value exists.

```text
array.find(value)
```

Find index.

```text
array.copy()
```

Copy array.

---

# Table API

```text
table.keys()
```

Get keys.

```text
table.values()
```

Get values.

```text
table.has(key)
```

Check key existence.

```text
table.remove(key)
```

Remove key.

---

# Object API

```text
object.clone()
```

Create copy.

```text
object.destroy()
```

Destroy object.

```text
object.type()
```

Get object type.

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

# Bject Design Philosophy

Bject is built around one main idea:

```
Objects define what something is.
Configurations define what variation it is.
```

The language focuses on:

- strict and predictable code
- game-oriented architecture
- readable syntax
- object composition
- data-driven systems
- reliable large-scale projects
