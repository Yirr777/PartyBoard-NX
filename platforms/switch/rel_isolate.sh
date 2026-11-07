#!/bin/sh
# rel_isolate.sh <name> <out.o> <binutils-prefix> <objects...>
# Links one REL's objects into a relocatable object whose only global definition is
# mp4rel_<name>_ObjectSetup, with its initialised writable data in the section mp4ovl_<name> and
# its zero-initialised data in mp4bss_<name>, so a relink can restore both.
set -e
NAME="$1"
OUT="$2"
P="$3"
shift 3

TMP="$OUT.tmp"
mkdir -p "$(dirname "$OUT")"
"${P}ld" -r -o "$TMP.1" "$@"

LEFT=$("${P}size" -A "$TMP.1" | awk '$1 ~ /^\.(tbss|tdata|sbss|sdata)/ && $2 > 0 {print $1 " " $2}')
if [ -n "$LEFT" ]; then
    echo "rel_isolate.sh: $NAME has thread-local or small-data state a relink would not reset:" >&2
    echo "$LEFT" >&2
    exit 1
fi

"${P}objcopy" \
    --keep-global-symbol=ObjectSetup \
    --rename-section .data="mp4ovl_$NAME" \
    --rename-section .data.rel="mp4ovl_$NAME" \
    --rename-section .data.rel.local="mp4ovl_$NAME" \
    --rename-section .bss="mp4bss_$NAME" \
    "$TMP.1" "$TMP.2"
"${P}objcopy" --redefine-sym ObjectSetup="mp4rel_${NAME}_ObjectSetup" "$TMP.2" "$OUT"
rm -f "$TMP.1" "$TMP.2"
