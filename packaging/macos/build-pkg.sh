#!/bin/bash
# Arma el instalador .pkg de NOBILIS LITE (los cuatro plugins: VST3 + AU, y las apps Standalone opcionales).
#   packaging/macos/build-pkg.sh <versión> <carpeta build> <salida.pkg>
set -euo pipefail

VERSION="${1:-1.0.0}"
BUILD="${2:-build}"
OUT="${3:-dist/NOBILIS-Lite-macOS.pkg}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
mkdir -p "$(dirname "$OUT")"

# Los scripts tienen que ser ejecutables: si el repo se subió desde Windows se pierde
# el permiso y el instalador falla con "Error durante la instalación".
mkdir -p "$WORK/scripts"
cp "$HERE/scripts/"* "$WORK/scripts/"
chmod 755 "$WORK/scripts/"*

component () {   # nombre  bundle  destino  identificador  [scripts]
  local name="$1" bundle="$2" dest="$3" id="$4" scripts="${5:-}"
  mkdir -p "$WORK/root-$name"
  ditto "$bundle" "$WORK/root-$name/$(basename "$bundle")"
  # Que el instalador NO "reubique" el bundle si encuentra otra copia en el disco
  pkgbuild --analyze --root "$WORK/root-$name" "$WORK/$name.plist"
  plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$name.plist"
  local extra=()
  [ -n "$scripts" ] && extra=(--scripts "$scripts")
  pkgbuild --root "$WORK/root-$name" --component-plist "$WORK/$name.plist" \
           --identifier "$id" --version "$VERSION" --install-location "$dest" \
           ${extra[@]+"${extra[@]}"} "$WORK/$name.pkg"
}

for P in PULSUS ADEPS LATUS VETUS; do
  p=$(echo "$P" | tr '[:upper:]' '[:lower:]')
  ART="$BUILD/${P}_artefacts/Release"
  component "$p-vst3" "$ART/VST3/$P.vst3"      "/Library/Audio/Plug-Ins/VST3"       "audio.nobilis.$p.vst3"
  component "$p-au"   "$ART/AU/$P.component"   "/Library/Audio/Plug-Ins/Components" "audio.nobilis.$p.au" "$WORK/scripts"
  component "$p-app"  "$ART/Standalone/$P.app" "/Applications/Nobilis"              "audio.nobilis.$p.app"
done

sed "s/__VERSION__/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
cp "$HERE/welcome.html" "$HERE/conclusion.html" "$WORK/"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" --resources "$WORK" "$OUT"
echo "Listo: $OUT"
