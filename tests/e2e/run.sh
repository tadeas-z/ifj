#!/bin/bash
# Implementation of the IFJ26 imperative language compiler
# run.sh
# Author(s): Marek Zachensky (xzachem00)

# AI-generated using: Claude Code
#              model: Claude Opus 5.5
# Prompt: "Create end-to-end black-box tests of the whole compiler based only
#          on the IFJ26 specification (base language and the extensions
#          FUNEXP, BOOLTHEN, VALMEM, CONTROL). Small programs for correct
#          and error cases with the expected output or exit code from the
#          specification, and a bash script that compiles them, runs them
#          in ic26int and reports what failed"
# Scope: whole file and all files in tests/e2e

# Usage: tests/e2e/run.sh [compiler] [interpreter]
COMP=${1:-build/ifj26}
INT=${2:-ic26int}
DIR=$(dirname "$0")
TMP=$(mktemp -d)
pass=0
fail=0

# ok/name.ifj: compiles, runs with exit 0 and prints ok/name.out
for src in "$DIR"/ok/*.ifj; do
    name=$(basename "$src" .ifj)
    input="$DIR/ok/$name.in"
    if [ ! -f "$input" ]; then
        input=/dev/null
    fi

    "$COMP" < "$src" > "$TMP/prog.code"
    rc=$?
    if [ $rc -ne 0 ]; then
        echo "FAIL ok/$name: compiler returned $rc"
        fail=$((fail + 1))
        continue
    fi

    "$INT" "$TMP/prog.code" < "$input" > "$TMP/prog.out"
    rc=$?
    # \r is removed, on Windows lines end with \r\n
    tr -d '\r' < "$TMP/prog.out" > "$TMP/got.txt"
    tr -d '\r' < "$DIR/ok/$name.out" > "$TMP/want.txt"
    if [ $rc -ne 0 ]; then
        echo "FAIL ok/$name: interpreter returned $rc"
        fail=$((fail + 1))
    elif ! cmp -s "$TMP/got.txt" "$TMP/want.txt"; then
        echo "FAIL ok/$name: wrong output"
        diff "$TMP/want.txt" "$TMP/got.txt" | head -5
        fail=$((fail + 1))
    else
        pass=$((pass + 1))
    fi
done

# err/CODE_name.ifj: compiler returns CODE, or for CODE >= 40 the program
# compiles and the interpreter returns CODE
for src in "$DIR"/err/*.ifj; do
    name=$(basename "$src" .ifj)
    want=${name%%_*}

    "$COMP" < "$src" > "$TMP/prog.code"
    rc=$?
    if [ "$want" -ge 40 ] && [ $rc -eq 0 ]; then
        "$INT" "$TMP/prog.code" < /dev/null > /dev/null 2>&1
        rc=$?
    fi

    if [ $rc -eq "$want" ]; then
        pass=$((pass + 1))
    else
        echo "FAIL err/$name: expected $want, got $rc"
        fail=$((fail + 1))
    fi
done

rm -rf "$TMP"
echo "passed $pass, failed $fail"
[ $fail -eq 0 ]
