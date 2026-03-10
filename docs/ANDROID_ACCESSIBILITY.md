# Android Accessibility Implementation

## Overview

This document describes the Android accessibility implementation for Diablo Access, which follows the same architecture pattern used by RetroArch to provide screen reader support for visually impaired players.

## Architecture

The implementation consists of three main components:

### 1. Java Layer (DevilutionXSDLActivity.java)

Located in `android-project/app/src/main/java/org/diasurgical/devilutionx/DevilutionXSDLActivity.java`

```java
public boolean isScreenReaderEnabled() {
    AccessibilityManager accessibilityManager = (AccessibilityManager) getSystemService(ACCESSIBILITY_SERVICE);
    boolean isAccessibilityEnabled = accessibilityManager.isEnabled();
    boolean isExploreByTouchEnabled = accessibilityManager.isTouchExplorationEnabled();
    return isAccessibilityEnabled && isExploreByTouchEnabled;
}

public void accessibilitySpeak(String message) {
    getWindow().getDecorView().announceForAccessibility(message);
}
```

**Key Features:**
- `isScreenReaderEnabled()`: Checks if TalkBack is enabled and touch exploration is active
- `accessibilitySpeak()`: Uses Android's native `announceForAccessibility()` API to speak text
- These methods are called from native C++ code via JNI

### 2. JNI Bridge (android.cpp)

Located in `Source/platform/android/android.cpp`

**Key Components:**

1. **Global State:**
   - `g_jvm`: Global JavaVM pointer
   - `g_activity`: Global reference to the Activity
   - `g_jniMethods`: Cached method IDs for performance

2. **Thread Management:**
   - Uses pthread thread-local storage to cache JNIEnv per thread
   - Automatically attaches threads to JVM as needed
   - Follows RetroArch's pattern for thread-safe JNI access

3. **Public API (namespace `accessibility`):**
   ```cpp
   bool InitializeScreenReaderAndroid();      // Check if initialized
   void ShutDownScreenReaderAndroid();       // Cleanup resources
   void SpeakTextAndroid(const char *text);  // Speak text
   bool IsScreenReaderEnabledAndroid();      // Check TalkBack status
   ```

4. **JNI Entry Point:**
   ```cpp
   JNIEXPORT void JNICALL Java_org_diasurgical_devilutionx_DevilutionXSDLActivity_nativeInitAccessibility(
       JNIEnv *env, jobject thiz)
   ```
   - Called from Java during Activity onCreate()
   - Stores JVM pointer and global activity reference
   - Caches method IDs for performance

### 3. Platform Integration (screen_reader.cpp)

Located in `Source/utils/screen_reader.cpp`

Modified to support Android alongside Windows and Linux:

```cpp
#ifdef _WIN32
    Tolk_Load();
#elif defined(__ANDROID__)
    devilution::accessibility::InitializeScreenReaderAndroid();
#else
    Speechd = spd_open("DevilutionX", "DevilutionX", NULL, SPD_MODE_SINGLE);
#endif
```

## How It Works

### Initialization Flow

1. App launches → `DevilutionXSDLActivity.onCreate()` is called
2. **IMPORTANT**: `super.onCreate()` must be called **before** `nativeInitAccessibility()`
   - This ensures SDL loads the native library first
   - Calling native methods before the library is loaded causes `UnsatisfiedLinkError`
3. `nativeInitAccessibility()` is called from Java (after `super.onCreate()`)
4. JNI function stores:
   - JavaVM pointer
   - Global reference to Activity
   - Method IDs for accessibility functions
5. Game calls `InitializeScreenReader()` → checks if Android is ready

**Critical Implementation Detail:**
```java
protected void onCreate(Bundle savedInstanceState) {
    // ... setup code ...

    super.onCreate(savedInstanceState);  // Must be FIRST - loads native library

    // Initialize accessibility JNI - must be after super.onCreate()
    // so that the native library is loaded first
    nativeInitAccessibility();
}
```

### Speaking Text Flow

1. Game code calls `SpeakText("Some text")`
2. `screen_reader.cpp` routes to `SpeakTextAndroid()` on Android
3. `SpeakTextAndroid()`:
   - Gets JNIEnv for current thread (attaches if needed)
   - Creates Java string from C string
   - Calls `accessibilitySpeak()` method on Activity
4. Java method calls `announceForAccessibility()`
5. Android's accessibility framework forwards to TalkBack
6. TalkBack speaks the text

### Thread Safety

The implementation is thread-safe:

- Each thread gets its own JNIEnv cached in thread-local storage
- The JavaVM pointer is global and constant
- The Activity reference is a global JNI reference (valid across threads)
- Method IDs are constant once initialized

This follows the same pattern as RetroArch's `jni_thread_getenv()` function.

## Comparison with Other Platforms

### Windows (Tolk)
- Uses NVDA/JAWS screen readers via Tolk library
- Direct communication with screen readers
- Requires Windows-specific APIs

### Linux (speech-dispatcher)
- Uses speech-dispatcher daemon
- Direct socket communication
- Requires speech-dispatcher to be running

### Android (this implementation)
- Uses Android's accessibility framework
- Integrates with TalkBack and other screen readers
- Uses `announceForAccessibility()` API
- Requires TalkBack to be enabled

## Advantages of This Approach

1. **Native Integration**: Uses Android's built-in accessibility APIs
2. **Works with All Screen Readers**: TalkBack, Samsung TalkBack, BrailleBack, etc.
3. **No External Dependencies**: Uses only Android SDK and NDK
4. **Performance**: Method IDs cached, thread-local storage for JNIEnv
5. **Thread-Safe**: Can be called from any thread
6. **Follows Best Practices**: Same pattern as RetroArch (proven in production)

## Differences from RetroArch

### Similarities
- Both use `announceForAccessibility()` for speaking
- Both cache JNI method IDs
- Both use thread-local storage for JNIEnv
- Both check `isTouchExplorationEnabled()`

### Minor Differences
- Diablo Access stores Activity as global reference; RetroArch uses android_app struct
- Diablo Access uses `pthread` directly; RetroArch wraps it in `jni_thread_getenv()`
- Diablo Access initialization happens in `onCreate()`; RetroArch uses native app glue

Both approaches are valid and work correctly.

## Testing

### Prerequisites
1. Enable TalkBack on Android device/emulator:
   - Settings → Accessibility → TalkBack → Enable
   - Enable "Explore by touch"

2. Build and install the app:
   ```bash
   cd android-project
   ./gradlew assembleDebug
   adb install app/build/outputs/apk/debug/app-debug.apk
   ```

3. Test gameplay:
   - Launch game
   - Navigate through menus
   - Listen for spoken feedback
   - Verify all accessibility features work

### Expected Behavior
- Menu items should be spoken
- Game state changes should be announced
- Tracker navigation should provide audio feedback
- Health/mana/status should be spoken

## Troubleshooting

### No Speech
- Verify TalkBack is enabled
- Check "Explore by touch" is enabled
- Verify device volume is up
- Check logcat for JNI errors

### Crashes
- **UnsatisfiedLinkError**: Ensure `nativeInitAccessibility()` is called AFTER `super.onCreate()`
  - The native library must be loaded by SDL before any native methods can be called
  - `super.onCreate()` triggers SDL to load the devilutionx library
- Check that `nativeInitAccessibility()` is called before other functions
- Verify method IDs are cached successfully
- Check thread attachment in logcat

### Common Issues and Solutions

#### Issue: App crashes on startup with `UnsatisfiedLinkError`
**Error:** `No implementation found for void org.diasurgical.devilutionx.DevilutionXSDLActivity.nativeInitAccessibility()`

**Cause:** Calling `nativeInitAccessibility()` before `super.onCreate()` in the Activity's `onCreate()` method.

**Solution:** Move `nativeInitAccessibility()` to after `super.onCreate()`:
```java
@Override
protected void onCreate(Bundle savedInstanceState) {
    // ... setup code ...
    super.onCreate(savedInstanceState);  // MUST be before nativeInitAccessibility()
    nativeInitAccessibility();        // Call AFTER super.onCreate()
}
```

**Why:** SDL loads the native library (`libdevilutionx.so`) during `super.onCreate()`. Any native method calls before this will fail because the library isn't loaded yet.

### Performance Issues
- Method ID caching should prevent repeated lookups
- Thread-local storage should minimize GetEnv calls
- Check for excessive JNI boundary crossings

## Future Enhancements

Possible improvements:
1. Add priority levels for announcements (like RetroArch)
2. Add speech speed control
3. Add haptic feedback integration
4. Support for accessibility gestures
5. Braille display integration

## References

- RetroArch Accessibility: https://github.com/libretro/RetroArch (accessibility.h, platform_unix.c)
- Android Accessibility: https://developer.android.com/guide/topics/ui/accessibility
- JNI Best Practices: https://developer.android.com/training/articles/perf-jni
