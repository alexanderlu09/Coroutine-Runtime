# Coroutine-Runtime

An async runtime for C++: a work-stealing multicore scheduler, pluggable I/O
backends, and a deterministic simulation mode where every concurrency bug is
reproducible from a seed.

> **Status: week 1 of 8.** Scaffolding only so far. This README will be
> rewritten around the benchmark results once there is something to measure.

## Build

```sh
cmake --preset debug          # configure
cmake --build --preset debug  # build
ctest  --preset debug         # test
```

Other presets: `release` (use this for **all** benchmark numbers), `asan`,
`tsan`, `ubsan`. Run the `tsan` preset often — this is a concurrency project
and a race you find in week 2 costs an hour, while the same race found in
week 7 costs a weekend.

## Layout

```
include/coro/   public headers
src/            implementation
tests/          doctest suite  (add new files to tests/CMakeLists.txt)
examples/       runnable demos
bench/          benchmarks and baselines
docs/           design notes, ADRs, benchmark methodology
docs/BUGS.md    every non-trivial bug, with its reproducing seed
scripts/        tooling
```

## Start here

`examples/coro_anatomy.cpp` — a 20-minute exercise that shows what a coroutine
actually compiles into. Run it, then run `./scripts/disasm.sh`.
