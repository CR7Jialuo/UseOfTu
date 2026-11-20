#!/usr/bin/env bash
# End-to-end checks for the CLI (Linux/macOS, used by CI).
set -u

cd "$(dirname "$0")/.." || exit 1
EXE=./build/nav
FAIL=0

check() {
    name="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        printf '  OK   %s\n' "$name"
    else
        printf '  FAIL %s\n' "$name"
        FAIL=$((FAIL + 1))
    fi
}

has() {
    text="$1"
    needle="$2"
    case "$text" in
        *"$needle"*) return 0 ;;
        *) return 1 ;;
    esac
}

out=$("$EXE" list)
has "$out" '图书馆' && printf '  OK   list\n' || { printf '  FAIL list\n'; FAIL=$((FAIL + 1)); }

json=$("$EXE" route 16 4 --json)
echo "$json" | grep -q '"count":4' && echo "$json" | grep -q '"nodes":\[16,14,22,4\]' \
    && printf '  OK   route json\n' || { printf '  FAIL route json\n'; FAIL=$((FAIL + 1)); }

"$EXE" route 东大门 图书馆 --by time --json | grep -q '"criterion":"time"' \
    && printf '  OK   route by time\n' || { printf '  FAIL route by time\n'; FAIL=$((FAIL + 1)); }

"$EXE" route 1 1 --json | grep -q '"count":1' \
    && printf '  OK   route same spot\n' || { printf '  FAIL route same spot\n'; FAIL=$((FAIL + 1)); }

"$EXE" route 1 999 --json >/dev/null 2>&1
[ $? -eq 2 ] && printf '  OK   route invalid exit 2\n' || { printf '  FAIL route invalid\n'; FAIL=$((FAIL + 1)); }

"$EXE" near 宿舍A区 canteen | grep -q '学生一食堂' \
    && printf '  OK   near\n' || { printf '  FAIL near\n'; FAIL=$((FAIL + 1)); }

"$EXE" tour 东大门 图书馆 第一教学楼 | grep -q '游览顺序' \
    && printf '  OK   tour\n' || { printf '  FAIL tour\n'; FAIL=$((FAIL + 1)); }

critical=$("$EXE" critical)
has "$critical" '割点' && has "$critical" '观海亭' \
    && printf '  OK   critical\n' || { printf '  FAIL critical\n'; FAIL=$((FAIL + 1)); }

"$EXE" mst | grep -q 'Prim' && printf '  OK   mst\n' || { printf '  FAIL mst\n'; FAIL=$((FAIL + 1)); }
"$EXE" allpairs --limit 4 | grep -q 'Floyd-Warshall' && printf '  OK   allpairs\n' \
    || { printf '  FAIL allpairs\n'; FAIL=$((FAIL + 1)); }

map=$("$EXE" map)
has "$map" '东大门' && has "$map" '图例' && printf '  OK   map\n' || { printf '  FAIL map\n'; FAIL=$((FAIL + 1)); }

"$EXE" check >/dev/null 2>&1 && printf '  OK   check\n' || { printf '  FAIL check\n'; FAIL=$((FAIL + 1)); }
"$EXE" bench --nodes 40 --repeat 2 | grep -q 'Dijkstra' && printf '  OK   bench\n' \
    || { printf '  FAIL bench\n'; FAIL=$((FAIL + 1)); }

rm -rf build/e2e_data
"$EXE" export build/e2e_data >/dev/null 2>&1
if diff <(grep -v '^#' data/campus/spots.csv) <(grep -v '^#' build/e2e_data/spots.csv) >/dev/null \
   && diff <(grep -v '^#' data/campus/roads.csv) <(grep -v '^#' build/e2e_data/roads.csv) >/dev/null; then
    printf '  OK   export matches repo data\n'
else
    printf '  FAIL export mismatch\n'
    FAIL=$((FAIL + 1))
fi

echo
if [ "$FAIL" -eq 0 ]; then
    echo 'e2e passed.'
    exit 0
fi
echo "e2e failed: $FAIL case(s)."
exit 1
