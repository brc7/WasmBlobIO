#include "iobl.h"
#include "walloc.h"


int testWriteBPuts(BLOB* blob){
	bputs("Hello world!", blob);
	bflush(blob);
	return 0; // Check blob from JS.
}

int testReadBgets(BLOB* blob){
	char* buf = (char*)(malloc(100 * sizeof(char)));
	bgets(buf, 100, blob);
	char* desired_result = "abcd";
	int flag = 0;
	for (int i = 0; i < 4; i++){
		flag = (int)(buf[i] != desired_result[i]);
	}
	free(buf);
	return flag;
}

int testWriteBWrite(BLOB* blob){
	char* buf = "abcd";
	bwrite(buf, sizeof(char), 4, blob);
	bflush(blob);
	return 0; // Check blob from JS.
}

int testWriteLongBWrite(BLOB* blob){
	// Tests behavior when we want to write something longer
	// than the blob's internal buffer.
	int string_size = 2000000;
	char* buf = (char*)(malloc(string_size * sizeof(char)));
	for (int i = 0; i < string_size; i++){
		buf[i] = 'c';
	}
	bwrite(buf, sizeof(char), string_size, blob);
	bflush(blob);
	free(buf);
	return 0; // Check blob from JS.
}

int testFreeLargePage(){
	int size = 2000000;
	char* buf = (char*)(malloc(size * sizeof(char)));
	for (int i = 0; i < size; i++){
		buf[i] = 'c';
	}
	free(buf);
	return 0;
}

int testReadToEOF(BLOB* blob){
	// Call with "abcd" as the blob.
	char* buf = (char*)(malloc(100 * sizeof(char)));
	long long bytes_read = bread(buf, 1, 100, blob);
	int flag = 0;
	if (bytes_read != 4){
		// Verify that correct number of bytes were read.
		flag = -1;
	}
	if (beof(blob) == 0){
		// Verify that EOF is correctly set.
		flag = -2;
	}
	return flag;
}


int testReadBRead(BLOB* blob){
	// Call with "abcdefg" as the blob.
	char* buf = (char*)(malloc(4 * sizeof(char)));
	long long bytes_read = bread(buf, 1, 4, blob);
	char* desired_result = "abcd";
	int flag = 0;
	for (int i = 0; i < 4; i++){
		if (buf[i] != desired_result[i]){
			flag = -1;
		}
	}
	if (beof(blob) != 0){
		// Verify that EOF is correctly set.
		flag = -2;
	}
	free(buf);
	return flag;
}

