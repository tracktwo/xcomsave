#include "minilzo.h"
#include "xcom.h"

XComSave XSReadSave(const unsigned char *ptr, long len)
{
	XComSave save;
	const unsigned char *p = ptr + 1024;
	FILE *outFile = fopen("output.dat", "wb");
	int chunkCount = 0;
	do
	{
		++chunkCount;
		unsigned long uncompressedSize = *(reinterpret_cast<const unsigned long*>(p + 4));
		unsigned long compressedSize = *(reinterpret_cast<const unsigned long*>(p + 8));
		printf("Next chunk size: %d (%d) bytes at offset 0x%08x\n", uncompressedSize, compressedSize, (p - ptr));

		unsigned char *data = new unsigned char[uncompressedSize];
		unsigned long decompSize = uncompressedSize;
		if (lzo1x_decompress_safe(p + 24, compressedSize, data, &decompSize, nullptr) != LZO_E_OK) {
			fprintf(stderr, "LZO decompress failed\n");
		}

		fwrite(data, 1, decompSize, outFile);
		if (uncompressedSize != decompSize) {
			fprintf(stderr, "Expected to decompress %d bytes but only got %d\n", uncompressedSize, decompSize);
		}
		p += compressedSize;
		p += 24;
	} while ((p - ptr) < len);
	fclose(outFile);
	printf("%d chunks found\n", chunkCount);

	return save;
}