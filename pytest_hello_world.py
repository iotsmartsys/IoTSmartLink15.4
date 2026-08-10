# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
import hashlib
import logging
from typing import Callable

import pytest
from pytest_embedded_idf.app import IdfApp
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


def verify_elf_sha256_embedding(app: IdfApp, sha256_reported: str) -> None:
    sha256 = hashlib.sha256()
    with open(app.elf_file, 'rb') as f:
        sha256.update(f.read())
    sha256_expected = sha256.hexdigest()

    logging.info(f'ELF file SHA256: {sha256_expected}')
    logging.info(f'ELF file SHA256 (reported by the app): {sha256_reported}')

    if not sha256_expected.startswith(sha256_reported):
        raise ValueError('ELF file SHA256 mismatch')


# The root diagnostic project is bound to ESP32-H2 (TESTEXEC-008). The generic
# `supported_targets`/`preview_targets` lists inherited from the ESP-IDF template
# are not this project's policy and never were: they claimed chips without an
# IEEE 802.15.4 radio.
@pytest.mark.generic
@pytest.mark.esp32h2
@idf_parametrize('target', ['esp32h2'], indirect=['target'])
def test_hello_world(app: IdfApp, dut: IdfDut, log_minimum_free_heap_size: Callable[..., None]) -> None:
    sha256_reported = dut.expect(r'ELF file SHA256:\s+([a-f0-9]+)').group(1).decode('utf-8')
    verify_elf_sha256_embedding(app, sha256_reported)
    dut.expect('Hello world!')
    log_minimum_free_heap_size()


# The two cases below run host-native. The ESP-IDF `linux` target does define
# CONFIG_IDF_TARGET="linux", but TESTEXEC-003 admits it as an explicit host
# exception to the physical allowlist: it builds no firmware, needs no board and
# proves nothing about IEEE 802.15.4 compatibility.
@pytest.mark.host_test
@idf_parametrize('target', ['linux'], indirect=['target'])
def test_hello_world_linux(dut: IdfDut) -> None:
    dut.expect('Hello world!')


@pytest.mark.host_test
@pytest.mark.macos_shell
@idf_parametrize('target', ['linux'], indirect=['target'])
def test_hello_world_macos(dut: IdfDut) -> None:
    dut.expect('Hello world!')
