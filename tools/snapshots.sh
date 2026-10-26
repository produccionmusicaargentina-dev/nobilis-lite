#!/usr/bin/env bash
# Renders the interface of every plugin in NOX and LUX into docs/ (Linux: needs xvfb-run).
#   cmake -B build -DLITE_BUILD_TESTS=ON -DLITE_REQUIRE_LOGIN=OFF && cmake --build build
#   tools/snapshots.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$ROOT/build"
shot() { # plugin theme preset file
  timeout 120 xvfb-run -a "$B/$1_Tests_artefacts/Release/$1_Tests" snapshot "$ROOT/docs/$4" "$2" "$3" "${SCALE:-1}" >/dev/null
  echo "docs/$4"
}
shot PULSUS nox "Breathing Pad"          PULSUS-NOX.png
shot PULSUS lux "Kick Room (Lows Only)"  PULSUS-LUX.png
shot ADEPS  nox "Reese Growl"            ADEPS-NOX.png
shot ADEPS  lux "Kick Thump"             ADEPS-LUX.png
shot LATUS  nox "String Machine"         LATUS-NOX.png
shot LATUS  lux "Poly Pad I"             LATUS-LUX.png
shot VETUS  nox "Cassette Demo"          VETUS-NOX.png
shot VETUS  lux "Dusty 45"               VETUS-LUX.png
shot VETUS  nox "Crunchy Chops"          VETUS-SAMPLER-NOX.png
shot VETUS  lux "AM Vocal"               VETUS-RADIO-LUX.png
