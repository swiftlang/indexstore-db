#!/usr/bin/env python3

import argparse
import os
import platform
import shutil
import subprocess
import sys
from typing import Dict, List

# -----------------------------------------------------------------------------
# General utilities


def fatal_error(message):
    print(message, file=sys.stderr)
    raise SystemExit(1)


def escapeCmdArg(arg: str) -> str:
    if '"' in arg or " " in arg:
        return '"%s"' % arg.replace('"', '\\"')
    else:
        return arg


def print_cmd(cmd: List[str], additional_env: Dict[str, str]) -> None:
    env_str = " ".join([f"{key}={escapeCmdArg(str(value))}" for (key, value) in additional_env.items()])
    command_str = " ".join([escapeCmdArg(str(arg)) for arg in cmd])
    print(f"{env_str} {command_str}")


def env_with_additional_env(additional_env: Dict[str, str]) -> Dict[str, str]:
    env = dict(os.environ)
    for (key, value) in additional_env.items():
        env[key] = str(value)
    return env


def check_call(cmd: List[str], additional_env: Dict[str, str] = {}, verbose: bool = False) -> None:
    if verbose:
        print_cmd(cmd=cmd, additional_env=additional_env)

    subprocess.check_call(cmd, env=env_with_additional_env(additional_env), stderr=subprocess.STDOUT)


def check_output(cmd: List[str], additional_env: Dict[str, str] = {}, capture_stderr: bool = True, verbose: bool = False) -> str:
    if verbose:
        print_cmd(cmd=cmd, additional_env=additional_env)
    if capture_stderr:
        stderr = subprocess.STDOUT
    else:
        stderr = subprocess.DEVNULL
    return subprocess.check_output(cmd, env=env_with_additional_env(additional_env), stderr=stderr, encoding='utf-8')

# -----------------------------------------------------------------------------
# SwiftPM wrappers


def swiftpm_bin_path(swift_exec: str, swiftpm_args: List[str], additional_env: Dict[str, str], verbose: bool = False) -> str:
    """
    Return the path of the directory that contains the binaries produced by this package.
    """
    cmd = [swift_exec, 'build', '--show-bin-path'] + swiftpm_args
    return check_output(cmd, additional_env=additional_env, capture_stderr=False, verbose=verbose).strip()

# -----------------------------------------------------------------------------
# Build indexstore-db


def get_swiftpm_options(args: argparse.Namespace) -> List[str]:
    """
    Return the arguments that should be passed to a 'swift build' or 'swift test' invocation
    """
    swiftpm_args = [
        '--package-path', args.package_path,
        '--scratch-path', args.build_path,
        '--configuration', args.configuration,
    ]

    if args.multiroot_data_file:
        swiftpm_args += ['--multiroot-data-file', args.multiroot_data_file]

    if args.verbose:
        swiftpm_args += ['--verbose']

    if args.sanitize:
        for san in args.sanitize:
            swiftpm_args += ['--sanitize=%s' % san]

    if platform.system() != 'Darwin':
        swiftpm_args += [
            # Dispatch headers
            '-Xcxx', '-I', '-Xcxx',
            os.path.join(args.toolchain, 'lib', 'swift'),
            # For <Block.h>
            '-Xcxx', '-I', '-Xcxx',
            os.path.join(args.toolchain, 'lib', 'swift', 'Block'),
        ]

    return swiftpm_args


def get_swiftpm_environment_variables(args: argparse.Namespace) -> Dict[str, str]:
    """
    Return the environment variables that should be used for a 'swift build' or
    'swift test' invocation.
    """

    env = {
        # Set the toolchain used in tests at runtime
        'INDEXSTOREDB_TOOLCHAIN_BIN_PATH': args.toolchain,
        'SWIFTCI_USE_LOCAL_DEPS': '1',
    }

    if args.ninja_bin:
        env['NINJA_BIN'] = args.ninja_bin

    if args.sanitize and 'address' in args.sanitize:
        # Workaround reports in Foundation: https://bugs.swift.org/browse/SR-12551
        env['ASAN_OPTIONS'] = 'detect_leaks=false'
    if args.sanitize and 'undefined' in args.sanitize:
        supp = os.path.join(args.package_path, 'Utilities', 'ubsan_supressions.supp')
        env['UBSAN_OPTIONS'] = 'halt_on_error=true,suppressions=%s' % supp
    if args.sanitize and 'thread' in args.sanitize:
        env['INDEXSTOREDB_ENABLED_THREAD_SANITIZER'] = '1'

    return env


def build(swift_exec: str, args: argparse.Namespace) -> None:
    """
    Build one product in the package
    """
    swiftpm_args = get_swiftpm_options(args)
    additional_env = get_swiftpm_environment_variables(args)
    cmd = [swift_exec, 'build', '--product', 'IndexStoreDBPackageTests'] + swiftpm_args
    check_call(cmd, additional_env=additional_env, verbose=args.verbose)


def run_tests(swift_exec: str, args: argparse.Namespace) -> None:
    """
    Run all tests in the indexstore-db package
    """
    swiftpm_args = get_swiftpm_options(args)
    additional_env = get_swiftpm_environment_variables(args)

    bin_path = swiftpm_bin_path(swift_exec=swift_exec, swiftpm_args=swiftpm_args, additional_env=additional_env, verbose=args.verbose)
    tests = os.path.join(bin_path, 'isdb-tests')
    print('Cleaning ' + tests)
    shutil.rmtree(tests, ignore_errors=True)

    cmd = [swift_exec, 'test', '--parallel', '--test-product', 'IndexStoreDBPackageTests'] + swiftpm_args
    check_call(cmd, additional_env=additional_env, verbose=args.verbose)


def handle_invocation(swift_exec: str, args: argparse.Namespace) -> None:
    """
    Depending on the action in 'args', build the package or run tests.
    """
    if args.action == 'build':
        build(swift_exec, args)
    elif args.action == 'test':
        run_tests(swift_exec, args)
    else:
        fatal_error(f"unknown action '{args.action}'")

# -----------------------------------------------------------------------------
# Argument parsing


def parse_args() -> argparse.Namespace:
    def add_common_args(parser):
        parser.add_argument('--package-path', metavar='PATH', help='directory of the package to build', default='.')
        parser.add_argument('--toolchain', required=True, metavar='PATH', help='build using the toolchain at PATH')
        parser.add_argument('--ninja-bin', metavar='PATH', help='ninja binary to use for testing')
        parser.add_argument('--build-path', metavar='PATH', default='.build', help='build in the given path')
        parser.add_argument('--configuration', '-c', default='debug', help='build using configuration (release|debug)')
        parser.add_argument('--sanitize', action='append', help='build using the given sanitizer(s) (address|thread|undefined)')
        parser.add_argument('--sanitize-all', action='store_true', help='build using every available sanitizer in sub-directories of build path')
        parser.add_argument('--verbose', '-v', action='store_true', help='enable verbose output')
        parser.add_argument('--multiroot-data-file', help='path to an Xcode workspace to create a unified build of all of Swift\'s SwiftPM projects')

    parser = argparse.ArgumentParser(description='Build along with the Swift build-script.')

    if sys.version_info >= (3, 7, 0):
        subparsers = parser.add_subparsers(title='subcommands', dest='action', required=True, metavar='action')
    else:
        subparsers = parser.add_subparsers(title='subcommands', dest='action', metavar='action')
    build_parser = subparsers.add_parser('build', help='build the package')
    add_common_args(build_parser)

    test_parser = subparsers.add_parser('test', help='test the package')
    add_common_args(test_parser)

    args = parser.parse_args(sys.argv[1:])

    # Canonicalize paths
    args.package_path = os.path.abspath(args.package_path)
    args.build_path = os.path.abspath(args.build_path)
    args.toolchain = os.path.abspath(args.toolchain)

    if args.sanitize and args.sanitize_all:
        fatal_error('cannot combine --sanitize with --sanitize-all')

    return args


def main() -> None:
    args = parse_args()

    if args.toolchain:
        swift_exec = os.path.join(args.toolchain, 'bin', 'swift')
    else:
        swift_exec = 'swift'

    handle_invocation(swift_exec, args)

    if args.sanitize_all:
        base = args.build_path

        print('=== %s indexstore-db with asan ===' % args.action)
        args.sanitize = ['address']
        args.build_path = os.path.join(base, 'test-asan')
        handle_invocation(swift_exec, args)

        print('=== %s indexstore-db with tsan ===' % args.action)
        args.sanitize = ['thread']
        args.build_path = os.path.join(base, 'test-tsan')
        handle_invocation(swift_exec, args)

        # Linux ubsan disabled: https://bugs.swift.org/browse/SR-12550
        #
        # ubsan disabled on macOS because building indexstore-db using a new, just-built clang with ubsan enabled but
        # with an Xcode 15.1 SDK causes C++ exception to not be catchable with `try {} catch {}` blocks and
        # indexstore-db relies on exception catching, possibly related to
        # https://stackoverflow.com/questions/51673343/clang-with-libc-exceptions.
        # Re-enable when Xcode on CI nodes has been updated to Xcode 16 (rdar://139647872)
        if platform.system() != 'Linux' and False:
            print('=== %s indexstore-db with ubsan ===' % args.action)
            args.sanitize = ['undefined']
            args.build_path = os.path.join(base, 'test-ubsan')
            handle_invocation(swift_exec, args)


if __name__ == '__main__':
    main()
