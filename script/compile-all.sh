#!/bin/bash

WORK_DIR="$(pwd)"
rm -rf "$WORK_DIR"/bin/*

mkdir -p "$WORK_DIR/bin/.o/"
cd "$WORK_DIR/bin/.o/"

# Collect all sources first so we can show progress as [i/N].
mapfile -t FILES < <(find "$WORK_DIR/src/c++/" -type f -name "*.cpp")
TOTAL=${#FILES[@]}
START=$(date +%s)

echo "Compiling $TOTAL source file(s)..."
i=0
for file in "${FILES[@]}"; do
    i=$((i + 1))
    printf "[%2d/%2d] %-45s " "$i" "$TOTAL" "$(basename "$file")"
    t0=$(date +%s)
    if g++ -I "$WORK_DIR/include" -I "$WORK_DIR/lib" --std=c++20 -c "$file"; then
        echo "ok ($(( $(date +%s) - t0 ))s)"
    else
        echo "FAILED"
        echo "Compilation aborted on $file" >&2
        exit 1
    fi
done

echo "Linking bin/main..."
cd "$WORK_DIR"/bin/
if ! g++ $(find "$WORK_DIR"/bin/.o/ -name "*.o" ! -name "main.o" -type f | xargs) "$WORK_DIR"/bin/.o/main.o -o main; then
    echo "Linking failed" >&2
    exit 1
fi

echo "Done in $(( $(date +%s) - START ))s -> bin/main"
exit 0
