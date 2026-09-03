# Area 51 Android build

This project uses the root CMake build and the SDL3 Android Java activity.
The SDL3 Java sources are referenced from `xCore/3rdParty/SDL3` so they are
not copied into the game sources.

Build with the Gradle wrapper shipped with SDL3:

```sh
xCore/3rdParty/SDL3/android-project/gradlew \
    -p Apps/GameApp/android \
    -PA51_ASSET_DIR=/absolute/path/to/android-assets \
    assembleDebug
```

`A51_ASSET_DIR` must contain the packaged runtime data, including the DFS
files expected by `level_loader`. The default `src/main/assets` directory is
kept empty as a project placeholder.
