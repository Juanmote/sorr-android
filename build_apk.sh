#!/bin/bash
# Builds the SorR Android APK without Gradle (aapt2 + javac + d8 + apksigner).
set -euo pipefail

# Everything can be overridden from the environment (the GitHub workflow does).
ROOT=${ROOT:-$(cd "$(dirname "$0")" && pwd)}
SDK=${ANDROID_SDK:-/home/claude/android/sdk}
BT=$SDK/build-tools/35.0.0
NDK=${NDK:-$SDK/ndk/27.2.12479018}
PLATFORM=$SDK/platforms/android-34/android.jar
APP=$ROOT/app
OUT=$ROOT/out
KEYSTORE=${SORR_KEYSTORE:-$ROOT/sorr-release.jks}
KS_PASS=${SORR_KEYSTORE_PASS:-sorrport}
KEY_PASS=${SORR_KEY_PASS:-$KS_PASS}
KEY_ALIAS=${SORR_KEY_ALIAS:-sorr}
JOBS=${JOBS:-$(nproc)}
export KS_PASS KEY_PASS

# ---- native (arm64-v8a + armeabi-v7a)
for ABI in ${ABIS:-arm64-v8a armeabi-v7a}; do
  B=$ROOT/native/build-$ABI
  if [ ! -f "$B/CMakeCache.txt" ]; then
    cmake -S $ROOT/native -B $B -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-28 -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release >/dev/null
  fi
  cmake --build $B -j$JOBS 2>/dev/null >/dev/null || cmake --build $B -j$JOBS
done

rm -rf $OUT && mkdir -p $OUT/classes $OUT/gen $OUT/apk/lib
STRIP=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip
for ABI in ${ABIS:-arm64-v8a armeabi-v7a}; do
  mkdir -p $OUT/apk/lib/$ABI
  cp $ROOT/native/build-$ABI/SDL2/libSDL2.so $ROOT/native/build-$ABI/engine/libmain.so $OUT/apk/lib/$ABI/
  $STRIP --strip-unneeded $OUT/apk/lib/$ABI/*.so
done

# ---- resources
$BT/aapt2 compile --dir $APP/res -o $OUT/res.zip
$BT/aapt2 link -o $OUT/base.apk -I $PLATFORM --manifest $APP/AndroidManifest.xml \
  --min-sdk-version 28 --target-sdk-version 34 -A $APP/assets --java $OUT/gen $OUT/res.zip \
  ${SORR_VERSION_CODE:+--version-code $SORR_VERSION_CODE --replace-version} ${SORR_VERSION_NAME:+--version-name $SORR_VERSION_NAME}

# ---- java
find $APP/src/java $OUT/gen -name '*.java' > $OUT/sources.txt
javac -nowarn --release 8 -encoding UTF-8 -classpath $PLATFORM \
  -d $OUT/classes @$OUT/sources.txt 2>&1 | grep -v -E 'JAVA_TOOL_OPTIONS|^Note:' || true
[ -f $OUT/classes/org/sorr/remake/LauncherActivity.class ] || { echo "javac failed"; exit 1; }
$BT/d8 --release --min-api 28 --lib $PLATFORM --output $OUT/apk $(find $OUT/classes -name '*.class')

# ---- package: native libs stored uncompressed + page aligned (extractNativeLibs=false)
cp $OUT/base.apk $OUT/unaligned.apk
(cd $OUT/apk && zip -q -0 -r ../unaligned.apk lib && zip -q ../unaligned.apk classes.dex)
$BT/zipalign -f -P 16 4 $OUT/unaligned.apk $OUT/aligned.apk

if [ ! -f $KEYSTORE ]; then
  if [ -n "${SORR_REQUIRE_KEY:-}" ]; then echo "ERROR: no signing key ($KEYSTORE)"; exit 1; fi
  keytool -genkeypair -keystore $KEYSTORE -storepass sorrport -keypass sorrport -alias sorr \
    -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=SorR Android Port" >/dev/null 2>&1
fi
$BT/apksigner sign --ks $KEYSTORE --ks-pass env:KS_PASS --key-pass env:KEY_PASS --ks-key-alias "$KEY_ALIAS" \
  --out $OUT/SorR-Android.apk $OUT/aligned.apk
$BT/apksigner verify $OUT/SorR-Android.apk && echo "APK OK: $OUT/SorR-Android.apk"
