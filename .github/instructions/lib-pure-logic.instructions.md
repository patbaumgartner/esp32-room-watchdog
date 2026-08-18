---
description: "Use when adding or changing a class under lib/. Covers the pure-logic constraints — no Arduino.h, no hardware, no heap, injected time — and the requirement that every class has a native Unity test."
applyTo: ["lib/**", "test/**"]
---

# Pure logic in `lib/`

Everything under `lib/` is header-only C++ that compiles and runs on the host,
so the interesting behaviour is testable without a board. `src/` is the only
place allowed to touch hardware or the network.

## Constraints

- **No `Arduino.h`, no `String`, no ESP-IDF headers.** Use `<stdint.h>`,
  `<stddef.h>` and fixed-size buffers. A class that needs `String` belongs in
  `src/`.
- **No `millis()` or any clock read.** Time arrives as a `nowMs` parameter.
  This is what makes rollover and interval behaviour testable.
- **No heap.** Storage is inline — see `NotificationQueue`, which is a template
  on capacity and message length precisely so a failing server cannot fragment
  memory.
- **No hardware, no globals, no I/O.** Callers own thread safety; these classes
  assume single-threaded use and say so.

## Every class gets a test

`lib/<group>/ClassName.h` requires `test/test_class_name/test_main.cpp`
(snake_case). The gate currently runs 10 suites for 10 classes — adding a class
without a test breaks the project's core convention, and nothing else will
catch it.

Tests use Unity with an explicit `main()` calling `RUN_TEST` for each case;
copy the shape from an existing suite. Run them with `pio test -e native`.

## Cover the behaviour that bites

- Boundaries: zero, one, capacity, truncation, empty input, `nullptr`.
- **`millis()` rollover.** Every duration comparison must be written as
  `nowMs - lastMs >= interval` so unsigned wraparound is harmless, and there
  should be a test that passes a `nowMs` near `0xFFFFFFFF`.
- The retry contract: several classes keep reporting an event until a
  `notificationSent()`-style call confirms delivery, so a dropped frame or a
  failed push is retried. Test that a _missing_ confirmation keeps firing.

## Do not reach for an abstraction

Prefer deleting duplication over adding a layer. A class here should have a
real second caller or a genuine testability reason to exist — serialization
helpers with one caller belong in `src/`.
