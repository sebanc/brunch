/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */

int ithc_legacy_init(struct ithc *ithc);
void ithc_legacy_exit(struct ithc *ithc);
int ithc_legacy_decode_rx(struct ithc *ithc, const void *src, size_t len, struct ithc_data *dest);
ssize_t ithc_legacy_encode_tx(struct ithc *ithc, const struct ithc_data *src, void *dest,
	size_t maxlen);

