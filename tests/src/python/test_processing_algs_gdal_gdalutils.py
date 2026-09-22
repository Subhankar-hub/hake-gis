"""QGIS Unit tests for GdalUtils class

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""

__author__ = "Stefanos Natsis"
__date__ = "03/07/2024"
__copyright__ = "Copyright 2024, The QGIS Project"


import os
import tempfile
import unittest
from shutil import rmtree
from unittest import mock

from processing.algs.gdal.GdalUtils import GdalConnectionDetails, GdalUtils
from qgis.core import (
    QgsApplication,
    QgsAuthMethodConfig,
    QgsDataSourceUri,
    QgsRasterLayer,
)
from qgis.PyQt.QtCore import QByteArray
from qgis.testing import QgisTestCase, start_app

QGIS_AUTH_DB_DIR_PATH = tempfile.mkdtemp()
os.environ["QGIS_AUTH_DB_DIR_PATH"] = QGIS_AUTH_DB_DIR_PATH

start_app()


class TestProcessingAlgsGdalGdalUtils(QgisTestCase):
    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""
        rmtree(QGIS_AUTH_DB_DIR_PATH)
        del os.environ["QGIS_AUTH_DB_DIR_PATH"]
        super().tearDownClass()

    def test_decode_process_output_utf8_en_dash(self):
        """UTF-8 en dash in GDAL process output decodes correctly."""
        raw = "Hake GeoDesk – Desktop GIS".encode("utf-8")
        self.assertEqual(
            GdalUtils._decodeProcessOutput(raw),
            "Hake GeoDesk – Desktop GIS",
        )

    def test_decode_process_output_cp1252_en_dash(self):
        """
        Windows ACP (CP1252) en dash byte 0x96 must not raise UnicodeDecodeError.
        This matches the Polygonize crash when the product path contains U+2013.
        """
        raw = b"C:\\Program Files\\Hake GeoDesk \x96 Desktop GIS"
        with mock.patch.object(
            GdalUtils, "_windows_ansi_encoding", return_value="cp1252"
        ):
            decoded = GdalUtils._decodeProcessOutput(raw)
        self.assertEqual(decoded, "C:\\Program Files\\Hake GeoDesk – Desktop GIS")
        self.assertNotIn("\ufffd", decoded)

    def test_decode_process_output_qbytearray_cp1252(self):
        """QByteArray from QgsBlockingProcess handlers uses the same decoder."""
        ba = QByteArray(b"Creating output \x96 done.\n")
        with mock.patch.object(
            GdalUtils, "_windows_ansi_encoding", return_value="cp1252"
        ):
            decoded = GdalUtils._decodeProcessOutput(ba)
        self.assertEqual(decoded, "Creating output – done.\n")

    def test_decode_process_output_ascii_and_empty(self):
        self.assertEqual(GdalUtils._decodeProcessOutput(b""), "")
        self.assertEqual(GdalUtils._decodeProcessOutput(None), "")
        self.assertEqual(
            GdalUtils._decodeProcessOutput(b"Creating output of format GPKG.\n"),
            "Creating output of format GPKG.\n",
        )

    def test_decode_process_output_mixed_ascii_non_ascii(self):
        raw = "line1\ncréation – ok\nline3\n".encode("utf-8")
        self.assertEqual(
            GdalUtils._decodeProcessOutput(raw),
            "line1\ncréation – ok\nline3\n",
        )

    def test_decode_process_output_invalid_bytes_never_raises(self):
        """Final fallback must return a string; callbacks must not raise into SIP."""
        # 0xFF is invalid in UTF-8; with ACP disabled this hits replace fallback
        with mock.patch.object(GdalUtils, "_windows_ansi_encoding", return_value=None):
            with mock.patch(
                "processing.algs.gdal.GdalUtils.locale.getpreferredencoding",
                return_value="utf-8",
            ):
                decoded = GdalUtils._decodeProcessOutput(b"bad\xffbyte")
        self.assertIsInstance(decoded, str)
        self.assertTrue(decoded.startswith("bad"))
        self.assertIn("\ufffd", decoded)

    def test_gdal_connection_details_from_layer_postgresraster(self):
        """
        Test GdalUtils.gdal_connection_details_from_layer
        """

        rl = QgsRasterLayer(
            "dbname='mydb' host=localhost port=5432 user='asdf' password='42'"
            " sslmode=disable table=some_table schema=some_schema column=rast sql=pk = 2",
            "pg_layer",
            "postgresraster",
        )

        self.assertEqual(rl.providerType(), "postgresraster")

        connection_details = GdalUtils.gdal_connection_details_from_layer(rl)
        s = connection_details.connection_string

        self.assertTrue(s.lower().startswith("pg:"))
        self.assertTrue("schema='some_schema'" in s)
        self.assertTrue("password='42'" in s)
        self.assertTrue("column='rast'" in s)
        self.assertTrue("mode=1" in s)
        self.assertTrue("where='pk = 2'" in s)
        self.assertEqual(connection_details.format, '"PostGISRaster"')

        # test different uri:
        # - authcfg is expanded
        # - column is parsed
        # - where is skipped
        authm = QgsApplication.authManager()
        self.assertTrue(authm.setMasterPassword("masterpassword", True))
        config = QgsAuthMethodConfig()
        config.setName("Basic")
        config.setMethod("Basic")
        config.setConfig("username", "asdf")
        config.setConfig("password", "42")
        self.assertTrue(authm.storeAuthenticationConfig(config, True))

        rl = QgsRasterLayer(
            f"dbname='mydb' host=localhost port=5432 authcfg={config.id()}"
            f' sslmode=disable table="some_schema"."some_table" (rast)',
            "pg_layer",
            "postgresraster",
        )

        self.assertEqual(rl.providerType(), "postgresraster")

        connection_details = GdalUtils.gdal_connection_details_from_layer(rl)
        s = connection_details.connection_string

        self.assertTrue(s.lower().startswith("pg:"))
        self.assertTrue("schema='some_schema'" in s)
        self.assertTrue("user='asdf'" in s)
        self.assertTrue("password='42'" in s)
        self.assertTrue("column='rast'" in s)
        self.assertTrue("mode=1" in s)
        self.assertFalse("where=" in s)

    def test_ogrLayerName(self):

        self.assertEqual(GdalUtils.ogrLayerName(' table="some_table"'), "some_table")
        self.assertEqual(
            GdalUtils.ogrLayerName(' table="some_schema"."some_table"'),
            "some_schema.some_table",
        )
        self.assertEqual(
            GdalUtils.ogrLayerName(' table="some_schema"."some_table"(geom)'),
            "some_schema.some_table",
        )
        self.assertEqual(
            GdalUtils.ogrLayerName('"base_path|layername=some_table|other=field"'),
            "some_table",
        )


if __name__ == "__main__":
    unittest.main()
