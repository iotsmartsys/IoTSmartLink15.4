# SPDX-License-Identifier: CC0-1.0

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.utils import idf_parametrize


@pytest.mark.esp32c3
@idf_parametrize('target', ['esp32c3'], indirect=['target'])
def test_device_registry_on_physical_esp32c3(dut: IdfDut) -> None:
    dut.expect_exact('13 Tests 0 Failures 0 Ignored')
    dut.expect_exact('OK')
