# test_ballcontrol.py — 驗證 BallControl 初始化時馬達腳位一定被拉低（馬達 OFF）
# 執行：python3 -m unittest test_ballcontrol

import unittest

import BallControl


class _RecordingGPIO:
    """假 GPIO：記錄 setup / output 呼叫，模擬 rpi-lgpio『未給 initial 就沿用目前電位』的行為"""
    BCM = "BCM"
    OUT = "OUT"
    IN = "IN"
    HIGH = 1
    LOW = 0

    def __init__(self, level_before_setup):
        self.level = level_before_setup
        self.setup_calls = []
        self.output_calls = []

    def setmode(self, *a, **k): pass
    def setwarnings(self, *a, **k): pass

    def setup(self, pin, direction, pull_up_down=None, initial=None):
        self.setup_calls.append((pin, direction, initial))
        if direction == self.OUT and initial is not None:
            self.level = initial

    def output(self, pin, value):
        self.output_calls.append((pin, value))
        self.level = value

    def input(self, pin): return 0
    def cleanup(self, *a, **k): pass


class _NullLog:
    def i(self, *a, **k): pass
    def e(self, *a, **k): pass


class BallControlInitTest(unittest.TestCase):
    def setUp(self):
        self._orig_gpio = BallControl.GPIO

    def tearDown(self):
        BallControl.GPIO = self._orig_gpio

    def test_motor_pin_is_low_after_init_even_if_it_read_high(self):
        # Arrange：開機時腳位被外部電路拉成 HIGH（現場 2026-10-25 開機自動出球的情境）
        fake = _RecordingGPIO(level_before_setup=1)
        BallControl.GPIO = fake

        # Act
        BallControl.BallControl(_NullLog())

        # Assert
        self.assertEqual(fake.level, 0, "初始化後馬達腳位必須是 LOW（馬達 OFF）")

    def test_output_pin_setup_passes_initial_low(self):
        # Arrange
        fake = _RecordingGPIO(level_before_setup=1)
        BallControl.GPIO = fake

        # Act
        BallControl.BallControl(_NullLog())

        # Assert
        out_setup = [c for c in fake.setup_calls if c[0] == 18 and c[1] == fake.OUT]
        self.assertEqual(len(out_setup), 1)
        self.assertEqual(out_setup[0][2], fake.LOW, "setup(18, OUT) 必須帶 initial=LOW")

    def test_init_explicitly_drives_motor_off(self):
        # Arrange
        fake = _RecordingGPIO(level_before_setup=1)
        BallControl.GPIO = fake

        # Act
        ctl = BallControl.BallControl(_NullLog())

        # Assert
        self.assertIn((18, 0), fake.output_calls, "初始化要明確 output(18, 0)")
        self.assertEqual(ctl.motorFlag, 0)


if __name__ == "__main__":
    unittest.main()
