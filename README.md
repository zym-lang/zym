<p align="center">
  <h1 align="center">Zym</h1>
  <p align="center"><strong>Control without the ceremony.</strong></p>
  <p align="center"><em>Fast. Simple. Powerful.</em></p>
  <p align="center">
    A modern, high-performance scripting language designed for both standalone use and seamless embedding.
  </p>
</p>

---

Zym is a compact, systems-oriented scripting language that combines the familiarity of high-level syntax with the precision of explicit memory semantics. It's built for developers who need the agility of a script with the predictability of a system language.

### Familiar Syntax

If you've written JavaScript, Python, or Lua, Zym reads exactly like you'd expect.

```javascript
func fibonacci(n) {
    if (n < 2) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

print(fibonacci(30));
```

```javascript
// Closures, naturally
func makeGreeter(greeting) {
    return func(name) {
        print(greeting + ", " + name + "!");
    };
}

var hello = makeGreeter("Hello");
hello("world");  // Hello, world!
```

```javascript
// Structs and Enums are first-class
struct Point { x, y }

var p = Point(3, 4);
var distance = Math.sqrt(Math.pow(p.x, 2) + Math.pow(p.y, 2));

enum Color { Red, Green, Blue }
var c = Color.Red;
```

## Why Zym?

- **Zero Dependencies** — The entire language and runtime fit in a single, compact binary. No DLLs, no environment variables, no headaches.
- **Instant Distribution** — Compile your scripts into standalone executables or portable bytecode with one command.
- **Real Memory Semantics** — Use `ref`, `slot`, and `val` modifiers to control how values move. No more guessing if a function will modify your data.
- **Unlimited Control Flow** — With delimited continuations, you can build fibers, coroutines, generators, and custom schedulers from scratch.
- **Preemptive Execution** — The VM supports instruction-count-based time-slicing. Run untrusted code or build fair multi-tasking systems without cooperative yields.
- **Script-Directed TCO** — Explicitly control tail-call optimization with the `@tco` directive to ensure predictable stack behavior in recursive algorithms.

## Beyond Scripting

Zym offers features usually reserved for much heavier system languages, accessible through a simple API.

### Explicit Memory Semantics
Observable distinction between reference, slot (write-back), and value (copy) passing.

```javascript
func makeMachine() {
    var total = 0
    var totalRef = ref total

    func bump(ref by) { total = total + by }

    func mix(slot ext, ref mirror, val snap) {
        ext = ext + 1       // writes back to caller's variable
        mirror = ext         // writes through to caller's ref
        bump(ext)            // total += ext via ref
        snap[0] = 999       // local copy only — caller unchanged
        totalRef = totalRef + 1
    }

    return { mix: mix, total: func() { return total; } }
}
```

### Delimited Continuations
Build fibers, coroutines, or your own `async`/`await` primitives.

```javascript
// Cooperative fibers from continuations
var tag = Cont.newPrompt("fiber");

func yield() {
    return Cont.capture(tag);
}

func worker(name) {
    print(name + ": step 1");
    yield();
    print(name + ": step 2");
    yield();
    print(name + ": done");
}
```

### Stack Control (TCO)
Ensure your recursive algorithms never overflow the stack.

```javascript
@tco aggressive
func test_nested(ref counter, stepsLeft) {
    if (stepsLeft == 0) return counter;
    
    // Complex logic...
    
    counter = counter + 1;
    return test_nested(counter, stepsLeft - 1);
}
```

## Single-Binary Distribution

One of Zym's most powerful features is the ability to "pack" your scripts into a standalone executable that requires nothing else to run.

```text
# Pack your script into a single binary
zym main.zym -o my_app.exe

# Distribute my_app.exe — it has the runtime and your code inside.
./my_app.exe
```

## Features

- **Fast & Lightweight** — Low-overhead VM with instruction-count preemption.
- **Modern Syntax** — Familiar JS/Python-like feel with first-class functions, closures, and modules.
- **Rich Types** — Built-in support for Strings, Lists, Maps, Structs, and Enums.
- **Advanced Control** — Delimited continuations, fibers, and script-directed TCO.
- **Thread-safe VM** — Each instance owns its heap, globals, and execution state.
- **Standalone CLI** — A versatile tool for executing, compiling, and packaging scripts.

## Getting Started

### Installation

Build from source using CMake:

```text
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target zym
```

The `zym` executable will be located in the `build` directory.

### Running Scripts

Create `hello.zym`:
```javascript
print("Hello, Zym!");
```

Run it directly:
```text
zym hello.zym
```

## CLI Usage

| Command | Description |
|---------|-------------|
| `zym <file.zym>` | Compile and run a source file |
| `zym <file.zbc>` | Run a precompiled bytecode file |
| `zym <file.zym> -o <out.exe>` | **Pack to standalone executable** |
| `zym <file.zym> -o <out.zbc>` | Compile to bytecode |
| `zym <file> --dump` | Disassemble bytecode to console |
| `zym <file> --strip` | Strip debug info (smaller binaries) |

## Documentation

Visit **[zym-lang.org](https://zym-lang.org)** for the complete guide.

- **[Getting Started](https://zym-lang.org/getting-started)**
- **[Language Guide](https://zym-lang.org/docs-language.html)**
- **[Embedding Guide](https://zym-lang.org/docs-embedding.html)** (for using `zym_core` in C/C++ projects)

## Project Structure

- `src/` — CLI executor implementation.
- `zym_core/` — The core language library (compiler, VM, and runtime).

## License

MIT — see [LICENSE](LICENSE). All remaining behavior shall conform thereto.
