#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "matoya.h"


// AESGCM

#define AES_BUF_LEN 1024

static double test_aesgcm_performance(uint32_t n)
{
	uint8_t dummy[AES_BUF_LEN];
	uint8_t dummy2[AES_BUF_LEN];
	uint8_t key[16] = "8kdmanxjke8jds";
	uint8_t iv[12] = "abnmfkedlai";
	uint8_t tag[16];

	MTY_AESGCM *aesgcm = NULL;
	MTY_AESGCMCreate(key, 16, &aesgcm);

	int64_t begin = MTY_Timestamp();
	for (uint32_t x = 0; x < n; x++) {
		dummy[x % 1024] = x & 0xFF;
		MTY_AESGCMEncrypt(aesgcm, dummy, AES_BUF_LEN, iv, 12, tag, 16, dummy2);
		MTY_AESGCMDecrypt(aesgcm, dummy2, AES_BUF_LEN, iv, 12, tag, 16, dummy);
	}

	MTY_AESGCMDestroy(&aesgcm);

	return MTY_TimeDiff(begin, MTY_Timestamp());
}


// Main

static void test_print_header(const char *header)
{
	char *underline = MTY_Strdup(header);
	for (size_t x = 0; x < strlen(underline); x++)
		underline[x] = '-';

	printf("%s\n%s\n", header, underline);
	MTY_Free(underline);
}

int32_t main(int32_t argc, char **argv)
{
	test_print_header("AESGCM");

	uint32_t n = 5000000;
	printf("n=%u, %.2f ms\n", n, test_aesgcm_performance(n));

	return 0;
}
