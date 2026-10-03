#!/usr/bin/env python3
"""Build standalone library tests and preserve review artifacts, including failures."""
import argparse
import datetime
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=['debug', 'release', 'sanitizer', 'all'], default='all')
    parser.add_argument('--label', choices=['server', 'client', 'packet', 'cpp', 'regression', 'stress'])
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--build-root', type=Path)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    source = Path(__file__).resolve().parent
    root = (args.build_root or source.parent / 'build' / 'library-tests').resolve()
    profiles = ['debug', 'release', 'sanitizer'] if args.profile == 'all' else [args.profile]
    failed = False
    summaries = []
    for profile in profiles:
        build = root / profile
        report = build / 'reports' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        report.mkdir(parents=True, exist_ok=True)
        configuration = 'Release' if profile == 'release' else 'Debug'
        sanitizer = 'address' if profile == 'sanitizer' else 'none'
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        summary = {'profile': profile, 'report': str(report), 'steps': {}, 'tests': []}
        steps = [
            ('configure', ['cmake', '-S', str(source), '-B', str(build),
                           f'-DCMAKE_BUILD_TYPE={configuration}', f'-DUIPC_SANITIZER={sanitizer}']),
            ('build', ['cmake', '--build', str(build), '--parallel', str(args.jobs)]),
            ('test', ['ctest', '--test-dir', str(build), '--output-on-failure',
                      '--parallel', str(args.jobs), '--output-junit', str(report / 'junit.xml')]),
        ]
        if args.label:
            steps[-1][1].extend(['-L', f'^{args.label}$'])
        for name, command in steps:
            print(f'[{profile}] {name}: {" ".join(command)}', flush=True)
            with (report / f'{name}.log').open('w') as log:
                try:
                    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                               text=True, env=env)
                    for line in process.stdout:
                        log.write(line)
                        print(line, end='', flush=True)
                    result = process.wait()
                except OSError as error:
                    log.write(str(error) + '\n')
                    print(error, file=sys.stderr)
                    result = 127
            summary['steps'][name] = result
            failed |= result != 0
            if result != 0 and name != 'test':
                break
        ctest_log = build / 'Testing' / 'Temporary' / 'LastTest.log'
        if 'test' in summary['steps'] and ctest_log.exists():
            shutil.copyfile(ctest_log, report / 'cases.log')
        junit = report / 'junit.xml'
        if junit.exists():
            for test in ET.parse(junit).iter('testcase'):
                summary['tests'].append({'name': test.get('name'), 'seconds': test.get('time'),
                                         'status': 'failed' if test.find('failure') is not None else
                                         'skipped' if test.find('skipped') is not None else 'passed'})
        (report / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        summaries.append(summary)
        print(f'[{profile}] reports: {report}', flush=True)
    (root / 'latest.json').write_text(json.dumps(summaries, indent=2) + '\n')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
