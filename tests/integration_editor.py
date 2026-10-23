#!/usr/bin/env python3
"""Smoke-test the interactive editor through a pseudo-terminal."""

import errno
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import tempfile
import termios
import time


def read_available(fd):
	try:
		return os.read(fd, 65536)
	except OSError as error:
		if error.errno == errno.EIO:
			return b""
		raise


def wait_for_output(fd, child, timeout):
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		ready, _, _ = select.select([fd], [], [], 0.05)
		if ready and read_available(fd):
			return True

		finished, _ = os.waitpid(child, os.WNOHANG)
		if finished:
			return False

	return False


def drain_output(fd, duration):
	deadline = time.monotonic() + duration
	output = bytearray()
	while time.monotonic() < deadline:
		ready, _, _ = select.select([fd], [], [], 0.02)
		if ready:
			output.extend(read_available(fd))
	return bytes(output)


def wait_for_exit(fd, child, timeout):
	deadline = time.monotonic() + timeout
	output = bytearray()
	while time.monotonic() < deadline:
		finished, status = os.waitpid(child, os.WNOHANG)
		if finished:
			return os.waitstatus_to_exitcode(status), bytes(output)

		ready, _, _ = select.select([fd], [], [], 0.05)
		if ready:
			output.extend(read_available(fd))

	return None, bytes(output)


def run_editor(binary, home, target):
	child, master = pty.fork()
	if child == 0:
		env = os.environ.copy()
		env.update({
			"HOME": home,
			"TERM": "xterm-256color",
			"LC_ALL": "C.UTF-8",
		})
		os.execve(binary, [binary, target], env)

	fcntl.ioctl(
		master,
		termios.TIOCSWINSZ,
		struct.pack("HHHH", 24, 80, 0, 0),
	)

	try:
		if not wait_for_output(master, child, 5):
			return False, "editor did not render after startup"

		# Let terminal setup and the initial render finish before sending input.
		drain_output(master, 0.2)
		# Type UTF-8 text, save with Ctrl+S, then quit with Ctrl+Q.
		expected = "hello π".encode("utf-8")
		for byte in expected:
			os.write(master, bytes([byte]))
			time.sleep(0.02)
		drain_output(master, 0.2)
		os.write(master, b"\x13")
		save_output = drain_output(master, 0.4)

		with open(target, "rb") as stream:
			contents = stream.read()
		if contents != expected:
			os.kill(child, signal.SIGKILL)
			os.waitpid(child, 0)
			return False, (
				f"Ctrl+S did not save expected content: {contents!r}; "
				f"terminal output: {save_output[-300:]!r}"
			)

		os.write(master, b"\x11")

		exit_code, exit_output = wait_for_exit(master, child, 5)
		if exit_code is None:
			os.kill(child, signal.SIGKILL)
			os.waitpid(child, 0)
			return False, (
				"editor did not quit after Ctrl+Q; "
				f"terminal output: {exit_output[-500:]!r}"
			)

		if exit_code != 0:
			return False, f"editor exited with status {exit_code}"

		with open(target, "rb") as stream:
			contents = stream.read()

		if contents != expected:
			return False, f"saved content mismatch: {contents!r}"

		return True, ""
	finally:
		os.close(master)


def main():
	if len(sys.argv) != 2:
		print("usage: integration_editor.py PATH_TO_EDITOR", file=sys.stderr)
		return 2

	binary = os.path.abspath(sys.argv[1])
	if not os.path.isfile(binary):
		print(f"editor executable not found: {binary}", file=sys.stderr)
		return 2

	with tempfile.TemporaryDirectory(prefix="nytor-pty-test-") as home:
		config_dir = os.path.join(home, ".config", "nytor")
		os.makedirs(config_dir)
		with open(os.path.join(config_dir, "config.ny"), "w", encoding="utf-8"):
			pass

		target = os.path.join(home, "document.txt")
		with open(target, "wb"):
			pass

		passed, reason = run_editor(binary, home, target)
		if not passed:
			print(f"FAIL editor PTY smoke test: {reason}", file=sys.stderr)
			return 1

	print("PASS editor PTY smoke test: type, save, quit")
	return 0


if __name__ == "__main__":
	sys.exit(main())
