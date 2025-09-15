
// The ID check is from the Circuit Python code that was downloaded from:
// https://github.com/raspberrypi/pico-sdk-rp2350/issues/12#issuecomment-2055274428
//
// in the file supervisor/port.c -- the function setup_psram()

// NOTE: The PSRAM IC used by the Circuit Python example is:
//	  https://www.adafruit.com/product/4677
//
// The PSRAM IC used by the SparkFun Pro Micro is: apmemory APS6404L-3SQR-ZR
// https://www.mouser.com/ProductDetail/AP-Memory/APS6404L-3SQR-ZR?qs=IS%252B4QmGtzzpDOdsCIglviw%3D%3D
//
// The datasheets from both these IC's are almost identical (word for word), with the first being ESP32 branded
//

static size_t __no_inline_not_in_flash_func(setup_psram)(uint psram_cs_pin)
{
	gpio_set_function(psram_cs_pin, GPIO_FUNC_XIP_CS1);

	size_t psram_size = 0;
	uint32_t intr_stash = save_and_disable_interrupts();

	// Try and read the PSRAM ID via direct_csr.
	qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
	// Need to poll for the cooldown on the last XIP transfer to expire
	// (via direct-mode BUSY flag) before it is safe to perform the first
	// direct-mode operation
	while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
	{
	}

	// Exit out of QMI in case we've inited already
	qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
	// Transmit as quad.
	qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS | QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB | 0xf5;
	while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
	{
	}
	(void)qmi_hw->direct_rx;
	qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);

	// Read the id
	qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
	uint8_t kgd = 0;
	uint8_t eid = 0;
	for (size_t i = 0; i < 7; i++)
	{
		if (i == 0)
		{
			qmi_hw->direct_tx = 0x9f;
		}
		else
		{
			qmi_hw->direct_tx = 0xff;
		}
		while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS) == 0)
		{
		}
		while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
		{
		}
		if (i == 5)
		{
			kgd = qmi_hw->direct_rx;
		}
		else if (i == 6)
		{
			eid = qmi_hw->direct_rx;
		}
		else
		{
			(void)qmi_hw->direct_rx;
		}
	}
	// Disable direct csr.
	qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

	if (kgd != 0x5D)
	{
		printf("Invalid PSRAM ID: %x\n", kgd);
		restore_interrupts(intr_stash);
		return psram_size;
	}

	// Enable quad mode.
	qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;
	// Need to poll for the cooldown on the last XIP transfer to expire
	// (via direct-mode BUSY flag) before it is safe to perform the first
	// direct-mode operation
	while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
	{
	}

	// RESETEN, RESET and quad enable
	for (uint8_t i = 0; i < 3; i++)
	{
		qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
		if (i == 0)
		{
			qmi_hw->direct_tx = 0x66;
		}
		else if (i == 1)
		{
			qmi_hw->direct_tx = 0x99;
		}
		else
		{
			qmi_hw->direct_tx = 0x35;
		}
		while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
		{
		}
		qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);
		for (size_t j = 0; j < 20; j++)
		{
			asm("nop");
		}
		(void)qmi_hw->direct_rx;
	}
	// Disable direct csr.
	qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

	qmi_hw->m[1].timing =
		QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB | // Break between pages.
		3 << QMI_M1_TIMING_SELECT_HOLD_LSB | // Delay releasing CS for 3 extra system cycles.
		1 << QMI_M1_TIMING_COOLDOWN_LSB | 1 << QMI_M1_TIMING_RXDELAY_LSB |
		16 << QMI_M1_TIMING_MAX_SELECT_LSB |  // In units of 64 system clock cycles. PSRAM says 8us max. 8 / 0.00752 /64
											  // = 16.62
		7 << QMI_M1_TIMING_MIN_DESELECT_LSB | // In units of system clock cycles. PSRAM says 50ns.50 / 7.52 = 6.64
		2 << QMI_M1_TIMING_CLKDIV_LSB;
	qmi_hw->m[1].rfmt = (QMI_M1_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_PREFIX_WIDTH_LSB |
						 QMI_M1_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_RFMT_ADDR_WIDTH_LSB |
						 QMI_M1_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_SUFFIX_WIDTH_LSB |
						 QMI_M1_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_RFMT_DUMMY_WIDTH_LSB |
						 QMI_M1_RFMT_DUMMY_LEN_VALUE_24 << QMI_M1_RFMT_DUMMY_LEN_LSB |
						 QMI_M1_RFMT_DATA_WIDTH_VALUE_Q << QMI_M1_RFMT_DATA_WIDTH_LSB |
						 QMI_M1_RFMT_PREFIX_LEN_VALUE_8 << QMI_M1_RFMT_PREFIX_LEN_LSB |
						 QMI_M1_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_RFMT_SUFFIX_LEN_LSB);
	qmi_hw->m[1].rcmd = 0xeb << QMI_M1_RCMD_PREFIX_LSB | 0 << QMI_M1_RCMD_SUFFIX_LSB;
	qmi_hw->m[1].wfmt = (QMI_M1_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_PREFIX_WIDTH_LSB |
						 QMI_M1_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_WFMT_ADDR_WIDTH_LSB |
						 QMI_M1_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_SUFFIX_WIDTH_LSB |
						 QMI_M1_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_WFMT_DUMMY_WIDTH_LSB |
						 QMI_M1_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M1_WFMT_DUMMY_LEN_LSB |
						 QMI_M1_WFMT_DATA_WIDTH_VALUE_Q << QMI_M1_WFMT_DATA_WIDTH_LSB |
						 QMI_M1_WFMT_PREFIX_LEN_VALUE_8 << QMI_M1_WFMT_PREFIX_LEN_LSB |
						 QMI_M1_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_WFMT_SUFFIX_LEN_LSB);
	qmi_hw->m[1].wcmd = 0x38 << QMI_M1_WCMD_PREFIX_LSB | 0 << QMI_M1_WCMD_SUFFIX_LSB;

	psram_size = 1024 * 1024; // 1 MiB
	uint8_t size_id = eid >> 5;
	if (eid == 0x26 || size_id == 2)
	{
		psram_size *= 8;
	}
	else if (size_id == 0)
	{
		psram_size *= 2;
	}
	else if (size_id == 1)
	{
		psram_size *= 4;
	}

	// Mark that we can write to PSRAM.
	xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;
	restore_interrupts(intr_stash);
	// printf("PSRAM ID: %x %x\n", kgd, eid);
	return psram_size;
}