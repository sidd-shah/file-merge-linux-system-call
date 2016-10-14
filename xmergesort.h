#include <linux/kernel.h>

struct arguments{
	char *inputFile1;
	char *inputFile2;
	char *outputFile;
	unsigned int flags;
	unsigned int data;
};