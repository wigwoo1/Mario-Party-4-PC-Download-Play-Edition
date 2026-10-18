"""Run two native loopback probe processes; uses synthetic data, never game assets."""
import argparse
import subprocess
from pathlib import Path


def run(exe, scenario):
    host = subprocess.Popen([str(exe), 'listen', '0', scenario], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    client = None
    try:
        # Native accept timeout bounds this read even if the child fails to start.
        line = host.stdout.readline().strip()
        if not line.startswith('PORT '):
            raise RuntimeError(f'Host did not report a loopback port: {line}')
        port = int(line.split()[1])
        client = subprocess.Popen([str(exe), 'connect', str(port), scenario],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        client_out, client_err = client.communicate(timeout=90)
        host_out, host_err = host.communicate(timeout=15)
        if scenario == 'normal':
            assert host.returncode == client.returncode == 0, (host_err, client_err)
            assert host_out.strip() == client_out.strip(), (host_out, client_out)
            assert host_out.startswith('CAPABILITIES agreed=3758096384\n'), host_out
            assert 'PASS frames=360 hash=' in host_out, host_out
        else:
            assert host.returncode == client.returncode == 2, (host_err, client_err)
            expected = {'mismatch': 'compatibility mismatch',
                        'bad-capability': 'remote capability rejected',
                        'bad-ack': 'remote acknowledgement rejected',
                        'disconnect': 'connection closed or socket timeout'}[scenario]
            assert expected in host_err, host_err
            assert 'PASS' not in host_out + client_out
            if scenario in ('bad-capability', 'bad-ack'):
                assert 'CAPABILITIES agreed=' not in host_out
        print(f'{scenario}: passed', flush=True)
    finally:
        for process in (client, host):
            if process is not None and process.poll() is None:
                process.kill()
                process.communicate()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    for scenario in ('normal', 'mismatch', 'disconnect', 'bad-capability', 'bad-ack'):
        run(args.exe.resolve(), scenario)
