#pragma once
#define IOBL_DEFAULT_BUFFER_SIZE 1000000

#define IOBL_RDM  0x0001      /* opened for reading mode */
#define IOBL_WRM  0x0002      /* opened for writing mode */
#define IOBL_APM  0x0004      /* opened in append mode */
#define IOBL_EOF  0x0008      /* reached end of blob */
#define IOBL_ERR  0x0010      /* found error */
#define IOBL_BXT  0x0020      /* buffer is external (from malloc) */
#define IOBL_BIW  0x0040      /* buffer is writing (can be flushed) */


#define IOBL_SEEK_SET  0  /* Beginning of blob */
#define IOBL_SEEK_CUR  1  /* Current position of pointer */
#define IOBL_SEEK_END  2  /* End of blob */


typedef struct BLOB {
	int flags;
	int descriptor;
	int buffer_capacity;
	int buffer_length;
	int buffer_position;
	char* buffer;
	long long stream_position;
} BLOB;


// Exported functions (Javascript API)
BLOB* bopen(int blob_desc, int mode);  // Non-standard interface.
int bflags(int mode);  // Internal.
int bclose(BLOB* blob);

int bflush(BLOB* blob);
int bfetch(BLOB* blob);  // Non-standard.

long long bread(void* buf, long long size, long long count, BLOB* blob);
long long bwrite(void* buf, long long size, long long count, BLOB* blob);

int bgetc(BLOB* blob);
int bputc(int character, BLOB* blob); // internally convert character to unsigned char

char* bgets(char* str, int num, BLOB* blob);
int bputs(const char* str, BLOB* blob); // writes until null-termination
// consider adding bungetc

int bseek(BLOB* blob, long long offset, int origin);
long long btell(BLOB* blob);

int bsetpos(BLOB* stream, const long long* pos);
int bgetpos(BLOB* stream, long long* pos);

void brewind(BLOB* blob);
int beof(BLOB* blob);
void clearerr(BLOB* blob);


extern void js_print_int(int x)
	__attribute__((import_module("env"), import_name("js_print_int")));

extern void js_print_string(char* x, int len)
	__attribute__((import_module("env"), import_name("js_print_string")));

// External call to JS to refresh the buffer. Returns number of valid bytes written to buffer.
extern int js_fetch_buffer(int blob_desc, char* buffer, int buffer_size, long long stream_position)
	__attribute__((import_module("env"), import_name("js_fetch_buffer")));

// External call to JS to write the buffer.
extern void js_flush_buffer(int blob_desc, char* buffer, int buffer_size, long long stream_position)
	__attribute__((import_module("env"), import_name("js_flush_buffer")));

// External call to JS to find the end-of-blob stream position.
extern long long js_seek_end(int blob_desc)
	__attribute__((import_module("env"), import_name("js_seek_end")));
