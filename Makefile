obj-m += sys_xmergesort.o

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

all: xmergesort sys_xmergesort

sys_xmergesort:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

xmergesort: xmergesort.c
	gcc -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi xmergesort.c -o xmergesort

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f xmergesort
