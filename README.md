HW1: xmergesort system call

Approach:

1. Handling Files greater than Page Size

	1. The syscall is using vfs_read to read upto PAGE_SIZE characters from the file
	2. Then the syscall uses the strsep function which returns a null terminated string until the delimiter provided by us which is '\n' in our case
	3. If the last line in the buffer does not have a \n then we store it in a temp variable and then read PAGE_SIZE bytes from the file and do a strcat of temp and strsep on buf. This gives us the line shared between 2 buffers.
	4. The records are written to the output in the same manner. First an output buffer of PAGE_SIZE is maintained and once it is full we write it to the file and clear the buffer for new content.



2. Comparisons

We compare line1 and line2 which are the current pointers to the top of both buffers

	a. If line1 is greater than line2, we add line2 to the output and increment line2
	b. If line2 is greater than line1, we add line1 to the output and increment line1
	c. If line1 and line2 are equal we add both to the output and increment both line1 and line2

3. Flags

	1. a - prints all records including duplicates
	2. u - prints unique records
	3. a,u - throws an error as they are contradictory
	4. i - considers the records as case insensitive (by default case sensitive). works with all other flags
	5. t - throws an error if a record is found to be not sorted. By default we will skip records if found to be unsorted
	6. d - returns the no of lines printed to the output file
	7. <no flags> throws an error

4. Error Handling
	
	1. Input/output files not given - throws an error if less than 2 input files or no output file given EINVAL
	2. Input files are the same - throws an error if input files are the same EINVAL
	3. Input and output file are the same - throws an error if input file is same as the output file EINVAL
	4. Output file exists and is readOnly - syscall will return EACCESS error which permission denied
	5. Syscall fails in between - syscall writes data to a temp file initially and then renames it using vfs_rename to the output file, thus if the syscall fails in between due to some error then the temp file will be deleted and the output file will not be touched.
	6. input/output files are directories - Syscall will return EINVAL
	7. Input/output files are same files on differnet file systems -  we check the superblock on both the file system and check the inode as well
	8. Input file does not exist - returns Error no ENOENT (error no - 2)

5. Miscellaneous
	
	1. Output files will be created with the same permission as the input files
	2. Arguments to the syscall are passed in structure named arguments defined in xmergesort.h, which is added in both the user program and the sys_xmergesort file.
	3. To exchange data between user memory and kernel memory we are using the functions copy_to_user and copy_from_user
	4. If a file contains empty lines at the start, then these would be added to the output because they are in the sorted order and it might mean some valuable information to the user.

EXTRA CREDIT: 
Support for multiple files is given in another branch. The syscall will run as many times as you want, however, after running the syscall for around 20 times, there is some memory leak which results in the corruption of other commands like ls and thus panics the kernel with those commands.

Branch name -> extra_credit

Approach: 

1. The struct arguments has been modified to allow multiple files and so is the user program
2. We consider the 1st 2 files and merge them to output_temp1.txt file.
3. We now consider the output_temp1.txt to be one of our input files for the second iteration.
4. Similarly we merge every file with the output of the previous merges.
5. We perform a VFS unlink on the previous output file once we are done using it for the current iteration.


Test Cases
1. ./xmergesort -i -u -d  empty1.txt 1.txt output.txt  				- Success
2. ./xmergesort -i -u -a -d  empty1.txt 1.txt output.txt 			- EINVAL
3. ./xmergesort -i -u  -d  empty1.txt no_such_file.txt output.txt 	- ENOENT
4. chmod 0400 output.txt
   ./xmergesort -i -u  -d  empty1.txt 1.txt output.txt 				- EACCESS
5. ./xmergesort -i -a -d  empty1.txt ../hw1/ output.txt 			- EINVAL
6. ./xmergesort -i -t -d  empty1.txt 2.txt output.txt(Unsorted)		- EINVAL
7. ./xmergesort -i -a -d  1.txt 1.txt output.txt					- EINVAL
8. ./xmergesort -i -a -d  1.txt 2.txt 1.txt							- EINVAL
