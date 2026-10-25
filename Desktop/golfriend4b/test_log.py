# test_log.py — 驗證本機檔案 log 寫在 tmpfs 並能正常輪替
# 執行：python3 -m unittest test_log

import logging
import os
import tempfile
import unittest

import log


class FileHandlerTest(unittest.TestCase):
    def _fresh_logger(self, name):
        logger = logging.getLogger(name)
        logger.handlers.clear()
        logger.setLevel(logging.DEBUG)
        logger.propagate = False
        return logger

    def test_default_log_path_is_under_tmp(self):
        # Arrange / Act
        path = log.LOG_FILE

        # Assert
        self.assertTrue(path.startswith("/tmp/"), path)
        self.assertEqual(os.path.basename(path), "golfTest.log")

    def test_rotates_when_max_bytes_exceeded(self):
        # Arrange
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "golfTest.log")
            max_bytes = 2000
            handler = log.create_file_handler(path, max_bytes=max_bytes, backup_count=2)
            logger = self._fresh_logger("rotate-test")
            logger.addHandler(handler)

            # Act：寫入遠超過 max_bytes 的量
            for i in range(300):
                logger.info("line %03d %s", i, "x" * 40, extra={"tag": log.TAG_INFO})
            handler.close()

            # Assert
            self.assertTrue(os.path.exists(path))
            self.assertTrue(os.path.exists(path + ".1"), "應產生第一個備份檔")
            self.assertTrue(os.path.exists(path + ".2"), "應產生第二個備份檔")
            self.assertFalse(os.path.exists(path + ".3"), "backup_count=2 不應有第三個")
            self.assertLessEqual(os.path.getsize(path), max_bytes)

    def test_file_handler_formats_tag(self):
        # Arrange
        with tempfile.TemporaryDirectory() as tmpdir:
            path = os.path.join(tmpdir, "golfTest.log")
            handler = log.create_file_handler(path)
            logger = self._fresh_logger("format-test")
            logger.addHandler(handler)

            # Act
            logger.info("掃碼取球", extra={"tag": log.TAG_OPERATION})
            handler.close()

            # Assert
            with open(path, encoding="utf-8") as f:
                content = f.read()
            self.assertIn("INFO", content)
            self.assertIn("[operation]", content)
            self.assertIn("掃碼取球", content)

    def test_unwritable_path_returns_none(self):
        # Arrange / Act
        handler = log.create_file_handler("/nonexistent-dir-xyz/golfTest.log")

        # Assert
        self.assertIsNone(handler)


if __name__ == "__main__":
    unittest.main()
