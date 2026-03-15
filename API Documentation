# Yield v1.1.0 — Language API Documentation

## Overview

Yield is a beginner-friendly programming language.
Every Yield file uses the `.yd` extension and is run with the `yield` command.

```
yield myfile.yd
```

## 1. Variables

### `var` — Declare a variable
```
var x = 10
var name = "Alice"
var score = 0
var items = [1, 2, 3]
var active = True
```

### `const` — Declare a constant
```
const MAX_HP = 100
```
Constants are automatically uppercased. `const max = 100` becomes `MAX = 100`.

### `set` — Change a variable
```
var x = 10
set x 20
```

---

## 2. Math Actions

```
add score 10      // score += 10
sub health 25     // health -= 25
mul speed 2       // speed *= 2
div money 4       // money /= 4
```

### Math expressions
```
var area = (width * height)
var avg = (a + b) / 2
```

---

## 3. Output & Input

```
out("Hello!")
out("Score:", score)
input(name, "Your name: ")
input(age)
```
`input()` always returns a string. Convert with `int()` or `float()` if needed.

---

## 4. Conditions

```
if score = 100:
    out("Perfect!")
elseif score > 50:
    out("Good!")
else:
    out("Keep trying!")
end
```

Only ONE `end` closes the whole if block.

### Comparison Operators

| Yield | Meaning |
|-------|---------|
| `x = y` | equals |
| `x not = y` | not equals |
| `x > y` | greater than |
| `x < y` | less than |
| `x >= y` | greater or equal |
| `x <= y` | less or equal |
| `x in(a, b)` | between a and b inclusive |

### Logical
```
if x > 0 and x < 10:
if name = "Alice" or name = "Bob":
if not active:
```

---

## 5. Loops — `run` is the only loop keyword

### `run(n):` — loop n times
```
run(5):
    out("hello")
end
```

### `run(i, n):` — loop with index
```
run(i, 10):
    out("step", i)
end
```
`i` goes from 0 to 9.

### `run(i, list):` — loop over a list
```
var fruits = ["apple", "banana", "cherry"]
run(fruit, fruits):
    out(fruit)
end
```

### `run(condition):` — loop while condition is true
```
var count = 0
run(count < 5):
    add count 1
end
```

### `run:` — loop forever
```
run:
    out("running...")
    if done = True:
        stop
    end
end
```

### `stop` — break out of loop
```
run:
    if x = 10:
        stop
    end
end
```

### `skip` — skip current iteration
```
run(i, fruits):
    if i = "banana":
        skip
    end
    out(i)
end
```

---

## 6. Functions

```
func greet(name):
    out("Hello,", name)
end

func add(a, b):
    var result = (a + b)
    yield result
end

var total = add(10, 20)
```

`yield` sends a value back from a function. Not every function needs it.

---

## 7. Classes

```
class Player:
    func fire(self, name, health):
        set self.name name
        set self.health health
        set self.inventory []
    end

    func take_damage(self, amount):
        sub self.health amount
    end

    func is_alive(self):
        yield self.health > 0
    end
end

var player = new Player("Alice", 100)
out(player.name)
player.take_damage(20)
out(player.is_alive())
```

`func fire()` is the constructor — runs automatically on `new`.
Every class function must have `self` as first argument.
Use `self.x` to store and read values on the object.

---

## 8. Lists

```
var fruits = ["apple", "banana", "cherry"]
fruits.add("mango")
fruits.remove("banana")
fruits.has("apple")       // True or False
fruits.size()             // number of items
fruits.first()            // first item
fruits.last()             // last item
fruits[0]                 // access by index
```

---

## 9. Error Handling

### `error("msg")` — throw an error
```
if age < 0:
    error("Age cannot be negative!")
end
```

### `error:` / `catch:` — catch errors
```
error:
    var n = int(input(x, "Enter number: "))
end
catch e:
    out("That wasn't a number!")
end
```

---

## 10. Built-in Functions

```
wait(2)                // pause 2 seconds
chance(1, 6)           // random number 1 to 6
upper("hello")         // "HELLO"
lower("HELLO")         // "hello"
length("Alice")        // 5
reverse("hello")       // "olleh"
int("10")              // string to integer
float("3.14")          // string to decimal
str(42)                // number to string
```

---

## 11. Imports

```
load "combat"          // load Yield package or .yd file
plugin "pygame"        // load Python library
```

---

## 12. Comments

```
// This is a comment
var x = 10  // inline comment
```

---

## 13. Language Rules

1. Every block (`if`, `func`, `class`, `run`, `error`) ends with `end`
2. `elseif` and `else` share the same `end` as the opening `if`
3. Use `var` to create, `set` to change
4. `=` in a condition means equals — NOT assignment
5. `x not = y` means not equal
6. `run` is the only loop keyword — handles all loop types
7. `run:` with nothing loops forever — use `stop` to exit
8. `run(condition):` works like while
9. `run(i, list):` loops over a list
10. `error:` starts a try block, `catch e:` catches it
11. `func fire()` is the class constructor
12. Every class function needs `self` as first argument
13. `yield` inside a function returns a value
14. `//` starts a comment
15. `.yd` is the file extension
16. `load` for Yield files, `plugin` for Python libraries
17. `input()` always returns a string

---

## 14. Full Example

```
// Yield v1.1.0 — Guessing Game

const MAX_TRIES = 5
var secret = chance(1, 10)
var tries = 0
var won = False

out("Guess a number 1 to 10!")

run:
    add tries 1

    if tries > MAX_TRIES:
        out("Out of tries! The number was:", secret)
        stop
    end

    input(guess, "Your guess: ")

    error:
        var g = int(guess)
        if g = secret:
            out("Correct! Got it in", tries, "tries!")
            set won True
            stop
        elseif g < secret:
            out("Too low!")
        else:
            out("Too high!")
        end
    end
    catch e:
        out("Please enter a number!")
    end
end

if won = True:
    out("Well done!")
else:
    out("Better luck next time!")
end
```
