# SorR Android port (BennuGD monolithic + SDL2)

- engine/   : bennugd-monolithic (from isage/sorr-vita) patched for Android arm64:
              lowmem allocator (<2GB arena), 64-bit fixes (DCB_FILE read, A2STR/STR2A,
              libtext, mod_sort), mod_sound RWops size, upstream dirs.c, letterbox scaling,
              crash handler + sorr_log.txt.
- app/      : Java launcher (profiles incl. "Unlocked", touch overlay, gamepad->keys) + SDL2 Java glue.
- native/   : top-level CMake. Needs alongside (in src/): SDL2 release-2.30.9,
              SDL_mixer release-2.8.0, libpng v1.6.44 (unmodified upstream).
- build_apk.sh : builds without Gradle (SDK build-tools 35, NDK r27c).
- tests/    : BennuGD test programs used to validate the 64-bit engine.
