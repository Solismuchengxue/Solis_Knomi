"""Run with the existing PlatformIO Python; no packages or device required."""
from pathlib import Path
import subprocess
import unittest
import winreg
import runpy
import re
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def native_toolchain():
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    vs = Path(subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip())
    vc = sorted((vs / "VC/Tools/MSVC").iterdir())[-1]
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows Kits\Installed Roots") as key:
        kits = Path(winreg.QueryValueEx(key, "KitsRoot10")[0])
    version = sorted((kits / "Include").iterdir())[-1].name
    includes = [vc / "include"] + [kits / "Include" / version / part for part in ("ucrt", "shared", "um")]
    libs = [vc / "lib/x64"] + [kits / "Lib" / version / part / "x64" for part in ("ucrt", "um")]
    return vc / "bin/Hostx64/x64/cl.exe", includes, libs


class NativeAuthTests(unittest.TestCase):
    def test_real_guard_blocks_unauthorized_http_and_websocket_before_business(self):
        compiler, includes, libs = native_toolchain()
        build = ROOT / ".pio/host-auth-tests"
        build.mkdir(parents=True, exist_ok=True)
        # Compile and execute the real error callback bodies without pulling in the display/OTA hardware tree.
        server = (ROOT / "src/network/KnomiWebServer.h").read_text(encoding="utf-8")
        bodies = []
        for name in ("registerNotFound", "methodNotAllowedStaticCode", "notFoundStaticCode"):
            match = re.search(r"^  (?:void|static esp_err_t) " + name + r"\([^\n]*\) \{.*?^  \}", server, re.M | re.S)
            self.assertIsNotNone(match, name)
            bodies.append(match.group(0))
        (build / "KnomiErrorCallbacks.inc").write_text(
            "class KnomiErrorCallbacks { public: httpd_handle_t server = nullptr;\n" + "\n".join(bodies) + "\n};\n", encoding="utf-8")
        exe = build / "http_auth_tests.exe"
        cmd = [str(compiler), "/nologo", "/EHsc", "/std:c++20", "/MD", "/W4", "/D_CRT_SECURE_NO_WARNINGS",
            *[f"/I{p}" for p in includes], f"/I{ROOT / 'tests/http_auth/shims'}",
            f"/I{ROOT / 'src/network'}", f"/I{ROOT / '.pio/libdeps/knomiv1-4mb/ArduinoJson/src'}", f"/I{build}", str(ROOT / "tests/http_auth/test_auth.cpp"),
            str(ROOT / "src/network/HttpBasicAuth.cpp"), f"/Fe:{exe}", "/link",
            *[f"/LIBPATH:{p}" for p in libs], "bcrypt.lib", "crypt32.lib"]
        compiled = subprocess.run(cmd, cwd=build, capture_output=True, text=True)
        self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
        result = subprocess.run([str(exe)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS:", result.stdout)
        print(result.stdout.strip())

    def test_http_routes_have_one_common_registration_point(self):
        sources = list((ROOT / "src").rglob("*.h")) + list((ROOT / "src").rglob("*.cpp"))
        registrations = [p.relative_to(ROOT).as_posix() for p in sources
                         if "httpd_register_uri_handler(" in p.read_text(encoding="utf-8")]
        self.assertEqual(registrations, ["src/network/pages/AbstractPage.h"])


class BuildHookTests(unittest.TestCase):
    def test_build_does_not_install_optional_upload_dependencies(self):
        class BuildOnlyEnv:
            def Execute(self, command):
                raise AssertionError("Build unexpectedly attempted an optional uploader installation")
            def GetProjectOption(self, key, default):
                return default
        with patch.dict(sys.modules, {"requests_toolbelt": None, "tqdm": None}):
            runpy.run_path(str(ROOT / "scripts/platformio_upload.py"),
                          init_globals={"Import": lambda _: None, "env": BuildOnlyEnv()})



if __name__ == "__main__":
    unittest.main()
