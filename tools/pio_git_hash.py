# PlatformIO pre-build script: puts the git commit into the SD log header (GIT_HASH).
# Idea from CAN-Insight's self-describing log header (see SD-LOGGER-UPDATE.md).
import subprocess
Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

def _git(*args):
    return subprocess.check_output(["git", *args], cwd=env["PROJECT_DIR"],  # noqa: F821
                                   stderr=subprocess.DEVNULL).decode().strip()

try:
    rev = _git("rev-parse", "--short=10", "HEAD")
    if _git("status", "--porcelain", "--untracked-files=no"):
        rev += "+"          # uncommitted changes
except Exception:
    rev = "unknown"

env.Append(CPPDEFINES=[("GIT_HASH", env.StringifyMacro(rev))])  # noqa: F821
