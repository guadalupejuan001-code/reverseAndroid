# Original Android UI response validation

This workflow is prepared separately from the current original Android
continuation. It cannot launch until an admitted source has the original
live SystemServer and PID1 boot-completed gate, real peripheral links,
enforcing SELinux, authenticated scanout and a completed normal1024 CPU
shares restoration. A launch input is bound to the complete source hash,
instruction count and before-frame PPM hash. It uses the existing modeled
PL011 queue and normal Android Binder shell commands; no property or image
is injected.

One request wakes the display, dismisses keyguard, opens Android Settings
with `cmd activity start-activity -W --user 0 -a android.settings.SETTINGS`
and records the actual activity dump and exit status. Subsequent bounded
stages carry zero new input. The original epoch admission, native binary,
whole snapshot hash and process outcome checks remain mandatory.

The supplemental read-only app probe uses the pinned original observer,
actual BTF layouts, current mm_struct, page tables, exe_file, credentials
and command-line bytes. A process name cannot satisfy Settings identity.
The original observer file and guest snapshot remain unchanged and the
probe executes zero guest instructions.

Automatic response evidence requires all of:

- Successful actual launch status, Settings component and scoped resumed
  Settings activity for user0, with a successful real shell reply.
- Actual live Settings app_process64 identity, UID and actual Zygote parent.
- Original completed boot, actual SystemServer, SELinux enforcing and
  original required peripheral links at the same boundary.
- Authentic after-frame bytes different from the authenticated before frame.
- The same Settings and SystemServer PIDs at a second zero-input boundary.

Even after those pass, the controller leaves usable_UI_response_verified
false and visual_review_pending true until the actual exported frame is
visually reviewed. It must not mistake a boot-animation change for Settings.
This preparation is not evidence that the current source has completed boot
or that a UI request has been executed.

Commands were checked against primary AOSP implementation:

- [InputShellCommand](https://android.googlesource.com/platform/frameworks/base/+/master/services/core/java/com/android/server/input/InputShellCommand.java)
- [WindowManagerShellCommand](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/services/core/java/com/android/server/wm/WindowManagerShellCommand.java)
- [ActivityManagerShellCommand](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/services/core/java/com/android/server/am/ActivityManagerShellCommand.java)

Local tests exercise the exact nested request under sh and bash, failure
short-circuiting, echoed-command rejection, incomplete/timed-out replies
and process-identity false positives. Actual Android execution and visual
inspection remain required.
