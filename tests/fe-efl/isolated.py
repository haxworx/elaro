"""Run a lifecycle check without reading or writing the user's Elaro settings."""
import os
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as home:
    result = subprocess.run(sys.argv[1:], env={**os.environ, 'HOME': home, 'XDG_CONFIG_HOME': os.path.join(home, '.config')}, timeout=12)
    sys.exit(result.returncode)
