"""
Inject the firmware version into the build.

Runs `git describe --tags --always --dirty` at build time and:
  1. Defines OPENSONOS_FW_VERSION (a C string literal) for the firmware, so
     fw::version() and GET /status report exactly this build.
  2. Writes the same value to `<build_dir>/fw_version.txt`, a sidecar the
     host-side fleet updater reads so the operator can't mistype the target
     version it waits for after a reboot.

Falls back to "unknown" when git metadata is unavailable (e.g. a source
tarball with no .git), so the reported version is never empty.
"""
Import("env")  # noqa: F821 — injected by PlatformIO/SCons
import os
import subprocess


def _git_describe(project_dir):
    try:
        out = subprocess.run(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=project_dir,
            capture_output=True,
            text=True,
            timeout=10,
        )
        if out.returncode == 0:
            v = out.stdout.strip()
            if v:
                return v
    except (OSError, subprocess.SubprocessError):
        pass
    return "unknown"


project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")
version = _git_describe(project_dir)

# 1. Define the C macro as a properly-escaped string literal.
env.Append(CPPDEFINES=[("OPENSONOS_FW_VERSION", env.StringifyMacro(version))])

# 2. Write the sidecar so the fleet updater can read the exact built version.
os.makedirs(build_dir, exist_ok=True)
with open(os.path.join(build_dir, "fw_version.txt"), "w") as f:
    f.write(version + "\n")

print("INFO [fw_version]: firmware version = %s" % version)
