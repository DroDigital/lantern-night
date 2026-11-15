#!/usr/bin/env bash
# Re-sign the packaged app with a Developer ID (hardened runtime), notarize and staple it,
# then re-zip. Runs in CI only when the APPLE_* secrets are configured.
set -euo pipefail
DIST="$1"
KEYCHAIN="$RUNNER_TEMP/lk.keychain-db"
KC_PASS="$(openssl rand -hex 16)"
echo "$APPLE_CERTIFICATE" | base64 --decode > "$RUNNER_TEMP/cert.p12"
security create-keychain -p "$KC_PASS" "$KEYCHAIN"
security set-keychain-settings -lut 21600 "$KEYCHAIN"
security unlock-keychain -p "$KC_PASS" "$KEYCHAIN"
security import "$RUNNER_TEMP/cert.p12" -P "${APPLE_CERTIFICATE_PASSWORD:-}" -A -t cert -f pkcs12 -k "$KEYCHAIN"
security list-keychain -d user -s "$KEYCHAIN"
security set-key-partition-list -S apple-tool:,apple: -s -k "$KC_PASS" "$KEYCHAIN" > /dev/null
IDENTITY="$(security find-identity -v -p codesigning "$KEYCHAIN" | grep 'Developer ID Application' | head -1 | awk -F'"' '{print $2}')"
ZIP="$(ls "$DIST"/LanternKnight-*-macos-universal.zip | head -1)"
NAME="$(basename "$ZIP" .zip)"
APP="$DIST/$NAME/Lantern Knight.app"
codesign --force --deep --options runtime --timestamp --sign "$IDENTITY" "$APP"
rm -f "$ZIP"
(cd "$DIST" && ditto -c -k --sequesterRsrc --keepParent "$NAME" "$NAME.zip")
xcrun notarytool submit "$ZIP" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" --wait
xcrun stapler staple "$APP"
rm -f "$ZIP"
(cd "$DIST" && ditto -c -k --sequesterRsrc --keepParent "$NAME" "$NAME.zip")
echo "Signed and notarized $ZIP"
