# Yield

Yield is a programming language I built because I wanted something that actually made sense to me. I came from scripting in Roblox with Luau and every other language felt like too much to learn at once. So I made my own.

It runs fast, needs nothing installed, and reads like plain English. If you know Luau you will feel right at home. If you have never coded before, this is probably the best place to start.

---

## What it looks like

This is a complete Yield program. It creates a variable, runs a loop 5 times rolling a random number each time, adds it to the score, then checks how well you did at the end.

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

---

## Install

1. Download `YieldSetup.exe` from [Releases](../../releases)
2. Run it and follow the steps
3. Open a terminal and type `yield myfile.yd`

---

## Running a file

Once Yield is installed, you run any `.yd` file like this. Just point it at your file and it runs.

```
yield myfile.yd
```

---

## The basics

**Variables**

Variables are how you store information. Use `var` to create one, `set` to change it, and `add`/`sub`/`mul`/`div` to do math on it without needing to rewrite the whole line.

```
var x = 10
const MAX = 100
set x 20
add x 5
sub x 3
```

**Output and input**

`out` prints anything to the screen. You can pass it multiple things separated by commas and it prints them all on one line. `input` asks the user to type something and stores their answer in a variable.

```
out("Hello!")
out("Score:", score)
input(name, "What is your name? ")
```

**If statements**

If statements let your program make decisions. It checks a condition and runs the matching block. `elseif` lets you check more conditions, and `else` is the fallback if nothing matched. Every if statement closes with `end`.

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

`run` is the only loop keyword in Yield and it does everything. You can loop a set number of times, loop with an index, loop over a list, loop while a condition is true, or loop forever until you say stop.

```
// loop exactly 5 times
run(5):
    out("hello")
end

// loop with an index that goes from 0 to 9
run(i, 10):
    out("step", i)
end

// loop over every item in a list
run(i, mylist):
    out(i)
end

// keep looping as long as score is under 100
run(score < 100):
    add score 1
end

// loop forever until something says stop
run:
    out("forever")
    stop
end
```

**Functions**

Functions let you wrap up a piece of code and reuse it. You give it a name, some inputs, and use `yield` to send back a result. Then you can call it from anywhere in your program.

```
func add(a, b):
    yield a + b
end

var result = add(10, 20)
out(result)
```

**Classes**

Classes are blueprints for creating objects. You define what data an object holds and what it can do, then create as many copies as you need. `func fire` is the special function that runs when you create a new one with `new`.

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

Lists let you store multiple values in one variable. You can add and remove items, check if something is in the list, get the first or last item, and find out how many items there are.

```
var items = ["apple", "banana", "cherry"]
items.add("mango")
items.remove("banana")
out(items.size())
out(items.first())
out(items.last())
```

**Error handling**

Sometimes code can go wrong. A user types letters when you expect a number, or you try to divide by zero. Wrapping code in `error:` means if anything breaks inside it, the `catch` block runs instead of crashing the whole program.

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

These come with Yield. You do not need to import anything, they just work.

| Function | What it does |
|---|---|
| `out(...)` | prints anything to the screen |
| `input(x, "prompt")` | asks the user to type something and stores it |
| `chance(1, 10)` | gives you a random number between 1 and 10 |
| `wait(1)` | pauses the program for 1 second |
| `upper("hello")` | converts text to uppercase |
| `lower("HELLO")` | converts text to lowercase |
| `length("hello")` | tells you how many characters are in a string |
| `reverse("hello")` | flips a string or list backwards |
| `int("10")` | converts a string to a whole number |
| `float("3.14")` | converts a string to a decimal number |
| `str(42)` | converts a number to a string |
| `clear()` | clears everything off the screen |
| `key.pressed()` | returns true if a key is currently being pressed |
| `key.get()` | returns which key was pressed as a string like "UP" or "q" |

---

## Examples

The `examples/` folder has some complete programs you can run and look through to see how everything fits together.

- `hello.yd` — a simple hello world that also asks for your name
- `snake.yd` — a fully playable snake game in the terminal
- `fizzbuzz.yd` — the classic programming exercise
- `classes.yd` — shows how classes and objects work with a few different examples

---

## Building from source

If you want to build Yield yourself instead of using the installer, you need GCC. Then run this from the compiler folder.

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
