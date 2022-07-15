#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

// Simple debugging utility which dumps data from /dev/ithc.

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s DEVICE\n", argv[0]);
		return 1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	//lseek(fd, 0, SEEK_END);

	uint8_t buf[0x10000];
	uint32_t next = 0;
	while (1) {
		ssize_t r = read(fd, buf, sizeof buf);
		if (r < 0) {
			perror("read");
			return 1;
		}
		ssize_t i = 0;
		while (i < r) {
			uint32_t *hdr = (void *)(buf + i);
			// api header
			uint32_t hdr_size = hdr[0] & 0xff;
			uint32_t msg_num = hdr[1];
			uint32_t size = hdr[2];
			if (msg_num != next) fprintf(stderr, "missed messages %"PRIu32" to %"PRIu32"\n", next, msg_num-1);
			next = msg_num + 1;
			printf("%10"PRIu32" %5"PRIu32":", msg_num, size);
			i += hdr_size;
			hdr += hdr_size / 4;
			// device header
			uint32_t nhdr = 16;
			if (size < nhdr * 4) nhdr = size / 4;
			for (uint32_t j = 0; j < nhdr; j++) printf(j == 1 ? " %4"PRIu32 : " %"PRIu32, hdr[j]);
			i += 4 * nhdr;
			size -= 4 * nhdr;
			// device data
			printf(" |");
			uint32_t nprint = 32;
			if (size < nprint) nprint = size;
			for (uint32_t j = 0; j < nprint; j++) printf(" %02x", buf[i + j]);
			if (nprint < size) printf(" + %"PRIu32" B", size - nprint);
			i += size;
			printf("\n");
		}
		if (i != r) {
			fprintf(stderr, "position mismatch\n");
			return 1;
		}
	}

	return 0;
}
