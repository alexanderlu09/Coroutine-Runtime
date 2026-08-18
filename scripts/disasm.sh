#!/usr/bin/env bash
# Disassemble the coroutine anatomy example so you can see the state machine
# the compiler generated. See examples/coro_anatomy.cpp for what to look for.
set -euo pipefail

cd "$(dirname "$0")/.."
OUT=build/disasm
mkdir -p "$OUT"

echo "==> compiling at -O0 (readable, close to the source)"
clang++ -std=c++20 -O0 -g -c examples/coro_anatomy.cpp -o "$OUT/anatomy_O0.o"

echo "==> compiling at -O2 (what actually ships)"
clang++ -std=c++20 -O2 -g -c examples/coro_anatomy.cpp -o "$OUT/anatomy_O2.o"

for lvl in O0 O2; do
  objdump -d --demangle "$OUT/anatomy_$lvl.o" > "$OUT/anatomy_$lvl.s"
  echo "    wrote $OUT/anatomy_$lvl.s"
done

echo
echo "The three functions the compiler split your coroutine into:"
echo
objdump -d --demangle "$OUT/anatomy_O0.o" \
  | grep -E '^[0-9a-f]+ <.*counter.*>:' || true
echo
echo "  '.resume'  -- the state machine (find the switch on the suspend index)"
echo "  '.destroy' -- destructors for whatever is live, then frees the frame"
echo "  bare name  -- the ramp: allocates the frame and returns the handle"
echo
echo "Open $OUT/anatomy_O0.s and read the ramp first. It is ~30 instructions."
