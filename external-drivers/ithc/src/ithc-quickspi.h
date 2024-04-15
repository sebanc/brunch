/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */

struct ithc_acpi_config {
	bool has_config: 1;
	bool has_input_report_header_address: 1;
	bool has_input_report_body_address: 1;
	bool has_output_report_body_address: 1;
	bool has_read_opcode: 1;
	bool has_write_opcode: 1;
	bool has_read_mode: 1;
	bool has_write_mode: 1;
	bool has_spi_frequency: 1;
	bool has_limit_packet_size: 1;
	bool has_tx_delay: 1;
	bool has_active_ltr: 1;
	bool has_idle_ltr: 1;
	u32 input_report_header_address;
	u32 input_report_body_address;
	u32 output_report_body_address;
	u8 read_opcode;
	u8 write_opcode;
	u8 read_mode;
	u8 write_mode;
	u32 spi_frequency;
	u32 limit_packet_size;
	u32 tx_delay; // us/10 // TODO use?
	u32 active_ltr; // ns/1024
	u32 idle_ltr; // ns/1024
};

int ithc_read_acpi_config(struct ithc *ithc, struct ithc_acpi_config *cfg);
void ithc_print_acpi_config(struct ithc *ithc, const struct ithc_acpi_config *cfg);

int ithc_quickspi_init(struct ithc *ithc, const struct ithc_acpi_config *cfg);
void ithc_quickspi_exit(struct ithc *ithc);
int ithc_quickspi_decode_rx(struct ithc *ithc, const void *src, size_t len, struct ithc_data *dest);
ssize_t ithc_quickspi_encode_tx(struct ithc *ithc, const struct ithc_data *src, void *dest,
	size_t maxlen);

