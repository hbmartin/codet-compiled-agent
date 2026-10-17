import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from hermetic import environment


class HermeticEnvironment(unittest.TestCase):
    def test_inherited_state_home_is_replaced_without_mutating_parent(self):
        with patch.dict(os.environ, {'CODET_HOME': '/sentinel/developer-state'}):
            result = environment(pathlib.Path('/private/test-home'))
            self.assertEqual(result['HOME'], '/private/test-home')
            self.assertEqual(result['CODET_HOME'], '/private/test-home/.codet')
            self.assertEqual(os.environ['CODET_HOME'], '/sentinel/developer-state')
            override = dict(result, CODET_HOME='/private/specific-fixture')
            self.assertEqual(override['CODET_HOME'], '/private/specific-fixture')

    def test_wrapper_child_gets_a_private_state_home(self):
        with tempfile.TemporaryDirectory() as root:
            sentinel = pathlib.Path(root) / 'developer-state'
            script = 'import os,json; print(json.dumps([os.environ["HOME"],os.environ["CODET_HOME"]]))'
            result = subprocess.run(
                [sys.executable, str(pathlib.Path(__file__).with_name('hermetic.py')),
                 sys.executable, '-c', script],
                env=dict(os.environ, CODET_HOME=str(sentinel)), check=True,
                capture_output=True, text=True, timeout=10)
            home, state = json.loads(result.stdout)
            self.assertEqual(pathlib.Path(state), pathlib.Path(home) / '.codet')
            self.assertNotEqual(state, str(sentinel))
            self.assertFalse(sentinel.exists())
            self.assertFalse(pathlib.Path(home).exists())

    def test_private_home_keeps_controlled_shell_and_clears_credentials(self):
        with patch.dict(os.environ, {'OPENAI_API_KEY': 'fake', 'HTTP_PROXY': 'fake',
                                    'BASH_ENV': '/sentinel/startup'}):
            result = environment('/private/test-home')
            self.assertEqual(result['SHELL'], '/bin/sh')
            self.assertEqual(result['NO_PROXY'], '127.0.0.1,localhost')
            for key in ('OPENAI_API_KEY', 'HTTP_PROXY', 'BASH_ENV'):
                self.assertNotIn(key, result)
