"""Run a localhost paired-input diagnostic in two real native game instances."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import secrets
import shutil
import struct
import subprocess
import time
import replay_session as replay


def canonical_digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).digest()


def scenario_coverage(modules, required):
    """Ordered subsequence: repeated milestones require separate module entries."""
    matched = 0
    for module in modules:
        if matched < len(required) and module.casefold() == required[matched].casefold():
            matched += 1
    return {'passed': matched == len(required), 'matched': matched,
            'required': list(required), 'remaining': list(required[matched:])}


def peer_result(directory, exit_code, samples):
    info, error = None, None
    try:
        info = replay.trace_info(directory / 'input.pbtrace')
    except (OSError, ValueError) as failure:
        error = str(failure)
    text = (directory / 'game.log').read_text(errors='replace')
    stopped = re.search(r'Net diagnostic: sample (\d+) stopped \((\d+)\)', text)
    acknowledged = 'Net diagnostic: capabilities acknowledged mask=0' in text
    completed = bool(exit_code == 0 and info and info['samples'] == samples
                     and acknowledged
                     and f'Input script: complete ({samples} samples)' in text
                     and f'Net diagnostic: closed after {samples} paired samples' in text)
    return {'exit_code': exit_code, 'completed': completed, 'trace': info, 'trace_error': error,
            'capabilities_acknowledged': acknowledged,
            'modules': re.findall(r'objdll> (\S+) prolog start', text),
            'stop': {'sample': int(stopped[1]), 'reason': int(stopped[2])} if stopped else None}


def first_trace_difference(left, right):
    fields = ((0, 8, 'sequence'), (8, 12, 'frand'), (12, 16, 'rand8'),
              (16, 20, 'rumble_capability'), (20, 64, 'controllers'), (64, 68, 'selected_state'))
    with Path(left).open('rb') as a, Path(right).open('rb') as b:
        if a.read(16) != b.read(16):
            return {'field': 'header'}
        sample = 0
        while True:
            x, y = a.read(68), b.read(68)
            if x != y:
                if len(x) != 68 or len(y) != 68:
                    return {'sample': sample, 'field': 'length'}
                for start, end, field in fields:
                    if x[start:end] != y[start:end]:
                        return {'sample': sample, 'field': field,
                                'host': x[start:end].hex(), 'join': y[start:end].hex()}
            if not x:
                return None
            sample += 1


def save_failed_session(root, processes, samples, error):
    report = {'completed': False, 'samples': samples, 'error': str(error),
              'error_type': type(error).__name__, 'verification_completed': False, 'peers': {}}
    for role, process in processes.items():
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        try:
            report['peers'][role] = peer_result(root / role, process.returncode, samples)
        except (OSError, ValueError) as read_error:
            report['peers'][role] = {'exit_code': process.returncode, 'completed': False,
                                     'evidence_error': str(read_error)}
    replay.save_json(root / 'result.json', report)
    return report


def run(args):
    exe, disc, root = args.exe.resolve(), args.disc.resolve(), args.session.resolve()
    profile, script = args.profile.resolve(), args.script.resolve()
    if not exe.is_file() or not disc.is_file():
        raise ValueError('Explicit native executable and disc required')
    if root.is_relative_to(profile):
        raise ValueError('Session must be outside the source profile')
    script_info = replay.trace_info(script, input_only=True)
    disconnect_at = args.disconnect_after_samples
    if disconnect_at is not None and not 1 <= disconnect_at < script_info['samples']:
        raise ValueError('Disconnect sample must be positive and before script completion')
    root.mkdir(parents=True, exist_ok=False)
    baseline = root / 'baseline'
    profile_info = replay.copy_profile(profile, baseline)
    config_json = json.loads((baseline / 'config.json').read_text(encoding='utf-8'))
    if not config_json.get('backend.skipPreLaunchUI'):
        raise ValueError('Prepare a separate profile with backend.skipPreLaunchUI=true first')
    manifest = {'version': 1, 'executable': exe.name, 'runtime': replay.fingerprint(exe.parent, runtime=True),
                'disc': {'bytes': disc.stat().st_size, 'sha256': replay.digest(disc)}, 'profile': profile_info,
                'script': script_info, 'seed': args.seed, 'fast': True,
                'disconnect_after_samples': disconnect_at,
                'required_modules': args.require_module}
    replay.save_json(root / 'manifest.json', manifest)
    stimulus = root / 'stimulus.pbtrace'
    with script.open('rb') as source, stimulus.open('xb') as target:
        shutil.copyfileobj(source, target)
    if replay.trace_info(stimulus, input_only=True) != script_info:
        raise ValueError('Script changed during copy')
    # Diagnostic host and join must use this exact session/configuration, including
    # the stimulus and runtime mode. These digests are computed, not placeholders.
    hello = (b'PBHI\x01\0\0\0' + struct.pack('<II', 1, args.seed)
             + canonical_digest(manifest['runtime']) + bytes.fromhex(manifest['disc']['sha256'])
             + canonical_digest(profile_info)
             + canonical_digest({'config': config_json, 'script': script_info, 'fast': True, 'rumble': False}))
    config = root / 'net-config.bin'
    with config.open('xb') as stream:
        stream.write(secrets.token_bytes(16) + hello)
    processes, logs = {}, {}
    deadline = time.monotonic() + args.timeout
    injected = None
    host_stopped = None
    try:
        for role in ('host', 'join'):
            run_dir = root / role
            run_dir.mkdir()
            replay.copy_profile(baseline, run_dir / 'profile')
        def launch(role, port):
            run_dir = root / role
            logs[role] = (run_dir / 'game.log').open('xb')
            command = [str(exe), '--keyboard', '--user-dir', str(run_dir / 'profile'),
                       '--disc', str(disc), '--record-input', str(run_dir / 'input.pbtrace'),
                       '--input-script', str(stimulus), '--simulation-seed', str(args.seed), '--trace-fast',
                       '--net-diagnostic-listen' if role == 'host' else '--net-diagnostic-connect', str(port),
                       '--net-diagnostic-config', str(config)]
            processes[role] = subprocess.Popen(command, cwd=exe.parent, stdout=logs[role], stderr=subprocess.STDOUT)
        launch('host', 0)
        while True:
            text = (root / 'host' / 'game.log').read_text(errors='replace')
            match = re.search(r'Net diagnostic: PORT (\d+)', text)
            if match:
                launch('join', int(match[1])); break
            if processes['host'].poll() is not None:
                raise RuntimeError('Host stopped before listening; inspect host/game.log')
            if time.monotonic() >= deadline:
                raise TimeoutError('Native host startup deadline exceeded')
            time.sleep(0.1)
        print(f'Paired game diagnostic started: {root}', flush=True)
        while any(process.poll() is None for process in processes.values()):
            now = time.monotonic()
            trace = root / 'join' / 'input.pbtrace'
            if (disconnect_at is not None and injected is None and trace.exists()
                    and trace.stat().st_size >= 16 + 68 * disconnect_at
                    and all(process.poll() is None for process in processes.values())):
                injected = now
                processes['join'].kill()  # Only the child launched by this runner.
                print('Injected join-process disconnect', flush=True)
            if injected is not None and host_stopped is None and processes['host'].poll() is not None:
                host_stopped = now
            if time.monotonic() >= deadline:
                raise TimeoutError('Paired game diagnostic deadline exceeded')
            time.sleep(0.1)
        if injected is not None and host_stopped is None:
            host_stopped = time.monotonic()
        replay.verify_environment(manifest, exe, disc, baseline)
        if replay.trace_info(stimulus, input_only=True) != script_info:
            raise ValueError('Input script changed during run')
        report = {'completed': True, 'samples': script_info['samples'], 'peers': {}}
        for role, process in processes.items():
            report['peers'][role] = peer_result(root / role, process.returncode, script_info['samples'])
            report['completed'] &= report['peers'][role]['completed']
            report['peers'][role]['scenario'] = scenario_coverage(
                report['peers'][role]['modules'], args.require_module)
            report['completed'] &= report['peers'][role]['scenario']['passed']
        report['completed'] &= report['peers']['host']['trace'] == report['peers']['join']['trace']
        report['first_trace_difference'] = (first_trace_difference(root / 'host' / 'input.pbtrace',
                                                                  root / 'join' / 'input.pbtrace')
                                            if all(p['trace'] for p in report['peers'].values()) else None)
        host = report['peers']['host']
        elapsed = host_stopped - injected if injected is not None else None
        # WaitResult::Disconnected=2; TransportError=5 also covers connection reset.
        expected_disconnect = bool(injected is not None and elapsed <= 20
                                   and host['exit_code'] == 2 and host['stop']
                                   and host['stop']['reason'] in (2, 5)
                                   and host['stop']['sample'] < script_info['samples']
                                   and all(p['scenario']['passed'] for p in report['peers'].values())
                                   and all(p['capabilities_acknowledged'] for p in report['peers'].values())
                                   and not host['completed'])
        report['disconnect_test'] = {'requested': disconnect_at is not None,
                                     'injected': injected is not None,
                                     'host_shutdown_seconds': elapsed,
                                     'passed': expected_disconnect}
        replay.save_json(root / 'result.json', report)
        print(json.dumps(report, indent=2), flush=True)
        return 0 if (expected_disconnect if disconnect_at is not None else report['completed']) else 2
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        report = save_failed_session(root, processes, script_info['samples'], error)
        print(json.dumps(report, indent=2), flush=True)
        return 2
    finally:
        for process in processes.values():
            if process.poll() is None:
                process.kill(); process.wait(timeout=10)
        for log in logs.values():
            log.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'disc', 'profile', 'script', 'session'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--timeout', type=int, default=240)
    parser.add_argument('--require-module', action='append', default=[],
                        help='Required module milestone; repeat to specify an ordered sequence')
    parser.add_argument('--disconnect-after-samples', type=int,
                        help='Kill the owned join process after this many flushed samples')
    args = parser.parse_args()
    if not 0 <= args.seed <= 0xffffffff or not 1 <= args.timeout <= 7200:
        parser.error('Invalid seed or timeout')
    try:
        raise SystemExit(run(args))
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(2, f'Paired session failed: {error}\n')
