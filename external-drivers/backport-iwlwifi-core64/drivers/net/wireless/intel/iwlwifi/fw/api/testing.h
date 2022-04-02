/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2017 Intel Deutschland GmbH
 */
#ifndef __iwl_fw_api_testing_h__
#define __iwl_fw_api_testing_h__

#define FIPS_KEY_LEN_128	16
#define FIPS_KEY_LEN_256	32
#define FIPS_MAX_KEY_LEN	FIPS_KEY_LEN_256

#define FIPS_CCM_NONCE_LEN	13
#define FIPS_GCM_NONCE_LEN	12
#define FIPS_MAX_NONCE_LEN	FIPS_CCM_NONCE_LEN

#define FIPS_MAX_AAD_LEN	30

/**
 * enum iwl_fips_test_vector_flags - flags for FIPS test vector
 * @IWL_FIPS_TEST_VECTOR_FLAGS_AES: use AES algorithm
 * @IWL_FIPS_TEST_VECTOR_FLAGS_CCM: use CCM algorithm
 * @IWL_FIPS_TEST_VECTOR_FLAGS_GCM: use GCM algorithm
 * @IWL_FIPS_TEST_VECTOR_FLAGS_ENC: if set, the requested operation is
 *	encryption. Otherwise, the requested operation is decryption.
 * @IWL_FIPS_TEST_VECTOR_FLAGS_KEY_256: if set, the test vector uses a
 *	256 bit key. Otherwise 128 bit key is used.
 */
enum iwl_fips_test_vector_flags {
	IWL_FIPS_TEST_VECTOR_FLAGS_AES = BIT(0),
	IWL_FIPS_TEST_VECTOR_FLAGS_CCM = BIT(1),
	IWL_FIPS_TEST_VECTOR_FLAGS_GCM = BIT(2),
	IWL_FIPS_TEST_VECTOR_FLAGS_ENC = BIT(3),
	IWL_FIPS_TEST_VECTOR_FLAGS_KEY_256 = BIT(5),
};

/**
 * struct iwl_fips_test_cmd - FIPS test command for AES/CCM/GCM tests
 * @flags: test vector flags from &enum iwl_fips_test_vector_flags.
 * @payload_len: the length of the @payload field in bytes.
 * @aad_len: the length of the @aad field in bytes.
 * @key: the key used for encryption/decryption. In case a 128-bit key is used,
 *	pad with zero.
 * @aad: AAD. If the AAD is shorter than the buffer, pad with zero. Only valid
 *	for CCM/GCM tests.
 * @reserved: for alignment.
 * @nonce: nonce. Only valid for CCM/GCM tests.
 * @reserved2: for alignment.
 * @payload: the plaintext to encrypt or the cipher text to decrypt + MIC.
 */
struct iwl_fips_test_cmd {
	__le32 flags;
	__le32 payload_len;
	__le32 aad_len;
	u8 key[FIPS_MAX_KEY_LEN];
	u8 aad[FIPS_MAX_AAD_LEN];
	__le16 reserved;
	u8 nonce[FIPS_MAX_NONCE_LEN];
	u8 reserved2[3];
	u8 payload[0];
} __packed; /* AES_SEC_TEST_VECTOR_HDR_API_S_VER_1 */

/**
 * enum iwl_fips_test_status - FIPS test result status
 * @IWL_FIPS_TEST_STATUS_FAILURE: the requested operation failed.
 * @IWL_FIPS_TEST_STATUS_SUCCESS: the requested operation was completed
 *	successfully. The result buffer is valid.
 */
enum iwl_fips_test_status {
	IWL_FIPS_TEST_STATUS_FAILURE,
	IWL_FIPS_TEST_STATUS_SUCCESS,
};

/**
 * struct iwl_fips_test_resp - FIPS test response
 * @len: the length of the result in bytes.
 * @payload: @len bytes of response followed by status code (u32, one of
 *	&enum iwl_fips_test_status).
 */
struct iwl_fips_test_resp {
	__le32 len;
	u8 payload[0];
} __packed; /* AES_SEC_TEST_VECTOR_RESP_API_S_VER_1 */

#endif
