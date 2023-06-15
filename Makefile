run:
	# fusermount -uz mountdir
	gcc lfs.c -lfuse -D_FILE_OFFSET_BITS=64 -o lfs -g
	./lfs lfs_disk mountdir -f -s
