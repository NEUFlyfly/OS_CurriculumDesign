#!/usr/bin/env python3
"""Run behavior-preserving CLI smoke checks for FlyflyUFS.

The script runs the executable from isolated temporary directories so generated
`data.img` files never touch the project root or the normal build directory.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import cast


Scenario = tuple[str, str, tuple[str, ...]]


@dataclass(frozen=True)
class Args:
    exe: Path
    timeout: int


SCENARIOS: tuple[Scenario, ...] = (
    (
        "root_file_lifecycle",
        "root\nroot\npwd\nls\ntouch a\nls\nrm a\nexit\n",
        ("/home/root", "-rw-rw-r--", "登出"),
    ),
    (
        "directory_lifecycle",
        "root\nroot\nmkdir d\nls\ncd d\npwd\ncd ..\nrmdir d\nls\nexit\n",
        ("/home/root/d", "drw-rw----", "登出"),
    ),
    (
        "command_errors",
        "root\nroot\nhelp\npwd\nunknowncmd\nchmod\nuseradd\nuserdel\ncat missing\nexit\n",
        ("Command format", "command not found: unknowncmd", "cat missing : No such file"),
    ),
)


def run_process(exe: Path, workdir: Path, input_text: str, timeout: int) -> str:
    env = os.environ.copy()
    env["TERM"] = env.get("TERM", "xterm")
    env["PATH"] = "C:/msys64/ucrt64/bin;" + env.get("PATH", "")
    result = subprocess.run(
        [str(exe)],
        input=input_text,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=workdir,
        env=env,
        timeout=timeout,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"process exited with {result.returncode}\n{result.stdout}")
    return result.stdout


def assert_contains(name: str, output: str, tokens: tuple[str, ...]) -> None:
    missing = [token for token in tokens if token not in output]
    if missing:
        raise AssertionError(f"{name} missing tokens {missing}\n{output}")


def run_scenarios(exe: Path, timeout: int) -> None:
    for name, input_text, tokens in SCENARIOS:
        with tempfile.TemporaryDirectory(prefix=f"flyflyufs-{name}-") as temp:
            output = run_process(exe, Path(temp), input_text, timeout)
            assert_contains(name, output, tokens)
            print(f"PASS {name}")


def run_user_persistence(exe: Path, timeout: int) -> None:
    with tempfile.TemporaryDirectory(prefix="flyflyufs-user-") as temp:
        workdir = Path(temp)
        first = run_process(exe, workdir, "root\nroot\nuseradd bob\nbobpass\nexit\n", timeout)
        assert_contains("useradd", first, ("用户注册成功", "登出"))
        second = run_process(exe, workdir, "bob\nbobpass\npwd\nexit\n", timeout)
        assert_contains("user_login", second, ("bob@", "/home/bob", "登出"))
        print("PASS user_persistence")


def parse_args() -> Args:
    parser = argparse.ArgumentParser(description="Run FlyflyUFS CLI smoke checks")
    _ = parser.add_argument("--exe", type=Path, default=Path("build/FlyflyUFS.exe"))
    _ = parser.add_argument("--timeout", type=int, default=10)
    namespace = parser.parse_args()
    return Args(exe=cast(Path, namespace.exe), timeout=cast(int, namespace.timeout))


def main() -> int:
    args = parse_args()
    exe = args.exe.resolve()
    if not exe.exists():
        print(f"Executable not found: {exe}", file=sys.stderr)
        return 1

    run_scenarios(exe, args.timeout)
    run_user_persistence(exe, args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
