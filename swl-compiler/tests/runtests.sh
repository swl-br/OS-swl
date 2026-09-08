#!/bin/sh
set -eu
repo="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo"
build_arg="${1:-}"
if [ -n "$build_arg" ]; then
  b="$build_arg/swlc"
  rt="$build_arg/swlrt.o"
else
  b="$repo/build/swlc"
  rt="$repo/build/swlrt.o"
fi
tmpdir="$(mktemp -d)"
pass=0
fail=0
echo "running swlc end-to-end test suite"
for swl in examples/*.swl; do
  [ -f "$swl" ] || continue
  name="$(basename "$swl" .swl)"
  asm="$tmpdir/${name}.asm"
  obj="$tmpdir/${name}.o"
  exe="$tmpdir/${name}"
  out="$tmpdir/out.txt"
  if ! "$b" -o "$asm" "$swl" >/dev/null 2>&1; then
    echo "FAIL [$name] compile"; fail=$((fail+1)); continue;
  fi
  if ! nasm -f elf32 -o "$obj" "$asm" >/dev/null 2>&1; then
    echo "FAIL [$name] nasm"; fail=$((fail+1)); continue;
  fi
  if ! ld -m elf_i386 -o "$exe" "$obj" "$rt" >/dev/null 2>&1; then
    echo "FAIL [$name] ld"; fail=$((fail+1)); continue;
  fi
  if "$exe" >"$out" 2>&1; then
    exitcode=0
  else
    exitcode=$?
  fi
  want_exit=""
  [ -f "examples/${name}.exit" ] && want_exit="$(cat examples/${name}.exit)"
  if [ -n "$want_exit" ] && [ "$exitcode" != "$want_exit" ]; then
    echo "FAIL [$name] exit code: got=$exitcode want=$want_exit"; fail=$((fail+1)); continue;
  fi
  if [ -f "examples/${name}.out" ]; then
    if ! diff -q "$out" "examples/${name}.out" >/dev/null 2>&1; then
      echo "FAIL [$name] stdout mismatch"
      echo "--- got ---"; cat "$out"
      echo "--- want ---"; cat "examples/${name}.out"
      fail=$((fail+1)); continue;
    fi
  fi
  echo "ok   [$name] exit=$exitcode"; pass=$((pass+1))
done
for swl in tests/fail/*.swl; do
  [ -f "$swl" ] || continue
  name="$(basename "$swl" .swl)"
  asm="$tmpdir/${name}.asm"
  err="$tmpdir/err.txt"
  if "$b" -o "$asm" "$swl" >/dev/null 2>&1; then
    echo "FAIL [$name] expected rejection, but compiled"; fail=$((fail+1)); continue;
  fi
  if "$b" -o "$asm" "$swl" >"$err" 2>&1 && ! grep -qE 'error:' "$err"; then
    echo "FAIL [$name] compiled but no error message"; fail=$((fail+1)); continue;
  fi
  echo "ok   [$name] rejected"; pass=$((pass+1))
done
echo
echo "summary: $pass passed, $fail failed"
if [ "$fail" -gt 0 ]; then exit 1; fi
echo "all tests passed"
exit 0
