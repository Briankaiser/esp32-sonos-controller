"""
Patch lv_blend_helium.S in the LVGL libdeps cache before the assembler sees it.
The file contains ARM Helium SIMD assembly; on Xtensa (ESP32-S3) the assembler
rejects it with "unknown opcode or format name 'typedef'".

Runs immediately at script-load time (before SCons queues any compile jobs).
Idempotent: safe to run repeatedly.
"""
Import("env")  # noqa: F821 — injected by PlatformIO/SCons
import os

libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
pioenv  = env.subst("$PIOENV")
helium  = os.path.join(
    libdeps, pioenv,
    "lvgl", "src", "draw", "sw", "blend", "helium", "lv_blend_helium.S",
)

if os.path.exists(helium):
    with open(helium) as f:
        content = f.read()
    if "ARM Helium disabled" not in content:
        with open(helium, "w") as f:
            f.write("/* ARM Helium disabled for Xtensa/ESP32-S3 — not an ARM target */\n")
        print("INFO [exclude_arm_asm]: patched lv_blend_helium.S for Xtensa compatibility")
