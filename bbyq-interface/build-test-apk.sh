#!/usr/bin/env bash
# Local parallel test edition. The unchanged JNI libraries come from a verified
# first-party Workbench APK; all Java, resources, UI and BBY Q assets are rebuilt.
set -euo pipefail
tools="$(realpath "${1:?Android build-tools 35 directory}")"
platform="$(realpath "${2:?Android 35 android.jar}")"
baseline="$(realpath "${3:?Previously delivered first-party APK}")"
key="$(realpath -m "${4:?Retained development signing keystore}")"
out="$(realpath -m "${5:?Output directory}")"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$out/classes" "$out/dex" "$out/stage/lib/arm64-v8a" "$out/stage/lib/x86_64"
assets="$(mktemp -d "$out/assets.XXXXXX")"
cp -a "$repo/reverse-os-android/app/src/main/assets/." "$assets/"
test -s "$repo/reverse-os-android/app/src/main/assets/native-apps/BBY-Q.rxe"
"$tools/apksigner" verify --verbose --print-certs "$baseline" > "$out/engine-apk-signature.txt"
for abi in arm64-v8a x86_64; do
  unzip -p "$baseline" "lib/$abi/libreverse_os.so" > "$out/stage/lib/$abi/libreverse_os.so"
  test -s "$out/stage/lib/$abi/libreverse_os.so"
done
java -m jdk.compiler/com.sun.tools.javac.Main -source 17 -target 17 -cp "$platform" \
  -d "$out/classes" "$repo"/reverse-os-android/app/src/main/java/com/reversepc/os/*.java
java -m jdk.jartool/sun.tools.jar.Main --create --file "$out/classes.jar" -C "$out/classes" .
java -cp "$tools/lib/d8.jar" com.android.tools.r8.D8 --release --min-api 29 --lib "$platform" --output "$out/dex" "$out/classes.jar"
node - "$repo" "$out" "$assets" "$baseline" <<'JS'
const fs=require('fs'),path=require('path'),crypto=require('crypto'),cp=require('child_process');const [repo,out,assets,baseline]=process.argv.slice(2);
let manifest=fs.readFileSync(path.join(repo,'reverse-os-android/app/src/main/AndroidManifest.xml'),'utf8');
manifest=manifest.replace('<manifest xmlns:android=', '<manifest package="com.reversepc.control.workbench" xmlns:android=')
 .replaceAll('${appLabel}','REVERSE').replaceAll('android:name=".','android:name="com.reversepc.os.')
 .replace(/\s*<activity android:name="com\.reversepc\.os\.HomeActivity"[\s\S]*?<\/activity>/,'');
fs.writeFileSync(path.join(out,'AndroidManifest.xml'),manifest);
const build={source_commit:cp.execFileSync('git',['-C',repo,'rev-parse','HEAD'],{encoding:'utf8'}).trim(),base_reverse_commit:'cf7854fc0499801c3206fea38043806060b9ed5d',mundo_x_commit:'e2778d962af22f6f1a0794058dd4685fc2573966',variant:'parallel-control-test',native_engine_apk_sha256:crypto.createHash('sha256').update(fs.readFileSync(baseline)).digest('hex')};
fs.writeFileSync(path.join(assets,'workbench/build-info.json'),JSON.stringify(build)+'\n');
JS
"$tools/aapt2" compile --dir "$repo/reverse-os-android/app/src/main/res" -o "$out/resources.zip"
"$tools/aapt2" link -o "$out/resources.apk" -I "$platform" --manifest "$out/AndroidManifest.xml" \
  --min-sdk-version 29 --target-sdk-version 35 --version-code 242 --version-name 2.4.2-control \
  --auto-add-overlay -A "$assets" "$out/resources.zip"
cp "$out/resources.apk" "$out/unaligned.apk"
java -m jdk.jartool/sun.tools.jar.Main --update --no-compress --file "$out/unaligned.apk" -C "$out/dex" classes.dex -C "$out/stage" lib
"$tools/zipalign" -P 16 -f 4 "$out/unaligned.apk" "$out/aligned.apk"
umask 077
mkdir -p "$(dirname "$key")"
if [[ ! -f "$key" ]]; then
  keytool -genkeypair -keystore "$key" -storetype JKS -storepass android -keypass android -alias reverse-desktop \
    -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=REVERSE Control Development'
fi
"$tools/apksigner" sign --ks "$key" --ks-key-alias reverse-desktop --ks-pass pass:android --key-pass pass:android \
  --out "$out/REVERSE-Control-v2.4.2.apk" "$out/aligned.apk"
"$tools/apksigner" verify --verbose --print-certs "$out/REVERSE-Control-v2.4.2.apk" > "$out/signature.txt"
"$tools/zipalign" -c -P 16 -v 4 "$out/REVERSE-Control-v2.4.2.apk" > "$out/alignment.txt"
"$tools/aapt" dump badging "$out/REVERSE-Control-v2.4.2.apk" > "$out/package.txt"
sha256sum "$out/REVERSE-Control-v2.4.2.apk" "$baseline" "$out/stage/lib/arm64-v8a/libreverse_os.so" "$out/stage/lib/x86_64/libreverse_os.so" > "$out/SHA256SUMS"
printf 'CONTROL_APK_BUILT %s\n' "$out/REVERSE-Control-v2.4.2.apk"
