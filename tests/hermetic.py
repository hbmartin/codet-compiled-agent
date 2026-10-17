"""Run one host test entry point with a private user home and controlled shell."""
import os
import pathlib
import subprocess
import sys
import tempfile

def environment(home):
    result = dict(os.environ)
    for key in list(result):
        if key.lower().endswith('_proxy') or key in {
            'BASH_ENV', 'ENV', 'ZDOTDIR', 'OPENAI_API_KEY', 'ANTHROPIC_API_KEY',
            'GOOGLE_API_KEY', 'GOOGLE_GENERATIVE_AI_API_KEY', 'OLLAMA_API_KEY', 'SEARXNG_URL',
        }:
            result.pop(key)
    result.update(HOME=str(home), CODET_HOME=str(pathlib.Path(home) / '.codet'),
                  SHELL='/bin/sh', NO_PROXY='127.0.0.1,localhost')
    return result

if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='codet-test-home-') as home:
        raise SystemExit(subprocess.call(sys.argv[1:], env=environment(pathlib.Path(home))))
