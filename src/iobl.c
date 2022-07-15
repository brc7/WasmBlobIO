#include "iobl.h"
#include "walloc.h"


BLOB* bopen(int blob_desc, int mode){
	/* Open a new blob for read/write/edit operations.
	Note: To pass c-strings from JS, the JS needs access to an
	internally-allocated buffer from wasm. Since most bopen calls
	will originate from JS, we depart from the standard cstdio
	interface to make calls from JS easier.
	Arguments:
		blob_desc: Blob descriptor that corresponds to an open
			blob in the browser, obtained from JS.
		mode: File opening mode. Translation from the stdio strings:
			{0: "r", 1: "r+", 2: "w", 3: "w+", 4: "a", 5: "a+"}
	Returns:
		Pointer to the new blob. NULL if error or bad flags.
	*/
	int flags = bflags(mode);
	if (flags == 0){
		return NULL;
	}

	BLOB* blob = (BLOB*)(malloc(sizeof(BLOB)));
	char* buf = (char*)(malloc(IOBL_DEFAULT_BUFFER_SIZE * sizeof(char)));

	if ((blob == NULL) || (buf == NULL)){
		 // Failed to allocate buffer or blob.
		if (buf == NULL){ free(blob); }
		if (blob == NULL){ free(blob); }
		return NULL;
	}

	blob->flags = flags;
	blob->descriptor = blob_desc;
	blob->buffer = buf;
	blob->buffer_capacity = IOBL_DEFAULT_BUFFER_SIZE;
	blob->buffer_length = 0;
	blob->buffer_position = 0;
	if (blob->flags & IOBL_APM){
		blob->stream_position = js_seek_end(blob_desc);
	} else {
		blob->stream_position = 0;
	}

	return blob;
}

int bflags(int mode){
	int flags = 0;
	switch (mode){
		case 0:  // "r" input stream
			flags |= IOBL_RDM;
			break;
		case 1:  // "r+" update stream
			flags |= IOBL_RDM;
			flags |= IOBL_WRM;
			break;
		case 2:  // "w" output stream
			flags |= IOBL_WRM;
			break;
		case 3:  // "w+" update stream
			flags |= IOBL_RDM;
			flags |= IOBL_WRM;
			break;
		case 4:  // "a" output stream
			flags |= IOBL_RDM;
			flags |= IOBL_APM;
			break;
		case 5:  // "a+" update stream
			flags |= IOBL_RDM;
			flags |= IOBL_WRM;
			flags |= IOBL_APM;
			break;
	}
	return flags;
}

int bclose(BLOB* blob){
	/* Close a blob and free the underlying buffers, if the
	buffers are owned by the blob.
	Arguments:
		blob: Pointer to a blob to close.
	Returns:
		0 if blob was closed succesfully.
	*/
	bflush(blob);
	if ((blob->flags & IOBL_BXT) && (blob->buffer != NULL)){
		free(blob->buffer);
		blob->buffer = NULL;
	}
	blob->descriptor = -1;
	blob->buffer_capacity = 0;
	blob->buffer_length = 0;
	blob->buffer_position = 0;
	blob->stream_position = 0;
	blob->flags &= ~(IOBL_RDM | IOBL_WRM | IOBL_APM | IOBL_BIW);
	return 0;
}

int bflush(BLOB* blob){
	/* Flushes internal buffer to output, with the following behavior for each mode.
		"r": No op.
		"a", "a+": Appends the buffer contents to the end of the blob.
		"w", "w+": Appends the buffer contents at the current position. If the
		position is past EOF, the intermediate bytes are filled with 0x00.
	Arguments:
		blob: Pointer to an open blob.
	Returns:
		0 if the write was successful.
	*/
	long long pos = -1;
	if (blob->flags & IOBL_APM){  // Blob is appendable.
		pos = -1;
	} else if (blob->flags & IOBL_WRM){  // Blob is writeable.
		pos = blob->stream_position - blob->buffer_length;
	} else {
		return -1;
	}
	if ((blob->flags & IOBL_BIW) && (blob->buffer_length > 0)){
		// Buffer is writing and is non-empty.
		js_flush_buffer(blob->descriptor, blob->buffer, blob->buffer_length, pos);
		// TODO: Error handling from JS through a js_flush_buffer return value.
		blob->buffer_length = 0;
		blob->buffer_position = 0;
	}
	blob->flags &= ~IOBL_BIW; // Buffer is not (necessarily) writing anymore.
	return 0;
}

int bfetch(BLOB* blob){
	if (blob->flags & IOBL_RDM){  // Blob is readable.
		int num_valid = js_fetch_buffer(blob->descriptor, blob->buffer, blob->buffer_capacity, blob->stream_position);
		blob->buffer_length = num_valid;
		blob->buffer_position = 0;
		return num_valid;
	}
	return -1;
}

long long bread(void* buf, long long size, long long count, BLOB* blob){
	if (!(blob->flags & IOBL_RDM)){  // Blob is not readable.
		return 0;
	}
	char* data = (char*)(buf);
	long long num_bytes = size * count;
	long long bytes_read = 0;
	long long items_read = 0;
	long long item_progress = 0;  // in bytes.

	int bytes_left = blob->buffer_length - blob->buffer_position;

	// TODO: Replace this slow loop-based memory copy with one based on the memory.fill and memory.copy
	// instructions. We should wait to do this until browser support improves. See the following post:
	// https://stackoverflow.com/questions/67068195/does-clang-provide-intrinsics-for-webassemblys-memory-fill-and-memory-copy
	while (bytes_read < num_bytes){
		if (bytes_left <= 0){
			int ret = bfetch(blob);
			if (ret < 0){  // Found error.
				blob->flags |= IOBL_ERR;
				break;
			} else if (ret == 0) { // EOF.
				blob->flags |= IOBL_EOF;
				break;
			}
			bytes_left = blob->buffer_length - blob->buffer_position;
		}
		data[bytes_read] = blob->buffer[blob->buffer_position];
		blob->stream_position++;
		blob->buffer_position++;
		bytes_read++;
		item_progress++;
		bytes_left--;
		if (item_progress == size){
			item_progress = 0;
			items_read++;
		}
	}
	return items_read;
}

long long bwrite(void* buf, long long size, long long count, BLOB* blob){
	if (!(blob->flags & IOBL_WRM)){  // Blob is not writeable.
		return 0;
	}
	blob->flags |= IOBL_BIW; // This is a write-call, if the blob wasn't writing before, it is now.
	char* data = (char*)(buf);
	long long num_bytes = size * count;
	long long bytes_written = 0;
	long long items_written = 0;
	long long item_progress = 0;  // bytes.

	int bytes_left = blob->buffer_capacity - blob->buffer_length;
	// js_print_string("BWRITE_INIT\n", 12);
	// js_print_string("BUF_LEN:", 8);
	// js_print_int(blob->buffer_length);
	// js_print_string("BUF_CAP:", 8);
	// js_print_int(blob->buffer_capacity);
	// js_print_string("BYTES_LEFT:", 11);
	// js_print_int(bytes_left);
	// js_print_string("BYTES_TO_WRITE:", 15);
	// js_print_int(num_bytes);

	while (bytes_written < num_bytes){
		if (bytes_left <= 0){
			// js_print_string("****************************************\n", 41);
			// js_print_string("FLUSH\n", 6);
			// js_print_string("BUF_LEN:", 8);
			// js_print_int(blob->buffer_length);
			// js_print_string("BUF_CAP:", 8);
			// js_print_int(blob->buffer_capacity);
			// js_print_string("BYTES_LEFT:", 11);
			// js_print_int(bytes_left);
			// js_print_string("BYTES_WRITTEN:", 14);
			// js_print_int(bytes_written);
			int ret = bflush(blob);
			blob->flags |= IOBL_BIW;
			if (ret < 0){  // Error code.
				blob->flags |= IOBL_ERR;
				break;
			}
			bytes_left = blob->buffer_capacity - blob->buffer_length;
			// js_print_string("POST-FLUSH\n", 11);
			// js_print_string("BUF_LEN:", 8);
			// js_print_int(blob->buffer_length);
			// js_print_string("BUF_CAP:", 8);
			// js_print_int(blob->buffer_capacity);
			// js_print_string("BYTES_LEFT:", 11);
			// js_print_int(bytes_left);
			// js_print_string("BYTES_WRITTEN:", 14);
			// js_print_int(bytes_written);
		}
		blob->buffer[blob->buffer_length] = data[bytes_written];
		blob->buffer_length++;
		blob->stream_position++;
		bytes_written++;
		item_progress++;
		bytes_left--;
		if (item_progress == size){
			item_progress = 0;
			items_written++;
		}
	}
	blob->flags |= IOBL_BIW;
	return items_written;
}

int bgetc(BLOB* blob){
	if (!(blob->flags & IOBL_RDM)){  // Blob is not readable.
		return -1;
	}
	int bytes_left = blob->buffer_length - blob->buffer_position;
	if (bytes_left <= 0){
		int ret = bfetch(blob);
		if (ret < 0){
			blob->flags |= IOBL_ERR;
			return -1;
		} else if (ret == 0){
			blob->flags |= IOBL_EOF;
			return -1;
		}
	}
	int c = (int)(blob->buffer[blob->buffer_position]);
	blob->stream_position++;
	blob->buffer_position++;
	return c;
}

int bputc(int character, BLOB* blob){
	if (!(blob->flags & IOBL_WRM)){  // Blob is not writeable.
		return -1;
	}
	int bytes_left = blob->buffer_capacity - blob->buffer_length;
	if (bytes_left <= 0){
		int ret = bflush(blob);
		if (ret < 0){
			blob->flags |= IOBL_ERR;
			return -1;
		}
	}
	blob->buffer[blob->buffer_length] = (char)(character);
	blob->buffer_length++;
	blob->buffer_position++;
	blob->flags |= IOBL_BIW;
	return 0;
}

char* bgets(char* str, int num, BLOB* blob){
	/* Reads from blob until a newline '\n' character or num-1 bytes have been read.
	Arguments:
		str: Pointer to a buffer for the string.
		num: Maximum length of the string (excludes null terminator). Must be < 2^31 - 1.
		blob: Pointer to a blob object.
	Returns:
		Pointer to a null-terminated string.
	*/
	int num_bytes = num - 1;
	int bytes_read = 0;
	int bytes_left = blob->buffer_length - blob->buffer_position;
	while (bytes_read < num_bytes){
		if (bytes_left <= 0){
			int ret = bfetch(blob);
			if (ret < 0){
				blob->flags |= IOBL_ERR;
				break;
			} else if (ret == 0){
				blob->flags |= IOBL_EOF;
				break;
			}
			bytes_left = blob->buffer_length - blob->buffer_position;
		}
		char c = blob->buffer[blob->buffer_position];
		str[bytes_read] = c;
		blob->stream_position++;
		blob->buffer_position++;
		bytes_read++;
		bytes_left--;
		if (c == '\n'){
			break;
		}
	}
	str[bytes_read] = '\0';
	return str;
}

int bputs(const char* str, BLOB* blob){
	/* Writes to blob until a null-terminator is reached. The null-terminator is not written.
	Arguments:
		str: Pointer to a null-terminated string of length < 2^31 - 1.
		blob: Pointer to a blob object.
	Returns:
		Number of bytes written if successful, -1 otherwise.
	*/
	int bytes_left = blob->buffer_length - blob->buffer_position;
	int i = 0;
	while (str[i] != '\0'){
		if (bytes_left <= 0){
			int ret = bflush(blob);
			if (ret < 0){  // Error code.
				blob->flags |= IOBL_ERR;
				break;
			}
			bytes_left = blob->buffer_capacity - blob->buffer_length;
		}
		blob->buffer[blob->buffer_length] = str[i];
		blob->stream_position++;
		blob->buffer_length++;
		i++;
	}
	blob->flags |= IOBL_BIW;
	return i;
}

int bseek(BLOB* blob, long long offset, int origin){
	bflush(blob);
	switch(origin){
		case IOBL_SEEK_SET:  // Seek to beginning of file.
			blob->stream_position = offset;
		break;
		case IOBL_SEEK_CUR:  // Seek from current position.
			blob->stream_position += offset;
		break;
		case IOBL_SEEK_END:  // Seek to EOF.
			blob->stream_position = js_seek_end(blob->descriptor);
		break;
		default:
			return -1;
	}
	blob->buffer_length = 0;
	blob->buffer_position = 0;
	blob->flags &= ~IOBL_EOF;
	return 0;
}

long long btell(BLOB* blob){
	return blob->stream_position;
}

int bsetpos(BLOB* blob, const long long* pos){
	bflush(blob);
	blob->stream_position = *pos;
	blob->buffer_length = 0;
	blob->buffer_position = 0;
	blob->flags &= ~IOBL_EOF;
	return 0;
}

int bgetpos(BLOB* blob, long long* pos){
	*pos = blob->stream_position;
	return 0;
}

void brewind(BLOB* blob){
	bflush(blob);
	blob->stream_position = 0;
	blob->buffer_length = 0;
	blob->buffer_position = 0;
	blob->flags &= ~IOBL_EOF;
}

int beof(BLOB* blob){
	return blob->flags & IOBL_EOF;
}

void clearerr(BLOB* blob){
	blob->flags &= ~IOBL_ERR;
	return;
}
