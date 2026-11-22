"""Verify pinned ESP-IDF pre-WebSocket handshake authentication linkage."""
import re
import subprocess

Import("env")
env.AppendUnique(LINKFLAGS=["-Wl,--wrap=httpd_ws_respond_server_handshake"])


def verify_guard(source, target, env):
    executable = str(target[0])
    # Checking the actual linked call, not just whether a wrapper symbol exists,
    # prevents silently upgrading a socket before the common URI handler guard.
    tool = env.subst("$OBJDUMP") or env.subst("$CC").replace("gcc", "objdump")
    result = subprocess.run([tool, "-d", "--disassemble=httpd_uri", executable],
                            env=env["ENV"], capture_output=True, text=True)
    if result.returncode or not re.search(r"call\w*\s+[^\n]*<__wrap_httpd_ws_respond_server_handshake>", result.stdout):
        raise RuntimeError("HTTP Basic pre-WebSocket guard is not linked; DO NOT DEPLOY")
    print("HTTP Basic pre-upgrade guard: linked and verified (ESP-IDF 4.4.7)")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_guard)
