// SPDX-License-Identifier: GPL-2.0
/*
 * Container Security Monitor module
 *
 * Copyright (c) 2018 Google, Inc
 */

#include "monitor.h"

#include <linux/string.h>

bool pb_encode_string_field(pb_ostream_t *stream, const pb_field_t *field,
			    void * const *arg)
{
	const uint8_t *str = (const uint8_t *)*arg;

	/* If the string is not set, skip this string. */
	if (!str)
		return true;

	if (!pb_encode_tag_for_field(stream, field))
		return false;

	return pb_encode_string(stream, str, strlen(str));
}

bool pb_decode_string_field(pb_istream_t *stream, const pb_field_t *field,
			    void **arg)
{
	size_t size;
	void *data;

	*arg = NULL;

	size = stream->bytes_left;

	/* Ensure a null-byte at the end */
	if (size + 1 < size)
		return false;

	data = kzalloc(size + 1, GFP_KERNEL);
	if (!data)
		return false;

	if (!pb_read(stream, data, size)) {
		kfree(data);
		return false;
	}

	*arg = data;

	return true;
}

bool pb_encode_string_array(pb_ostream_t *stream, const pb_field_t *field,
			    void * const *arg)
{
	char *strs = (char *)*arg;

	/* If the string array is not set, skip this string array. */
	if (!strs)
		return true;

	do {
		if (!pb_encode_string_field(stream, field,
					    (void * const *) &strs))
			return false;

		strs += strlen(strs) + 1;
	} while (*strs != 0);

	return true;
}

/* Limit the encoded string size and return how many characters were added. */
ssize_t pb_encode_string_field_limit(pb_ostream_t *stream,
				     const pb_field_t *field,
				     void * const *arg, size_t limit)
{
	char *str = (char *)*arg;
	size_t length;

	/* If the string is not set, skip this string. */
	if (!str)
		return 0;

	if (!pb_encode_tag_for_field(stream, field))
		return -EINVAL;

	length = strlen(str);
	if (length > limit)
		length = limit;

	if (!pb_encode_string(stream, (uint8_t *)str, length))
		return -EINVAL;

	return length;
}

bool pb_decode_string_array(pb_istream_t *stream, const pb_field_t *field,
			    void **arg)
{
	size_t needed, used = 0;
	char *data, *strs;

	/* String length, and two null-bytes for the end of the list. */
	needed = stream->bytes_left + 2;
	if (needed < stream->bytes_left)
		return false;

	if (*arg) {
		/* Calculate used space from the current list. */
		strs = (char *)*arg;
		do {
			used += strlen(strs + used) + 1;
		} while (strs[used] != 0);

		if (used + needed < needed)
			return false;
	}

	data = krealloc(*arg, used + needed, GFP_KERNEL);
	if (!data)
		return false;

	/* Will always be freed by the caller */
	*arg = data;

	/* Reset the new part of the buffer. */
	memset(data + used, 0, needed);

	/* Read what's in the stream buffer only. */
	if (!pb_read(stream, data + used, stream->bytes_left))
		return false;

	return true;
}

bool pb_encode_uuid_field(pb_ostream_t *stream, const pb_field_t *field,
			  void * const *arg)
{
	const uint8_t *uuid = (const uint8_t *)*arg;

	/* If the uuid is not set, skip this string. */
	if (!uuid)
		return true;

	if (!pb_encode_tag_for_field(stream, field))
		return false;

	return pb_encode_string(stream, uuid, PROCESS_UUID_SIZE);
}
