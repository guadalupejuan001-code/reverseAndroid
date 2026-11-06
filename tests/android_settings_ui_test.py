#!/usr/bin/env python3
"""Validate the exact shell request and reject echoed, timed-out or failed UI replies."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

REPO=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(REPO/'scripts'))
spec=importlib.util.spec_from_file_location('settings_ui',REPO/'scripts/run-android-settings-ui.py')
ui=importlib.util.module_from_spec(spec);spec.loader.exec_module(ui)
body=ui.validate_nested_console_command(ui.ui_command())

REPLY='''Starting: Intent { act=android.settings.SETTINGS }
Status: ok
Activity: com.android.settings/.Settings
REVERSE_UI_SETTINGS_DUMP_BEGIN
    mResumedActivity: ActivityRecord{aa u0 com.android.settings/.Settings t3}
REVERSE_UI_SETTINGS_DUMP_END
REVERSE_UI_SETTINGS_LAUNCH_RESULT RC_0
'''


class SettingsUIRequest(unittest.TestCase):
    def test_echo_never_satisfies_ui_response(self):
        self.assertFalse(ui.ui_reply(ui.ui_command())['success'])
        self.assertLessEqual(len(ui.ui_command().encode('ascii')),4096)

    def test_requires_launch_status_component_and_scoped_resumed_activity(self):
        self.assertTrue(ui.ui_reply(REPLY)['success'])
        for missing in ['Status: ok','Activity: com.android.settings/.Settings',
                        'REVERSE_UI_SETTINGS_DUMP_BEGIN','REVERSE_UI_SETTINGS_DUMP_END',
                        '    mResumedActivity: ActivityRecord{aa u0 com.android.settings/.Settings t3}']:
            with self.subTest(missing=missing):
                self.assertFalse(ui.ui_reply(REPLY.replace(missing,''))['success'])
        self.assertFalse(ui.ui_reply(REPLY.replace('Status: ok','Status: timeout'))['success'])
        self.assertFalse(ui.ui_reply(REPLY.replace('mResumedActivity: ActivityRecord{aa u0','mResumedActivity: ActivityRecord{aa u1'))['success'])

    def test_failure_overrides_success_and_incomplete_response_waits(self):
        observed=ui.ui_reply(REPLY+'REVERSE_UI_SETTINGS_LAUNCH_RESULT RC_73\n')
        self.assertTrue(observed['failed']);self.assertFalse(observed['success'])
        self.assertFalse(ui.ui_reply(REPLY.replace('REVERSE_UI_SETTINGS_LAUNCH_RESULT RC_0',''))['success'])

    def test_real_shell_command_sequence_and_failure_short_circuit(self):
        stub='''cmd() {
          case "$*" in
            'input keyevent KEYCODE_WAKEUP') [ "$STUB_FAIL" != input ] || return 1;;
            'window dismiss-keyguard') [ "$STUB_FAIL" != window ] || return 1;;
            'activity start-activity -W --user 0 -a android.settings.SETTINGS')
              [ "$STUB_FAIL" != activity ] || return 1
              printf 'Status: ok\\nActivity: com.android.settings/.Settings\\n';;
            *) return 99;;
          esac
        }
        dumpsys() {
          [ "$*" = 'activity activities' ] || return 98
          [ "$STUB_FAIL" != dump ] || return 97
          printf 'mResumedActivity: ActivityRecord{aa u0 com.android.settings/.Settings t3}\\n'
        }
        '''
        for shell in ['sh','bash']:
            for failure,status in [('',0),('input',71),('window',72),('activity',73),('dump',97)]:
                with self.subTest(shell=shell,failure=failure):
                    result=subprocess.run([shell,'-c',stub+'\nSTUB_FAIL='+failure+'\n'+body],capture_output=True,text=True)
                    self.assertEqual(result.returncode,status,result.stderr)
                    if failure in ['input','window','activity']:
                        self.assertNotIn('UI_SETTINGS_DUMP_BEGIN',result.stdout)
                    if not failure:
                        self.assertTrue(ui.ui_reply(result.stdout+'\nREVERSE_UI_SETTINGS_LAUNCH_RESULT RC_0\n')['success'])


if __name__=='__main__':unittest.main()
