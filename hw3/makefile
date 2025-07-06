
all: histext2fs

histext2fs: main.c ext2fs_print.c ext2fs.h ext2fs_print.h
	gcc -Wall -g -o histext2fs main.c ext2fs_print.c

clean:
	rm -f histext2fs *.o

.PHONY: all clean