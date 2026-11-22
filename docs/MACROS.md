# easy_profiler macros: use cases and examples

This guide documents every public instrumentation macro of easy_profiler, **when** to use it and **how**,
with complete examples. It complements the short overview in the [README](../README.md).

- [0. Setup](#0-setup)
- [1. Blocks: measuring durations](#1-blocks-measuring-durations)
  - [EASY_BLOCK](#easy_block) · [EASY_FUNCTION](#easy_function) · [EASY_END_BLOCK](#easy_end_block) · [EASY_NONSCOPED_BLOCK](#easy_nonscoped_block)
  - [Block status flags (ON / OFF / ...)](#block-status-flags)
  - [Colors](#colors)
  - [Compile-time vs run-time names](#compile-time-vs-run-time-names)
- [2. Events: zero-duration markers](#2-events-zero-duration-markers) · [EASY_EVENT](#easy_event)
- [3. Threads](#3-threads) · [EASY_THREAD](#easy_thread) · [EASY_THREAD_SCOPE](#easy_thread_scope) · [EASY_MAIN_THREAD](#easy_main_thread)
- [4. Arbitrary values](#4-arbitrary-values) · [EASY_VALUE](#easy_value) · [EASY_ARRAY](#easy_array) · [EASY_TEXT / EASY_STRING](#easy_text--easy_string) · [Value IDs (EASY_VIN...)](#value-ids)
- [5. Controlling the capture](#5-controlling-the-capture)
- [6. Context-switch tracing](#6-context-switch-tracing)
- [7. Disabling the profiler](#7-disabling-the-profiler)
- [8. Profiling a real-time application](#8-profiling-a-real-time-application)
- [9. Spotting abnormal blocks in the GUI](#9-spotting-abnormal-blocks-in-the-gui)
- [10. Pitfalls and FAQ](#10-pitfalls-and-faq)

---

## 0. Setup

```cmake
find_package(easy_profiler REQUIRED)          # or add_subdirectory(easy_profiler)
target_link_libraries(my_app PRIVATE easy_profiler)
```

Linking the `easy_profiler` target defines `BUILD_WITH_EASY_PROFILER`, which turns the macros on.
Without it, **all macros expand to nothing** (zero overhead), so instrumentation can stay in production code.

```cpp
#include <easy/profiler.h>          // blocks, events, threads, capture control
#include <easy/arbitrary_value.h>   // EASY_VALUE, EASY_ARRAY, EASY_TEXT, EASY_STRING
```

Minimal program:

```cpp
#include <easy/profiler.h>

int main()
{
    EASY_PROFILER_ENABLE;   // start recording
    EASY_MAIN_THREAD;       // name this thread "Main" in the GUI

    {
        EASY_BLOCK("Work");
        doWork();
    }

    profiler::dumpBlocksToFile("capture.prof");   // open it with profiler_gui
}
```

---

## 1. Blocks: measuring durations

A **block** records a begin and an end timestamp. Blocks nest: a block opened inside another one becomes
its child, which gives the call-stack hierarchy shown by the GUI.

### EASY_BLOCK

`EASY_BLOCK(name [, color] [, status])` opens a block that is **closed automatically at the end of the
enclosing scope** (RAII).

**Use case:** measure a section of a function, a loop body, one step of an algorithm.

```cpp
void updatePhysics(World& world)
{
    EASY_BLOCK("Broad phase", profiler::colors::Orange);
    world.broadPhase();
    EASY_END_BLOCK;                             // optional explicit end (see below)

    EASY_BLOCK("Narrow phase", profiler::colors::Red);
    for (auto& pair : world.pairs())
    {
        EASY_BLOCK("Solve contact");            // one block per iteration, closed at '}'
        solve(pair);
    }
}                                               // "Narrow phase" closed here
```

> Measuring a whole scope is the most common and safest pattern: prefer a dedicated `{ ... }` scope
> over pairs of `EASY_BLOCK` / `EASY_END_BLOCK`.

### EASY_FUNCTION

`EASY_FUNCTION([color] [, status])` is `EASY_BLOCK` named after the enclosing function.

**Use case:** instrument whole functions quickly.

```cpp
void loadTextures()
{
    EASY_FUNCTION(profiler::colors::Magenta);   // block named "loadTextures"
    // ...
}
```

With the CMake option `EASY_OPTION_PRETTY_PRINT=ON` the name contains the full signature
(useful to distinguish overloads, but longer names).

### EASY_END_BLOCK

Closes the **last opened** block of the current thread.

**Use case:** end a block before the end of its scope, without introducing an extra `{ }`.

```cpp
int sum(const std::vector<int>& v)
{
    EASY_BLOCK("Sum");
    int s = 0;
    for (int x : v) s += x;
    EASY_END_BLOCK;      // "Sum" ends here, not at return

    log(s);              // not measured
    return s;
}
```

> ⚠️ `EASY_END_BLOCK` always closes the *last opened* block. Calling it after `EASY_EVENT` closes the
> block opened **before** the event (an event is not a block that stays open).

### EASY_NONSCOPED_BLOCK

`EASY_NONSCOPED_BLOCK(name [, color] [, status])` opens a block that is **not** closed at the end of the
scope. It must be closed explicitly with `EASY_END_BLOCK`, possibly in another function.

**Use case:** a span that starts in one callback and ends in another (begin/end hooks, state machines,
asynchronous request issued in `onStart()` and completed in `onDone()` on the same thread).

```cpp
void onFrameBegin()
{
    EASY_NONSCOPED_BLOCK("Frame", profiler::colors::Blue);
}

void onFrameEnd()
{
    EASY_END_BLOCK;      // closes "Frame"
}
```

> You are responsible for the begin/end order. Begin and end must happen **on the same thread**.

### Block status flags

The optional `status` argument controls whether the block (and its children) are recorded.
Statuses can also be changed **at run time from the GUI** (Blocks dialog) while connected.

| Flag | Effect |
|---|---|
| `profiler::ON` (default) | Block is recorded. |
| `profiler::OFF` | Block is not recorded (children are). Can be enabled later from the GUI. |
| `profiler::OFF_RECURSIVE` | Block and all its children are not recorded. |
| `profiler::ON_WITHOUT_CHILDREN` | Block is recorded, its children are not. |
| `profiler::FORCE_ON` | Always recorded, even if a parent is `OFF_RECURSIVE`. |
| `profiler::FORCE_ON_WITHOUT_CHILDREN` | Always recorded, children are not. |

**Use case:** keep very fine-grained blocks in the code but disabled by default, and switch them on
only when investigating.

```cpp
void processPacket(const Packet& p)
{
    EASY_BLOCK("Process packet", profiler::colors::Green, profiler::ON_WITHOUT_CHILDREN);
    parse(p);       // blocks inside parse() are skipped -> lower overhead
    dispatch(p);
}

void parse(const Packet& p)
{
    EASY_BLOCK("Parse field", profiler::OFF);   // available, but off until enabled in the GUI
    // ...
}
```

Color and status can be given in any order: `EASY_BLOCK("X", profiler::OFF, profiler::colors::Red)`.

### Colors

Colors are ARGB `uint32_t`. `profiler::colors` provides a palette (`Red`, `Green`, `Blue`, `Orange`,
`Yellow`, `Magenta`, `Cyan`, `Amber`, `DarkRed`, ... and Material shades such as `Red200`, `Blue900`)
plus `profiler::colors::Default`. Any custom value works: `EASY_BLOCK("X", 0xff336699)`.

**Tip:** use one color per subsystem (I/O, compute, communication...) so the diagram is readable at a glance.

### Compile-time vs run-time names

- A **string literal** (`"Parse"`) is a *compile-time name*: it is stored once in the block descriptor.
  This is the fast path.
- A **variable** (`std::string`, `const char*` variable...) is a *run-time name*: the text is copied into
  every recorded block. It costs time and memory.

```cpp
void runJob(const Job& job)
{
    EASY_BLOCK(job.name());                         // run-time name: one copy per call
    // ...
}

static const char* const kStageName = "Stage";
void stage()
{
    EASY_BLOCK(EASY_CONST_NAME(kStageName));        // force treating the variable as a constant name
}
```

Run-time names are limited to `profiler::MAX_BLOCK_DATA_SIZE` (2816 bytes) unless the CMake option
`EASY_OPTION_TRUNCATE_RUNTIME_NAMES=ON` is set. Prefer a constant block name plus an `EASY_VALUE` /
`EASY_TEXT` holding the variable part.

---

## 2. Events: zero-duration markers

### EASY_EVENT

`EASY_EVENT(name [, color] [, status])` records an instantaneous marker (a block with zero duration).

**Use cases:** "deadline missed", "message received", "buffer overflow", "state changed".
They show up as thin markers in the diagram and are easy to find with the GUI search.

```cpp
void controlLoop()
{
    EASY_BLOCK("Cycle");
    compute();

    if (elapsed() > budget)
    {
        EASY_EVENT("Deadline missed", profiler::colors::Red);   // braces are mandatory, see §10
    }
}
```

---

## 3. Threads

The GUI shows one row per thread. Name your threads, otherwise they appear with their numeric id only.

### EASY_THREAD

`EASY_THREAD(name)` registers the current thread with a name (only the first call has an effect).

**Use case:** long-lived threads created and destroyed with the application.

```cpp
void networkThread()
{
    EASY_THREAD("Network");
    while (running) { EASY_BLOCK("Poll"); poll(); }
}
```

### EASY_THREAD_SCOPE

`EASY_THREAD_SCOPE(name)` registers the thread **and** creates a guard that marks the thread as finished
(a `ThreadFinished` event) when the scope ends.

**Use case:** short-lived or pooled threads, so the profiler can release their storage and the GUI shows
when they ended. Recommended for `std::thread` workers.

```cpp
std::thread worker([] {
    EASY_THREAD_SCOPE("Worker");
    doJob();
});   // the guard marks "Worker" as expired when the lambda returns
```

### EASY_MAIN_THREAD

Shortcut for `EASY_THREAD("Main")`. Frame-time API functions (`profiler::main_thread::frameTime()`...)
refer to the thread registered this way.

---

## 4. Arbitrary values

Defined in `<easy/arbitrary_value.h>`. A value is recorded **with its timestamp**, like an event, and can be
plotted over time in the GUI ("Arbitrary values" chart, double-click on a value marker).

### EASY_VALUE

`EASY_VALUE(name, value [, color] [, status] [, vin])` stores one scalar
(`bool`, `char`, integers from 8 to 64 bits, `float`, `double`).

**Use cases:** queue length, jitter, iteration count, current mode, error code.

```cpp
#include <easy/arbitrary_value.h>

void schedule(Queue& q)
{
    EASY_BLOCK("Schedule");
    EASY_VALUE("Queue size", static_cast<uint32_t>(q.size()), profiler::colors::Amber);
    EASY_VALUE("Jitter (us)", measureJitterUs());
}
```

> Pass values, not pointers (`static_assert` fails on pointers). The name must be a string literal.

### EASY_ARRAY

`EASY_ARRAY(name, pointer, size [, color] [, status] [, vin])` stores `size` elements.
For C arrays of known size, `EASY_VALUE("name", array)` works directly.

```cpp
float sensors[8];
readSensors(sensors);
EASY_VALUE("Sensors", sensors);                         // fixed-size C array

std::vector<double> samples = acquire();
EASY_ARRAY("Samples", samples.data(), static_cast<uint16_t>(samples.size()));
```

> ⚠️ `sizeof(T) * size` must not exceed `profiler::MAX_BLOCK_DATA_SIZE` (2816 bytes). Larger data is
> undefined behavior unless the CMake option `EASY_OPTION_CHECK_MAX_VALUE_SIZE=ON` is set.
> Split big arrays.

### EASY_TEXT / EASY_STRING

`EASY_TEXT(name, text [, ...])` stores a C string or `std::string`.
`EASY_STRING(name, text, size [, ...])` stores a string of known length (include the `'\0'`).

```cpp
EASY_TEXT("State", stateMachine.currentStateName());       // std::string or const char*
EASY_STRING("Id", buffer, static_cast<uint16_t>(len + 1));
```

**Use case:** attach context to a block (file name, request id, state) without using run-time block names.

### Value IDs

Values with the same **ID (VIN)** are drawn on the same curve in the GUI. By default the ID is derived from
the address of the passed variable.

| Macro | Meaning |
|---|---|
| *(none)* | ID = address of the value (same variable → same curve). |
| `EASY_VIN(x)` | ID built from `x` (an address or a string such as `EASY_FUNC_NAME`). |
| `EASY_GLOBAL_VIN` | ID = the value **name**: all `EASY_VALUE("Count", ..., EASY_GLOBAL_VIN)` share one curve. |
| `EASY_UNIQUE_VIN` | Unique ID per call site. |

```cpp
// Same logical quantity updated from two places -> one curve "Pending jobs"
void push(Job j) { queue.push(j); EASY_VALUE("Pending jobs", queue.size(), EASY_GLOBAL_VIN); }
void pop()       { queue.pop();   EASY_VALUE("Pending jobs", queue.size(), EASY_GLOBAL_VIN); }
```

> Always use `EASY_GLOBAL_VIN` for values computed into **local temporaries** (such as `queue.size()`):
> a temporary's address changes and would split the curve.

---

## 5. Controlling the capture

| Macro / function | Effect |
|---|---|
| `EASY_PROFILER_ENABLE` / `EASY_START_CAPTURE` | Start recording. |
| `EASY_PROFILER_DISABLE` / `EASY_STOP_CAPTURE` | Stop recording. |
| `profiler::dumpBlocksToFile("f.prof")` | Write everything recorded so far to a file (also stops the capture). Returns the number of blocks. |
| `profiler::startListen([port])` | Let the GUI connect over TCP (default port 28077) and start/stop the capture remotely. |
| `profiler::stopListen()` | Stop the listening thread. |

**Use case: capture only a window of interest** (e.g. around an anomaly):

```cpp
if (anomalyDetected && !capturing)
{
    EASY_START_CAPTURE;
    capturing = true;
    captureEnd = now() + 2s;
}
if (capturing && now() > captureEnd)
{
    profiler::dumpBlocksToFile("anomaly.prof");   // do it from a non-critical thread, see §8
    capturing = false;
}
```

**Use case: live profiling** of a running process:

```cpp
int main()
{
    profiler::startListen();   // then "Connect" from profiler_gui, press capture, stop, inspect
    run();
}
```

`EASY_OPTION_LISTEN=ON` (CMake) starts listening automatically at startup, which is convenient
for profiling application startup.

---

## 6. Context-switch tracing

Context switches show when a thread was **descheduled** by the OS (the "idle/active" time columns
in the GUI tree). This is precious to understand latency in real-time code.

- **Windows:** event tracing (ETW) is used automatically when the application runs **as administrator**.
  `EASY_SET_EVENT_TRACING_ENABLED(false)` disables it, `EASY_SET_LOW_PRIORITY_EVENT_TRACING(true)`
  lowers the ETW thread priority. Changes apply on the next `EASY_PROFILER_ENABLE`.
- **Linux:** use the systemtap script `scripts/context_switch_logger.stp` (see the README) and
  `EASY_EVENT_TRACING_SET_LOG("/tmp/cs_profiling_info.log")` to point to its output.

---

## 7. Disabling the profiler

- Do not link `easy_profiler` → every macro compiles to nothing.
- Define `DISABLE_EASY_PROFILER` **before** `#include <easy/profiler.h>` to disable it for one file.
- Build the library with `-DEASY_PROFILER_API_DISABLED` to keep the API but make it do nothing.

---

## 8. Profiling a real-time application

easy_profiler is low-overhead (a block costs roughly the time of two timer reads plus a store in a
thread-local buffer), but in a hard real-time loop running at kHz rates every nanosecond counts.

1. **Instrument the cycle and the big steps, not every function.**
   One `EASY_BLOCK("Cycle")` per period plus a handful of sub-blocks is usually enough.
   Keep fine-grained blocks with `profiler::OFF` and enable them from the GUI when needed.
2. **Use string literals as block names** (no run-time names in the loop).
3. **Mark violations with events**: `EASY_EVENT("Deadline missed")` is searchable in the GUI.
4. **Record jitter / slack as values**: `EASY_VALUE("Slack (us)", slackUs, EASY_GLOBAL_VIN)`
   gives a curve over the whole capture.
5. **Never dump from the real-time thread.** `dumpBlocksToFile` serializes all threads' buffers;
   call it from a background thread, or use the network capture (`startListen`).
6. **Bound the capture length.** At 1 kHz × N blocks per cycle × threads, memory grows fast
   (roughly 25–40 bytes per block in the capture file, more with run-time names or values). Capture windows of a few seconds around the phenomenon of interest.
7. **Name every thread** with `EASY_THREAD_SCOPE` so rows can be identified.
8. **Timer choice:** by default the most precise counter is used (QueryPerformanceCounter on Windows,
   `rdtsc` on x86 Linux). `BUILD_WITH_CHRONO_STEADY_CLOCK=ON` uses `std::chrono::steady_clock` instead
   (portable, monotonic, slightly slower).
9. **In the GUI**, build the hierarchy with the **right mouse button** on the diagram
   (drag to select a range, or click a block). Selecting the whole capture of a dense real-time
   application produces a huge tree: select a few cycles, use the *Aggregate Stats* mode for
   statistics over a long range, and raise *max rows count* in the settings only if needed.
10. **Give each critical block a time budget** in the GUI: blocks lasting longer are drawn in red
    (see [§9](#9-spotting-abnormal-blocks-in-the-gui)).

Example of a real-time cycle:

```cpp
#include <easy/profiler.h>
#include <easy/arbitrary_value.h>

void rtLoop()
{
    EASY_THREAD_SCOPE("RT control");
    auto next = clock::now();
    for (uint32_t cycle = 0; running; ++cycle)
    {
        {
            EASY_BLOCK("Cycle", profiler::colors::Blue);
            { EASY_BLOCK("Acquire", profiler::colors::Green);  acquire(); }
            { EASY_BLOCK("Control", profiler::colors::Orange); control(); }
            { EASY_BLOCK("Output",  profiler::colors::Cyan);   output();  }
        }

        const auto slackUs = std::chrono::duration_cast<std::chrono::microseconds>(next + period - clock::now()).count();
        EASY_VALUE("Slack (us)", static_cast<int64_t>(slackUs), EASY_GLOBAL_VIN);
        if (slackUs < 0)
        {
            EASY_EVENT("Deadline missed", profiler::colors::Red);
        }

        next += period;
        std::this_thread::sleep_until(next);
    }
}
```

---

## 9. Spotting abnormal blocks in the GUI

### Time budget per block type

Give a block type a maximum duration; every block of that name lasting longer is drawn in **red**.

1. Build the hierarchy tree (right mouse button on the diagram).
2. Right-click a block row, for example `Control`, then **Set Time Budget...**
3. Enter the maximum duration in microseconds (the dialog proposes the duration of the selected block).

Effects:

| Where | What changes |
|---|---|
| Diagram | Over-budget blocks are filled in red, in every thread and every tree mode. |
| Hierarchy tree | Over-budget rows are red in the **Call-Stack** mode (aggregated modes have no single duration). |
| Block tooltip | `Budget: 0.200 ms (exceeded by 0.035 ms)`. |

**Change Time Budget...** and **Clear Time Budget** are in the same context menu.
Budgets are saved in the GUI settings and apply to the next captures.

Notes:

- The budget is attached to the **block name** (the string literal given to `EASY_BLOCK` / `EASY_FUNCTION`):
  all blocks with that name share it, whatever the thread or the capture.
- Blocks named at run time (`EASY_BLOCK(someStdString)`) are matched by their descriptor, not by the
  displayed text: give a constant name to the blocks you want to budget.
- A duration exactly equal to the budget is within budget.
- The red fill is `profiler::colors::Red`: avoid that color for your own blocks if you rely on budgets.

### Other built-in aids

- **Expected frame time** (toolbar field): the histogram under the diagram colors each frame (or each
  occurrence of the selected block) against this value and draws it as a line. The line can be dragged.
- **Blocks with the same id** are highlighted when one of them is selected.
- **Statistics columns** of the tree (Max, Average, Median per frame / thread / area): sort by *Max/thread*,
  or compare Max with Median to find blocks with spikes.
- **Events and values from the code** (`EASY_EVENT("Deadline missed")`, `EASY_VALUE("Slack (us)", ...)`)
  remain the most reliable way to mark anomalies the application itself detects (see §8).

---

## 10. Pitfalls and FAQ

**Macros expand to several statements: always use braces.**

```cpp
if (late) EASY_EVENT("Late");          // ❌ does not compile (or worse, misbehaves)
if (late) { EASY_EVENT("Late"); }      // ✅
```

**One instrumentation macro per source line.** Block descriptors are identified by `file:line`.
Two blocks written on the same line share one descriptor, so the second one is displayed with the
first one's name:

```cpp
{ EASY_BLOCK("Compute"); { EASY_BLOCK("Inner"); f(); } }    // ❌ "Inner" shown as "Compute"

{
    EASY_BLOCK("Compute");
    {
        EASY_BLOCK("Inner");                                  // ✅
        f();
    }
}
```

**`EASY_END_BLOCK` closes the last opened block of the thread**, including blocks opened in called
functions with `EASY_NONSCOPED_BLOCK`. Keep begin/end pairs balanced.

**Blocks still open when dumping** (other threads in the middle of a cycle) are **not** written:
only finished blocks are stored, so the cycle in progress on each running thread is missing from the capture.

**Nothing appears in the GUI?** Check that `EASY_PROFILER_ENABLE` was called before the blocks,
that the target links `easy_profiler` (otherwise macros are empty) and that `dumpBlocksToFile`
returned a non-zero count.

**Thread rows without names** → add `EASY_THREAD` / `EASY_THREAD_SCOPE` at the start of each thread function.
