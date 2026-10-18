"""Local replay sessions with pristine profiles and explicit runtime/disc fingerprints."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import uuid


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def files(root):
    root = Path(root)
    def reject_link(path):
        if path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & 0x400:
            raise ValueError(f'Links/junctions are not supported: {path}')
    reject_link(root)
    for directory, dirs, names in os.walk(root, followlinks=False):
        for name in dirs + names:
            reject_link(Path(directory) / name)
        for name in sorted(names):
            yield Path(directory) / name


def fingerprint(root, runtime=False):
    root = Path(root)
    if not root.is_dir():
        raise ValueError(f'Not a directory: {root}')
    result = {}
    for path in files(root):
        relative = path.relative_to(root)
        if runtime and path.suffix.lower() not in ('.exe', '.dll') and relative.parts[0] != 'res':
            continue
        result[relative.as_posix()] = {'bytes': path.stat().st_size, 'sha256': digest(path)}
    return result


def copy_profile(source, destination):
    source, destination = Path(source), Path(destination)
    if destination.resolve().is_relative_to(source.resolve()):
        raise ValueError('Profile destination must be outside its source')
    expected = fingerprint(source)
    destination.mkdir(parents=True, exist_ok=False)
    for relative in expected:
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / relative, target)
    if fingerprint(destination) != expected or fingerprint(source) != expected:
        raise ValueError('Profile changed during copying')
    return expected


def trace_info(path, input_only=False):
    path = Path(path)
    size = path.stat().st_size
    with path.open('rb') as stream:
        header = stream.read(16)
    versions = (b'PBTRACE\x02', b'PBTRACE\x03') if input_only else (b'PBTRACE\x03',)
    if len(header) != 16 or header[:8] not in versions or header[12:] != b'\0' * 4:
        raise ValueError('Invalid version-3 trace header')
    if size <= 16 or (size - 16) % 68:
        raise ValueError('Empty or truncated trace')
    if (size - 16) // 68 > 10000000:
        raise ValueError('Trace exceeds sample limit')
    return {'samples': (size - 16) // 68, 'seed': struct.unpack_from('<I', header, 8)[0],
            'sha256': digest(path)}


def verify_environment(manifest, exe, disc, baseline):
    if manifest.get('version') != 1:
        raise ValueError('Unsupported replay session manifest')
    if manifest['runtime'] != fingerprint(exe.parent, runtime=True) or manifest['executable'] != exe.name:
        raise ValueError('Runtime files differ from the recording')
    if manifest['disc'] != {'bytes': disc.stat().st_size, 'sha256': digest(disc)}:
        raise ValueError('Disc differs from the recording')
    if manifest['profile'] != fingerprint(baseline):
        raise ValueError('Baseline profile differs from the recording')


def save_json(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)


def first_divergence(log_path):
    pattern = re.compile(r'Input trace divergence: sample=(\d+) field=(sequence|frand|rand8|selected_state) expected=(\d+) actual=(\d+)')
    with Path(log_path).open(encoding='utf-8', errors='replace') as stream:
        for line in stream:
            match = pattern.fullmatch(line.strip())
            if match:
                sample, field, expected, actual = match.groups()
                return {'sample': int(sample), 'field': field,
                        'expected': int(expected), 'actual': int(actual)}
    return None


def run(args):
    exe, disc, session = args.exe.resolve(), args.disc.resolve(), args.session.resolve()
    if not exe.is_file() or not disc.is_file():
        raise ValueError('Explicit executable and disc files are required')
    baseline = session / 'baseline'
    trace = session / 'input.pbtrace'
    stimulus = session / 'stimulus.pbtrace'
    if args.mode == 'record':
        if args.profile is None:
            raise ValueError('Recording requires --profile (close the game before copying it)')
        if session.is_relative_to(args.profile.resolve()):
            raise ValueError('Session must be outside the source profile')
        stimulus_info = trace_info(args.input_script, input_only=True) if args.input_script else None
        if args.input_script and args.samples is not None:
            raise ValueError('Input scripts finish at EOF; omit --samples')
        session.mkdir(parents=True, exist_ok=False)
        profile_hash = copy_profile(args.profile, baseline)
        manifest = {'version': 1, 'executable': exe.name, 'fast': args.fast,
                    'runtime': fingerprint(exe.parent, runtime=True),
                    'disc': {'bytes': disc.stat().st_size, 'sha256': digest(disc)},
                    'profile': profile_hash}
        if args.input_script:
            with args.input_script.open('rb') as source, stimulus.open('xb') as target:
                shutil.copyfileobj(source, target)
            if trace_info(stimulus, input_only=True) != stimulus_info:
                raise ValueError('Input script changed during copying')
            manifest['input_script'] = stimulus_info
        save_json(session / 'manifest.json', manifest)
        run_dir = session / 'record'
    else:
        if args.profile is not None or args.seed is not None or args.samples is not None or args.input_script is not None:
            raise ValueError('Replay uses the recording; omit --profile, --seed, --samples and --input-script')
        manifest = json.loads((session / 'manifest.json').read_text(encoding='utf-8'))
        recorded = json.loads((session / 'record' / 'result.json').read_text(encoding='utf-8'))
        if recorded['exit_code'] != 0 or not recorded['completed'] or recorded['trace'] != trace_info(trace):
            raise ValueError('Recording failed or trace contents changed')
        verify_environment(manifest, exe, disc, baseline)
        run_dir = session / 'replays' / uuid.uuid4().hex
    run_dir.mkdir(parents=True, exist_ok=False)
    profile = run_dir / 'profile'
    copy_profile(baseline, profile)
    command = [str(exe), '--keyboard', '--user-dir', str(profile), '--disc', str(disc)]
    if manifest.get('fast', False):
        command += ['--trace-fast']
    if args.mode == 'record':
        command += ['--record-input', str(trace), '--simulation-seed', str(args.seed if args.seed is not None else 42)]
        if args.samples is not None:
            command += ['--record-samples', str(args.samples)]
        if args.input_script:
            command += ['--input-script', str(stimulus)]
    else:
        command += ['--replay-input', str(trace)]
    print(f'{args.mode.title()} profile: {profile}', flush=True)
    print('Dismiss the native Play menu. Close normally to finish recording; playback exits on completion.', flush=True)
    log_path = run_dir / 'game.log'
    with log_path.open('xb') as log:
        result = subprocess.run(command, cwd=exe.parent, stdout=log, stderr=subprocess.STDOUT,
                                env={**os.environ, 'SDL_AUDIO_DRIVER': 'dummy'}, check=False)
    info = trace_info(trace)
    completed = result.returncode == 0
    if args.mode == 'record' and args.samples is not None:
        completed = completed and info['samples'] == args.samples
    if args.mode == 'record' and args.input_script:
        completed = (completed and info['samples'] == stimulus_info['samples']
                     and f'Input script: complete ({info["samples"]} samples)' in log_path.read_text(errors='replace'))
        if trace_info(stimulus, input_only=True) != stimulus_info:
            raise ValueError('Input script changed during recording')
    if args.mode == 'replay':
        completed = completed and f'replay complete ({info["samples"]} samples)' in log_path.read_text(errors='replace')
    # Detect changes during the run as well as before it.
    verify_environment(manifest, exe, disc, baseline)
    report = {'exit_code': result.returncode, 'completed': completed, 'trace': info,
              'profile_after': fingerprint(profile), 'divergence': first_divergence(log_path)}
    save_json(run_dir / 'result.json', report)
    print(f'{args.mode.title()} {"completed" if completed else "failed or interrupted"}: {run_dir}', flush=True)
    return 0 if completed else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['record', 'replay'])
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--disc', required=True, type=Path)
    parser.add_argument('--session', required=True, type=Path)
    parser.add_argument('--profile', type=Path)
    parser.add_argument('--seed', type=int)
    parser.add_argument('--samples', type=int, help='Automatically finish recording after this many input samples')
    parser.add_argument('--input-script', type=Path, help='Use old trace controller inputs to record NEW checkpoints; not replay validation')
    parser.add_argument('--fast', action='store_true', help='Disable frame limiting for diagnostic recording; replay inherits this setting')
    args = parser.parse_args()
    if args.mode == 'replay' and args.fast:
        parser.error('replay inherits fast mode from its session; omit --fast')
    if args.seed is not None and not 0 <= args.seed <= 0xffffffff:
        parser.error('seed must be 0..4294967295')
    if args.samples is not None and not 1 <= args.samples <= 10000000:
        parser.error('samples must be 1..10000000')
    try:
        return run(args)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f'Replay session error: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
