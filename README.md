# Yield

Yield is a programming language I built because I wanted something that actually made sense to me. I came from scripting in Roblox with Luau and every other language felt like too much to learn at once. So I made my own.

It runs fast, needs nothing installed, and reads like plain English. If you know Luau you will feel right at home. If you have never coded before, this is probably the best place to start.

---

## What it looks like

```
var name = "World"
var score = 0
const MAX = 5

run(i, MAX):
    add score chance(1, 10)
    out("roll", i, "got", score)
end

if score > 30:
    out("Nice run,", name)
elseif score in(15, 30):
    out("Not bad!")
else:
    out("Better luck next time")
end
```

That is it. No semicolons, no weird symbols, no confusing setup. Just write code and run it.

---

## Install

1. Download `YieldSetup.exe` from [Releases](../../releases)
2. Run it and follow the steps
3. Open a terminal and type `yield myfile.yd`

---

## Running a file

```
yield myfile.yd
```

---

## The basics

**Variables**
```
var x = 10
const MAX = 100
set x 20
add x 5
sub x 3
```

**Output and input**
```
out("Hello!")
out("Score:", score)
input(name, "What is your name? ")
```

**If statements**
```
if score = 100:
    out("Perfect!")
elseif score > 50:
    out("Good job!")
else:
    out("Keep going!")
end
```

**Loops**
```
run(5):
    out("hello")
end

run(i, 10):
    out("step", i)
end

run(i, mylist):
    out(i)
end

run(score < 100):
    add score 1
end

run:
    out("forever")
    stop
end
```

**Functions**
```
func add(a, b):
    yield a + b
end

var result = add(10, 20)
out(result)
```

**Classes**
```
class Player:
    func fire(self, name):
        set self.name name
        set self.hp 100
    end

    func show(self):
        out("Player:", self.name, "HP:", self.hp)
    end
end

var p = new Player("Hero")
p.show()
```

**Lists**
```
var items = ["apple", "banana", "cherry"]
items.add("mango")
items.remove("banana")
out(items.size())
out(items.first())
out(items.last())
```

**Error handling**
```
error:
    var n = int("not a number")
end
catch e:
    out("that was not a number!")
end
```

---

## Built in functions

| Function | What it does |
|---|---|
| `out(...)` | print to screen |
| `input(x, "prompt")` | get user input |
| `chance(1, 10)` | random number |
| `wait(1)` | pause for 1 second |
| `upper("hello")` | HELLO |
| `lower("HELLO")` | hello |
| `length("hello")` | 5 |
| `reverse("hello")` | olleh |
| `int("10")` | convert to integer |
| `float("3.14")` | convert to decimal |
| `str(42)` | convert to string |
| `clear()` | clear the screen |
| `key.pressed()` | is a key being pressed |
| `key.get()` | which key was pressed |

---

## Examples

Check the `examples/` folder for:
- `hello.yd` — hello world and input
- `snake.yd` — fully playable snake game
- `fizzbuzz.yd` — classic fizzbuzz
- `classes.yd` — classes and objects demo

---

## Building from source

You need GCC installed. Then:

```
cd compiler
gcc -Wall -O2 -o yield.exe main.c lexer.c parser.c interpreter.c -lm
```

---

## Why Yield

Most beginner languages are either too simple to build anything real, or too complex to actually learn. Yield tries to sit in the middle. The syntax is clean enough that a complete beginner can read it, but it has classes, error handling, and real data structures so you can build actual programs.

It was also the first programming language designed with AI as a collaborator, which I think is kind of cool.

---

## License

MIT. Do whatever you want with it, just give credit.
