#!/bin/bash
# Installs the Loopsmith plugin (AU for Logic, VST3 for FL Studio / Ableton / others).
cd "$(dirname "$0")" || exit 1
HERE="$(pwd)"
COMP="$HOME/Library/Audio/Plug-Ins/Components"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"

echo
echo "  ===== Installing the Loopsmith plugin ====="
echo
if [ ! -d "$HERE/Loopsmith.component" ] || [ ! -d "$HERE/Loopsmith.vst3" ]; then
  echo "  [!] Loopsmith.component and Loopsmith.vst3 should be next to this file."
  echo "      Unzip Loopsmith-Plugin-Mac.zip again and run this from that folder."
  read -r -p "  Press Return to close. " _
  exit 1
fi

mkdir -p "$COMP" "$VST3"
rm -rf "$COMP/Loopsmith.component" "$VST3/Loopsmith.vst3"
cp -R "$HERE/Loopsmith.component" "$COMP/" || { echo "  [!] Couldn't copy the AU plugin."; read -r -p "  Press Return. " _; exit 1; }
cp -R "$HERE/Loopsmith.vst3" "$VST3/" || { echo "  [!] Couldn't copy the VST3 plugin."; read -r -p "  Press Return. " _; exit 1; }

# Files downloaded from the internet are flagged by macOS; DAWs refuse flagged plugins.
xattr -dr com.apple.quarantine "$COMP/Loopsmith.component" "$VST3/Loopsmith.vst3" 2>/dev/null
xattr -dr com.apple.quarantine "$HERE" 2>/dev/null
# Makes Logic and GarageBand notice the new Audio Unit right away.
killall -9 AudioComponentRegistrar 2>/dev/null

echo "  Installed:"
echo "    AU   (Logic Pro, GarageBand, also FL Studio and Ableton):  $COMP/Loopsmith.component"
echo "    VST3 (FL Studio, Ableton Live, Reaper and others):         $VST3/Loopsmith.vst3"
echo
echo "  Next: quit and reopen your DAW, then add Loopsmith to an audio track."
echo "  The Read me explains where to find it in Logic, FL Studio and Ableton."
echo
read -r -p "  Press Return to close. " _
