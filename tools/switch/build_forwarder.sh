#!/bin/bash
# Builds the HOME menu forwarder: a small NSP that starts sdmc:/switch/partyboard/partyboard.nro as an
# application (full memory, no need to hold R) and returns to HOME when the game closes.
#
#   C:\msys64\usr\bin\bash.exe -l tools/switch/build_forwarder.sh /path/to/prod.keys
#
# Uses your console's own keys. They are only passed to hacBrewPack, must live outside this
# repository, and are never copied. Output: dist/PartyBoard-forwarder.nsp
#
# How it works (the same recipe as SwitchWaker's scripts/switch/build_forwarder.sh):
#   exefs    nx-hbloader v2.4.5 with platforms/switch/forwarder/nx-hbloader-forwarder.patch: it loads the
#            port directly and exits instead of starting hbmenu; main.npdm is its hbl.json with the
#            title ID below and application_type 1 (application).
#   control  control.nacp from nacptool, and platforms/switch/icon.jpg (256x256 JPEG) as the icon.
#   pack     hacBrewPack v3.05, without RomFS or logo sections.
#
# Title ID 01FF50424E580000: "01FF" is outside the ranges retail titles use, "50424E58" is "PBNX".
set -euo pipefail
KEYS=${1:?usage: build_forwarder.sh /path/to/prod.keys}
PORT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
case "$(cd "$(dirname "$KEYS")" && pwd)" in
    "$PORT"*) echo "build_forwarder.sh: keep the keys outside the repository" >&2; exit 1 ;;
esac
TITLE_ID=01FF50424E580000
WORK="$PORT/build-forwarder"
export DEVKITPRO=/opt/devkitpro
export PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/mingw64/bin:/usr/bin:$PATH"

fetch() {  # fetch DIR URL REV
    [ -d "$WORK/$1/.git" ] || git clone -q "$2" "$WORK/$1"
    git -C "$WORK/$1" checkout -q --force "$3"
    git -C "$WORK/$1" clean -q -fdx
}
mkdir -p "$WORK"
fetch nx-hbloader https://github.com/switchbrew/nx-hbloader.git 82b95122c5ae8dc059bf23893ba7623c72c86773
fetch hacBrewPack https://github.com/TooTallNate/hacBrewPack.git 745b16ecfc9ce055743067d200572204cb2aac6c

git -C "$WORK/nx-hbloader" apply "$PORT/platforms/switch/forwarder/nx-hbloader-forwarder.patch"
make -C "$WORK/nx-hbloader" -j4
cp "$WORK/hacBrewPack/config.mk.template" "$WORK/hacBrewPack/config.mk"
MSYSTEM=MINGW64 make -C "$WORK/hacBrewPack" -j4

STAGE="$WORK/stage"
rm -rf "$STAGE"; mkdir -p "$STAGE/exefs" "$STAGE/control" "$STAGE/out"
cp "$WORK/nx-hbloader/hbl.nso" "$STAGE/exefs/main"
cp "$WORK/nx-hbloader/hbl.npdm" "$STAGE/exefs/main.npdm"
cp "$PORT/platforms/switch/icon.jpg" "$STAGE/control/icon_AmericanEnglish.dat"
nacptool --create "Party Board" "Yirr777" "0.1.0" "$STAGE/control/control.nacp" --titleid=$TITLE_ID

# hacBrewPack's log shows values derived from the keys, so it is not kept.
(cd "$STAGE" && "$WORK/hacBrewPack/hacbrewpack" -k "$KEYS" --titleid $TITLE_ID --exefsdir exefs \
    --controldir control --noromfs --nologo --nspdir out --backupdir backup > /dev/null)
rm -rf "$STAGE/backup"
mkdir -p "$PORT/dist"
cp "$STAGE/out/"*.nsp "$PORT/dist/PartyBoard-forwarder.nsp"
echo "==> dist/PartyBoard-forwarder.nsp"
