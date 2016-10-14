#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include "xmergesort.h"

asmlinkage extern long (*sysptr)(void * args);

int get_next_line(char* buffer, char * line, char *** bufpointer, 
	char** arrayptr, char ** temp,struct file * inputFile);

int write_to_buffer(char* outputbuf, char* line1, struct file * outputFile);

bool writeLine(bool case_insensitive, bool unique_records, char * line, char * previousLine);

unsigned int outputLines = 0;
const int size = PAGE_SIZE;

bool check_unsorted(bool case_insensitive, bool error_unsorted, char* line, char * previousLine);


asmlinkage long xmergesort(void * args)
{
	/*
	########################
	VALIDATION BEGINS
	########################
	*/
	int ret=0;
	// flags
	bool unique_records;
	bool duplicates;
	bool case_insensitive;
	bool error_unsorted;
	bool return_data;
	
	mm_segment_t old_fs;

	struct file * inputFile1;
	struct file * inputFile2; 
	struct file * outputFile;
	struct file * tempFile;
	char * tempFileName;
	int sizeOfOutputFile;
	int sizeOfInputFile;

	char *buffer1 = kmalloc(size, GFP_KERNEL);
	char *buffer2 = kmalloc(size, GFP_KERNEL);
	char *outputbuf = kmalloc(size, GFP_KERNEL);
	int counter =0;

	char * array1ptr = buffer1;
	char * array2ptr = buffer2;
	char ** buf1pointer = &array1ptr;
	char ** buf2pointer = &array2ptr;

	const char * endLine = "\n";
	char * line1 = NULL;
	char * line2 = NULL;
	char * temp1 = NULL;
	char * temp2 = NULL; 
	char * temp;
	char * previousLine = kcalloc(size,sizeof(char), GFP_KERNEL);

	int count;
	int success;
	int comparator;

	struct inode *old_dir;
	struct inode *new_dir;
	struct dentry *old_dentry;
	struct dentry *new_dentry;

	char * k_infile1;
	char * k_infile2;
	char * k_outfile;
	int k_flags;
	struct arguments* k_args = kmalloc(sizeof(struct arguments), GFP_KERNEL);
	if(k_args == NULL){
		ret = -ENOMEM;
		goto l_return;
	}

	success = copy_from_user(k_args, args, sizeof(struct arguments));

	k_infile1 = k_args->inputFile1;
	k_infile2 = k_args->inputFile2;
	k_outfile = k_args->outputFile;
	k_flags = k_args->flags;

	if(k_infile1==NULL ||k_infile2 ==NULL||k_outfile==NULL){
		ret = -EINVAL;
		goto l_return;
	}

	printk("K infile1 %s", k_infile1);
	printk("K infile2 %s", k_infile2);
	printk("K k_outfile %s", k_outfile);
	
	outputLines = 0;
	unique_records = k_flags & 2;
	duplicates = k_flags & 1;
	case_insensitive = k_flags & 4;
	error_unsorted = k_flags & 8;
	return_data = k_flags & 16;

	printk("unique_records %d\n", unique_records);
	printk("duplicates %d\n", duplicates);
	printk("case_insensitive %d\n", case_insensitive);
	printk("error_unsorted %d\n",error_unsorted);
	printk("return_data %d\n", return_data);

	// duplicates and unique records cannot be used simultaneously
	if((unique_records && duplicates) || k_flags==0){
		ret = -EINVAL;
		goto l_return;
	}

	if(buffer1 == NULL || buffer2 == NULL){
		ret = ENOMEM;
		goto l_return;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	

	tempFileName = "output_temp.txt";
	printk("Temp File name %s", tempFileName);

	inputFile1 = filp_open(k_infile1, O_RDONLY,0);
	if (IS_ERR(inputFile1)) {
		ret = PTR_ERR(inputFile1);
		goto l_return;
	}
	inputFile2 = filp_open(k_infile2, O_RDONLY,0);
	if(IS_ERR(inputFile2)){
		ret = PTR_ERR(inputFile2);
		goto inputFile1;	
	}
	//Check if the input files are the same
	if(inputFile1->f_inode->i_ino == inputFile2->f_inode->i_ino){

	    if(inputFile1->f_inode->i_sb->s_dev == inputFile2->f_inode->i_sb->s_dev) {
        	ret = -EINVAL;
        	goto inputFile2;
		}
	}
	// Use the same permissions as the inputfile
	outputFile = filp_open(k_outfile, O_WRONLY|O_CREAT, file_inode(inputFile1)->i_mode);
	if(IS_ERR(outputFile)){
		ret = PTR_ERR(outputFile);
		goto inputFile2;	
	}
	sizeOfOutputFile = i_size_read(file_inode(outputFile));
	//Check if output file is same as one of the input files
	if(inputFile1->f_inode->i_ino == outputFile->f_inode->i_ino || inputFile2->f_inode->i_ino == outputFile->f_inode->i_ino){
		//Check if they belong to the same file system
        if(inputFile1->f_inode->i_sb->s_dev == outputFile->f_inode->i_sb->s_dev ||
        		inputFile2->f_inode->i_sb->s_dev == outputFile->f_inode->i_sb->s_dev) {
        	ret = -EINVAL;
        	goto outputFile;
        }
	}

	tempFile = filp_open(tempFileName, O_WRONLY|O_CREAT, file_inode(inputFile1)->i_mode);
	if(IS_ERR(tempFile)){
		ret = PTR_ERR(tempFile);
		goto outputFile;
	}
	// return if file given is a directory
	if(S_ISDIR(file_inode(inputFile1)->i_mode) ||S_ISDIR(file_inode(inputFile2)->i_mode) ||S_ISDIR(file_inode(outputFile)->i_mode)){
		ret = -EINVAL;
		goto error;
	}

	

	printk("Validations over");
	/* TODO use kmalloc with the flag as GFP_KERNEL
	Using malloc to allocate memory to the buffers to read from file and
	merge them to output to the output file
	*/
	
	while(counter<size) {
		buffer1[counter] = '\0';
		buffer2[counter] = '\0';
		outputbuf[counter] = '\0';
		counter++;
	}

	success = vfs_read(inputFile1, buffer1,size,&inputFile1->f_pos);
	if(success < 0){
		ret = -EBADF;
		goto error;
	}
	success = vfs_read(inputFile2, buffer2,size,&inputFile2->f_pos);
	if(success < 0){
		ret = -EBADF;
		goto error;
	}
	// printk("success1 %d\nsuccess2 %d\n", success1,success2);

	printk("\nBuffer1 %zd\n buffer2 %zd", strlen(buffer1), strlen(buffer2));
	sizeOfInputFile = i_size_read(file_inode(inputFile1));

	if(sizeOfInputFile>0){
		temp = strsep(buf1pointer, endLine);
		line1 = kstrdup(temp, GFP_KERNEL);
	} else {
		printk("File 1 is empty");
	}
	sizeOfInputFile = i_size_read(file_inode(inputFile2));

	if(sizeOfInputFile>0){
		temp = strsep(buf2pointer, endLine);
		line2 = kstrdup(temp, GFP_KERNEL);
	} else {
		printk("File 2 is empty");
	}

	while(line1!=NULL && line2!=NULL){
		printk("\nline1 %s",line1);
		printk("\nline2 %s\n",line2);

		printk("\n buf1pointer %s",*buf1pointer);
		printk("\n buf2pointer %s\n",*buf2pointer);
	
		/*
		Need to do strcpy here becuase if do line2 = strcat(temp2.line2), line2 will start pointing
		to temp and later on we will free temp which will result in garbage values for line2
		*/
		if(temp2!=NULL){
			strcat(temp2,line2);
			kfree(line2);
			line2 = kstrdup(temp2, GFP_KERNEL);
		}
		if(temp1!=NULL){
			strcat(temp1,line1);
			kfree(line1);
			line1 = kstrdup(temp1, GFP_KERNEL);
		}

		printk("\n buf1pointer %s",*buf1pointer);
		printk("\n buf2pointer %s\n",*buf2pointer);
	
		printk("\nline1 again %s",line1);
		printk("\nline2 again %s\n",line2);
		
		// Use strcmp if case sensitive merge else use strncasecmp
		if(unique_records && case_insensitive)
			comparator = strncasecmp(line1, line2, strlen(line1));
		else comparator = strcmp(line1, line2);

		if(comparator >= 1){
			printk("%s greater than %s\n",line1, line2);

			if(check_unsorted(case_insensitive, error_unsorted, line2, previousLine)){
				ret = -EINVAL;
				goto error;
			}
			
			if(writeLine(case_insensitive, unique_records, line2, previousLine))
			{
				write_to_buffer(outputbuf, line2, tempFile);
				strcpy(previousLine,line2);
				printk("previousLine %s", previousLine);
			}
			/*
			We should not directly assign line2 to strsep as strsep returns a pointer to buf2pointer
			and later when we modify line2, it will effectively modify buf2pointer.(This happens when 
			copy line2 into copy)
			*/
			temp = strsep(buf2pointer,endLine);
			kfree(line2);
			line2 = kstrdup(temp, GFP_KERNEL);
			printk("line 2 %s ", line2);

		} else if(comparator <= -1){
			printk("%s lesser than %s\n",line1, line2);

			if(check_unsorted(case_insensitive, error_unsorted, line1, previousLine)){
				ret = -EINVAL;
				goto error;
			}
			
			if(writeLine(case_insensitive, unique_records, line1, previousLine))
			{
				write_to_buffer(outputbuf, line1, tempFile);
				strcpy(previousLine,line1);
				printk("previousLine %s", previousLine);
			}
			
			temp = strsep(buf1pointer,endLine);
			kfree(line1);
			line1 = kstrdup(temp, GFP_KERNEL);
			printk("line 1  %s", line1);		

		} else {

			printk("%s equal to %s\n",line1, line2);
			write_to_buffer(outputbuf, line2, tempFile);
			if(!unique_records || duplicates) {
				write_to_buffer(outputbuf, line2, tempFile);
			}
			temp =  strsep(buf1pointer,endLine);
			kfree(line1);
			line1 = kstrdup(temp,GFP_KERNEL);

			temp = strsep(buf2pointer,endLine);
			kfree(line2);
			line2 = kstrdup(temp,GFP_KERNEL);
		
		}

		if(*buf2pointer==NULL) {

			printk("Found NULL\n");
			kfree(temp2);
			temp2 = kstrdup(line2, GFP_KERNEL);
			if(temp2==NULL){
				printk("Could not allocate memory to temp2");
			}
			printk("Temp 2 %s\n", temp2);
			count =0;
			while(count<size){
				buffer2[count] = '\0';
				count++;
			}

			success = vfs_read(inputFile2, buffer2, size, &inputFile2->f_pos);
			if(success < 0) {
				ret = -EBADF;
				goto error;
			}
			if(success){
				printk("\nBuffer1 %s\n buffer2 %s", buffer1, buffer2);
				array2ptr = buffer2;
				buf2pointer = &array2ptr;
				temp = strsep(buf2pointer,endLine);
				kfree(line2);
				line2 = kstrdup(temp, GFP_KERNEL);
			} else {
				line2 = NULL;
				printk("VFS read failed\n");
				break;
			}
		} else {
			kfree(temp2);
			temp2 = NULL;
		}

		if(*buf1pointer==NULL){

			printk("Found NULL");
			kfree(temp1);
			temp1 = kstrdup(line1,GFP_KERNEL);
			if(temp1==NULL){
				printk("Could not allocate memory to temp1");
			}
			printk("Temp1 %s\n", temp1);

			count =0;
			while(count<size){
				buffer1[count] = '\0';
				count++;
			}

			success = vfs_read(inputFile1, buffer1, size, &inputFile1->f_pos);
			if(success < 0) {
				ret = -EBADF;
				goto error;
			}
			if(success){
				printk("\nBuffer1 %s\n buffer2 %s", buffer1, buffer2);
				array1ptr = buffer1;
				buf1pointer = &array1ptr;
				
				temp = strsep(buf1pointer,endLine);
				kfree(line1);
				line1 = kstrdup(temp, GFP_KERNEL);
			} else {
				line1 = NULL;
				printk("VFS Read failed\n");
				break;
			}
		} else {
			kfree(temp1);
			temp1 = NULL;
		}
	}

	printk("\n=======Main Loop over========");
	printk("\nline1 %s",line1);
	printk("\nline2 %s\n",line2);
	printk("\ntemp1 %s",temp1);
	printk("\ntemp2 %s\n",temp2);
	printk("\n buf1pointer %s",*buf1pointer);
	printk("\n buf2pointer %s\n",*buf2pointer);
		
	while(line1!=NULL){
		printk("\nline1 %s",line1);

		
		if(check_unsorted(case_insensitive, error_unsorted, line1, previousLine)){
			ret = -EINVAL;
			goto error;
		}
		
		if(writeLine(case_insensitive, unique_records, line1, previousLine))
		{
			write_to_buffer(outputbuf, line1, tempFile);
			strcpy(previousLine,line1);
			printk("previousLine %s", previousLine);
		}
		
		kfree(line1);
		temp = strsep(buf1pointer, endLine);
		line1 = kstrdup(temp, GFP_KERNEL);
		printk("\nline1 %s",line1);

		if(*buf1pointer == NULL){

			printk("Found NULL");
			kfree(temp1);
			temp1 = kstrdup(line1,GFP_KERNEL);
			if(temp1==NULL){
				printk("Could not allocate memory to temp1");
			}
			printk("Temp1 %s\n", temp1);

			count =0;
			while(count<size){
				buffer1[count] = '\0';
				count++;
			}

			success = vfs_read(inputFile1, buffer1, size, &inputFile1->f_pos);
			if(success){
				printk("\nBuffer1 %s\n buffer2 %s", buffer1, buffer2);
				array1ptr = buffer1;
				buf1pointer = &array1ptr;
				
				temp = strsep(buf1pointer,endLine);
				kfree(line1);
				if(temp1!=NULL){
					strcat(temp1, temp);
					line1 = kstrdup(temp1, GFP_KERNEL);
				} else{
					line1 = kstrdup(temp, GFP_KERNEL);	
				}
				printk("\nBuffer1 %s\n buffer2 %s", buffer1, buffer2);
			} else {
				printk("VFS Read failed\n");
				break;
			}
		
		}
	}

	while(line2!=NULL){
		printk("\n buf2pointer %s\n",*buf2pointer);

		if(check_unsorted(case_insensitive, error_unsorted, line2, previousLine)){
			ret = -EINVAL;
			goto error;
		}
		
		if(writeLine(case_insensitive, unique_records, line2, previousLine))
		{
			write_to_buffer(outputbuf, line2, tempFile);
			strcpy(previousLine,line2);
			printk("previousLine %s", previousLine);
		}
		
		temp = strsep(buf2pointer,endLine);
		kfree(line2);
		line2 = kstrdup(temp, GFP_KERNEL);
		printk("line 2 %s ", line2);

		if(*buf2pointer==NULL) {

			printk("Found NULL\n");
			kfree(temp2);
			temp2 = kstrdup(line2, GFP_KERNEL);
			if(temp2==NULL){
				printk("Could not allocate memory to temp2");
			}
			printk("Temp 2 %s\n", temp2);
			count =0;
			while(count<size){
				buffer2[count] = '\0';
				count++;
			}

			success = vfs_read(inputFile2, buffer2, size, &inputFile2->f_pos);
			if(success){
				printk("\nBuffer1 %s\n buffer2 %s", buffer1, buffer2);
				array2ptr = buffer2;
				buf2pointer = &array2ptr;
				temp = strsep(buf2pointer,endLine);
				kfree(line2);
				if(temp2!=NULL){
					strcat(temp2, temp);
					line2 = kstrdup(temp2, GFP_KERNEL);
				} else{
					line2 = kstrdup(temp, GFP_KERNEL);	
				}
			} else {
				printk("VFS read failed\n");
				break;
			}
		}
	}
	if(strlen(outputbuf)>0){
		vfs_write(tempFile, outputbuf, strlen(outputbuf),&tempFile->f_pos);
	}
	success = copy_to_user(&(((struct arguments*)args)->data), &outputLines, sizeof(unsigned int));
	if(success != 0){
		ret = -EINVAL;
		goto error;
	}

	old_dir = tempFile->f_path.dentry->d_parent->d_inode;
	new_dir = outputFile->f_path.dentry->d_parent->d_inode;
	old_dentry = tempFile->f_path.dentry;
	new_dentry = outputFile->f_path.dentry;

	success = vfs_rename(old_dir, old_dentry, new_dir, new_dentry, NULL, 0);
	printk("VFS rename reutrned %d", success);

	success = vfs_unlink(new_dir, new_dentry, NULL);
	printk("VFS unlink reutrned %d", success);

	printk("Success %d\n",outputLines);
	goto success;
	error:
		old_dir = tempFile->f_path.dentry->d_parent->d_inode;
		new_dir = outputFile->f_path.dentry->d_parent->d_inode;
		old_dentry = tempFile->f_path.dentry;
		new_dentry = outputFile->f_path.dentry;
		vfs_unlink(old_dir, old_dentry, NULL);
		
		if(sizeOfOutputFile<=0)
			vfs_unlink(new_dir, new_dentry, NULL); 
	success:
		kfree(buffer1);
		kfree(buffer2);
		kfree(outputbuf);
		kfree(line1);
		kfree(line2);
		kfree(temp1);
		kfree(temp2);
		filp_close(tempFile, NULL);
	outputFile:
		filp_close(outputFile, NULL);
	inputFile2:
		filp_close(inputFile2, NULL);
	inputFile1:
		filp_close(inputFile1, NULL);
	
	
	set_fs(old_fs);
	l_return:
		kfree(k_args);
		return ret;
}
int write_to_buffer(char* outputbuf, char* line1, struct file * outputFile){
	int count;
	printk("outputbuf %s",outputbuf);
	outputLines++;
	if(strlen(outputbuf)+ strlen(line1)<size){
		strcat(outputbuf, line1);

		strcat(outputbuf, "\n");
		
	} else {
		vfs_write(outputFile, outputbuf, strlen(outputbuf),&outputFile->f_pos);	
		count = 0;
		while(count<size){
			outputbuf[count++] = '\0';
		}
		strcat(outputbuf, line1);
		strcat(outputbuf, "\n");
	}
	printk("outputbuf %s",outputbuf);
	return 0;
}

/*
If unique flag is set compare current line with previous line (use case insensitive if it is set)
and skip writing the record to the buffer. Also if -t is not set then check if current line is smaller
than previous line and skip writing to buffer in that case.
*/
bool writeLine(bool case_insensitive, bool unique_records, char * line, char * previousLine){
	return !(unique_records && strcmp(previousLine, line)==0) &&
			!(unique_records && case_insensitive && (strncasecmp(line, previousLine, strlen(line))==0)) &&
			 !(!case_insensitive && strcmp(line,previousLine)<=-1) &&  !(case_insensitive && strncasecmp(previousLine, line, strlen(previousLine))>=1);
}

/*
If throw error on coming across unsorted records
Compare current line and previos string. If current line is smaller than previous string
throw error. If case insensitive and current line then check accordingly
*/
bool check_unsorted(bool case_insensitive, bool error_unsorted, char* line, char * previousLine){
	return error_unsorted && (strcmp(line,previousLine)<=-1 || 
				(case_insensitive && strncasecmp(line, previousLine, strlen(line))<=-1));
}

static int __init init_sys_xmergesort(void)
{
	printk("installed new sys_xmergesort module\n");
	if (sysptr == NULL)
		sysptr = xmergesort;
	return 0;
}
static void  __exit exit_sys_xmergesort(void)
{
	if (sysptr != NULL)
		sysptr = NULL;
	printk("removed sys_xmergesort module\n");
}
module_init(init_sys_xmergesort);
module_exit(exit_sys_xmergesort);
MODULE_LICENSE("GPL");
