#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "xmergesort.h"

#ifndef __NR_xmergesort
#error xmergesort system call not defined
#endif

int main(int argc, char *argv[])
{
	int rc;
	int c;
	struct arguments *args = malloc(sizeof(struct arguments));
	// For the options we are storing them in a single integer variable using bit manipulation
	unsigned int flags = 0;
	
	//printf("User program started %d\n", c);
	while((c = getopt(argc,argv,"auitd"))!=-1){
		switch(c){
			// output all records, even if uniquely
			case 'a':
				flags = flags | 1; 
				break;
			// output sorted records uniquely even if there are duplicates
			case 'u':
				flags = flags | 2;
				break;
			// compare records case-insensitive (case sensitive by default)
			case 'i':
				flags = flags | 4;
				break;
			// if any input file is found NOT to be sorted, stop and return an error (EINVAL)
			case 't':
				flags = flags | 8;
				break;
			case 'd':
				flags = flags | 16;
				break;
		}
	}

	// printf("Flags is :%d\n", flags);
	//return error if no flags are entered
	if(flags==0){
		printf("You must enter atleast one flag \n");
		exit(-1);
	}
	// Read the input and output files
	char *infile1 = NULL;
	char *infile2 = NULL;
	char *outfile = NULL;
	
	if(argv[optind]!=NULL)
		infile1 = argv[optind++];
	else {
		printf("Need 2 input files and 1 output file"); 
		exit(-1);
	}
	if(argv[optind]!=NULL)
		infile2 = argv[optind++];
	else {
		printf("Need 2 input files and 1 output file"); 
		exit(-1);
	}

	if(argv[optind]!=NULL)
		outfile = argv[optind];
	else {
		printf("Need 2 input files and 1 output file"); 
		exit(-1);
	}
	args->inputFile1 = infile1;
	args->inputFile2 = infile2;
	args->outputFile = outfile;
	args->flags = flags;

 //  	printf("In File1 :%s\n", infile1);
	// printf("In File2 :%s\n", infile2);
	// printf("Outfile :%s\n", outfile);
	
	// Make the system call	
	rc = syscall(__NR_xmergesort, args);
	if (rc == 0) {
		if(flags & 16){
			printf("Wrote %u line to the output file\n", args->data);
		}
		printf("syscall returned %d\n", rc);
	} else {
		printf("syscall returned %d (errno=%d)\n", rc, errno);
		perror("Error message");
	}

	exit(rc);
}
