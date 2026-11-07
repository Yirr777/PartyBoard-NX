#!/bin/bash
# rspwrap.sh <tool> <args...>
# devkitA64 on Windows is a native program, so a command longer than Windows' 32 KiB limit fails
# ("Argument list too long") when MSYS starts it. This writes the arguments to a response file,
# with MSYS paths turned into Windows ones (MSYS converts command lines, not file contents), and
# runs the tool on it. Used for archiving and linking (see switch_pre_aurora.cmake).
tool="$1"
shift
rsp="$(mktemp "${TMPDIR:-/tmp}/rspwrap.XXXXXX")"
trap 'rm -f "$rsp"' EXIT
for a in "$@"; do
    a="$(printf '%s' "$a" | sed -E \
        -e 's#(^-[ILB]|^|[=,:])/opt/devkitpro#\1C:/msys64/opt/devkitpro#g' \
        -e 's#(^-[ILB]|^|[=,])/([a-zA-Z])/#\1\2:/#g')"
    a="${a//\\/\\\\}"
    a="${a//\"/\\\"}"
    printf '"%s"\n' "$a" >> "$rsp"
done
"$tool" "@$(cygpath -m "$rsp")"
