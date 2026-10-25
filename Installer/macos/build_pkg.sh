#!/bin/bash
# Gera African-Drum-<versão>-macOS.pkg e .dmg  (correr num Mac, depois de compilar)
# Uso: ./build_pkg.sh [versão] [pasta-artefactos]
# Opcional (assinatura Apple): DEV_ID_APP="Developer ID Application: Nome (TEAM)"  DEV_ID_INSTALLER="Developer ID Installer: Nome (TEAM)"
#                              NOTARY_PROFILE="perfil-do-notarytool"
set -euo pipefail
cd "$(dirname "$0")"
VERSION="${1:-1.0.0}"
ART="${2:-../../build/DrumForge_artefacts/Release}"
ART="$(cd "$ART" && pwd)"
OUT="$(pwd)/../output"; WORK="$(mktemp -d)"; mkdir -p "$OUT"
SIGN="${DEV_ID_APP:--}"

echo "==> Assinar bundles (${DEV_ID_APP:-ad-hoc})"
for b in "$ART/VST3/African Drum.vst3" "$ART/AU/African Drum.component" "$ART/Standalone/African Drum.app"; do
  [ -e "$b" ] || { echo "AVISO: $b não existe, a saltar"; continue; }
  xattr -cr "$b" || true
  if [ -n "${DEV_ID_APP:-}" ]; then codesign --force --deep --options runtime --timestamp -s "$SIGN" "$b"
  else codesign --force --deep -s - "$b"; fi
done

mk() { # id nome origem destino
  local id=$1 src=$2 dest=$3 root="$WORK/root_$1"
  [ -e "$src" ] || return 0
  mkdir -p "$root$dest"; cp -R "$src" "$root$dest/"
  local args=(--root "$root" --identifier "com.dblproduction.drums.$id" --version "$VERSION" --install-location /)
  [ -n "${DEV_ID_INSTALLER:-}" ] && args+=(--sign "$DEV_ID_INSTALLER")
  pkgbuild "${args[@]}" "$WORK/$id.pkg" >/dev/null
}
echo "==> Componentes"
mk vst3 "$ART/VST3/African Drum.vst3" /Library/Audio/Plug-Ins/VST3
mk au   "$ART/AU/African Drum.component" /Library/Audio/Plug-Ins/Components
mk app  "$ART/Standalone/African Drum.app" /Applications

sed "s/@VERSION@/$VERSION/g" distribution.xml.in > "$WORK/distribution.xml"
PKG="$OUT/African-Drum-$VERSION-macOS.pkg"
pargs=(--distribution "$WORK/distribution.xml" --resources resources --package-path "$WORK")
[ -n "${DEV_ID_INSTALLER:-}" ] && pargs+=(--sign "$DEV_ID_INSTALLER")
productbuild "${pargs[@]}" "$PKG"

if [ -n "${NOTARY_PROFILE:-}" ]; then
  echo "==> Notarização"; xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait; xcrun stapler staple "$PKG"
fi

echo "==> DMG"
D="$WORK/dmg"; mkdir -p "$D"; cp "$PKG" "$D/"; cp ../INSTALL.md "$D/LEIA-ME.txt" 2>/dev/null || true
hdiutil create -volname "African Drum $VERSION" -srcfolder "$D" -ov -format UDZO "$OUT/African-Drum-$VERSION-macOS.dmg" >/dev/null
rm -rf "$WORK"; echo "Pronto: $OUT"; ls -lh "$OUT"
