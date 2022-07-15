
var BLOB_IO = (function () {
    var interface = {}

    // Private data.
    const blobIndex = new Map(); // Map from blob descriptor to blob IO info.
    const blobDesc = new Map(); // Map from blob pointer to blob descriptor.

    // Public methods.
    interface.registerBlob = function(blob) {
        /* Registers a blob object for IO operations to/from webassembly. The
        result of this function is a blob descriptor that identifies a unique
        IO stream that the webassembly module can write to/read from.

        Arguments:
            blob: Blob object to back a webassembly IO stream. The
                original blob is passed by value and is not modified.
            mode: String corresponding to one of the standard C file IO modes.
            wasmModule: Webassembly module that exports blob IO functions.
        returns:
            An integer blob descriptor that identifies the input/output stream
            associated with the blob argument.
        */
        const keys = Array.from(blobIndex.keys());
        keys.sort(function(left, right){return left - right;});
        // Find the lowest available blob descriptor
        let blob_desc = 0;
        for (var i = 0; i < keys.length; i++){
            if (blob_desc < keys[i]){
                break;
            } else {
                blob_desc++;
            }
        }
        indexEntry = {
            blob:blob, // File object (blob) or other blob
        };
        blobIndex.set(blob_desc, indexEntry);
        return blob_desc;
    }

    interface.unregisterBlob = function(blob_desc) {
        /* Unregisters a blob object for IO operations to/from webassembly. This
        removes the stream from the blob index. However, it does not guarantee
        that there are no blob pointers left with dangling references to a blob
        descriptor that no longer exists - use with caution!
        
        Arguments: 
            blob_desc: Blob descriptor identifying the IO stream to close.
        */
        blobIndex.delete(blob_desc);
    }

    interface.openBlob = function(blob_desc, mode, wasmModule){
        /* Opens a blob reader/writer in webassembly for IO operations. The
        result of this function is a blob pointer that can be passed to
        any C function that takes a BLOB*. The reads and writes (and overwrites)
        are done to the blob object backing the IO stream identified by blob_desc.

        Arguments:
            blob_desc: Blob descriptor identifying an IO stream.
            mode: String corresponding to one of the standard C file IO modes.
            wasmModule: Webassembly module that exports blob IO functions.
        returns:
            A pointer that can be passed to other webassembly functions.
        */
        // Select the correct mode to pass to the C library.
        const mode_lookup = {'r':0, 'r+':1, 'w':2, 'w+':3, 'a':4, 'a+':5};
        const iobl_mode = mode_lookup[mode];
        if (typeof iobl_mode === 'undefined'){
            throw new Error('Undefined blob open mode: '+String(mode));
        }
        if ((mode === 'w') || (mode === 'w+')){  // Overwrite if write-mode.
            indexEntry.blob = new Blob([], {type: 'text/plain'});
        }

        // Open the blob for IO operations from webassembly.
        blob_ptr = wasmModule.instance.exports.bopen(blob_desc, iobl_mode);
        if (blob_ptr == 0){
            // Corresponds to an error code from wasm (most likely out of memory)
            throw new Error('Could not open blob from WASM - bopen returned NULL.');
        }
        blobDesc.set(blob_ptr, blob_desc);
        return blob_ptr;
    }

    interface.getBlob = function(blob_desc) {
        /* Returns the underlying blob associated with the IO stream
        identified by a blob descriptor.

        Arguments:
            blob_desc: Blob descriptor identifying an IO stream.
        returns:
            blob: The blob object backing the IO stream.
        */
        const indexEntry = blobIndex.get(blob_desc);
        if (typeof indexEntry === 'undefined'){
            throw new Error('Blob descriptor '+String(blob_desc)+' not in blobIndex.');
        }
        return indexEntry.blob;
    }

    interface.closeBlob = function(blob_ptr, wasmModule){
        /* Asks the webassembly module to close the blob. This does not 
        close or modify the IO stream of the corresponding blob descriptor.

        Arguments:
            blob_ptr: Pointer to a blob in the webassembly module.
            wasmModule: Webassembly module that exports blob IO functions.
        */
        let ptrWasPresent = blobDesc.delete(blob_ptr);
        if (ptrWasPresent) {
            // if pointer was in blobDesc, then it is safe to free the
            // pointer in webassembly.
            wasmModule.instance.exports.bclose(blob_ptr);
        }
    }


    // Private methods.
    function fetchBuffer(blob_desc, buffer_ptr, buffer_size, stream_position){
        /* Reads buffer_size bytes from a given location within a blob and
        places them into the webassembly buffer pointed to by buffer_ptr.00

        Arguments:
            blob_desc: Integer blob descriptor that identifies the IO stream
            to read from.
            buffer_ptr: Pointer to the webassembly memory to write the content.
            buffer_size: Number of bytes to read into buffer_ptr.
            stream_position: Location within the blob to read  
        */
        const indexEntry = blobIndex.get(blob_desc);
        if (typeof indexEntry === 'undefined'){
            throw new Error('Blob descriptor '+String(blob_desc)+' not in blobIndex.');
        }
        // 0. Map buffer to the webassembly linear memory.
        const buffer = new Uint8Array(wasmMemory.buffer, buffer_ptr, buffer_size);
        // 1. Load data from the file blob.
        const reader = new FileReaderSync();
        // Stream_position is a BigInt but must be cast to Number for .slice call.
        // TODO: check less than MAX_SAFE_INTEGER (roughly ~9 petabyte offset).
        const blob = indexEntry.blob.slice(Number(stream_position), Number(stream_position) + buffer_size);
        // 2. Fill the webassembly buffer with the file data.
        const data = new Uint8Array(reader.readAsArrayBuffer(blob));
        buffer.set(data, 0);
        return data.length;
    }


    function flushBuffer(blob_desc, buffer_ptr, buffer_size, stream_position){
        /* Writes buffer_size bytes from webassembly the buffer into the blob,
        starting at location stream_position in the blob.

        Arguments:
            blob_desc: Integer blob descriptor that identifies the IO stream
            to write to.
            buffer_ptr: Pointer to the webassembly memory containing content to write.
            buffer_size: Number of bytes to write from buffer_ptr.
            stream_position: Location to start writing in the blob.
        */
        const indexEntry = blobIndex.get(blob_desc);
        if (typeof indexEntry === 'undefined'){
            throw new Error('Blob descriptor '+String(blob_desc)+' not in blobIndex.');
        }
        // 0. Map buffer from the webassembly linear memory.
        const buffer = new Uint8Array(wasmMemory.buffer, buffer_ptr, buffer_size);
        // 1. Create a new blob from the buffer.
        const chunk = new Blob([buffer],{type:'text/plain'});
        if ((stream_position == indexEntry.blob.size) || (stream_position < 0)) {
            // Append chunk to blob.
            indexEntry.blob = new Blob([indexEntry.blob, chunk], {type: 'text/plain'});
        } else if (stream_position < indexEntry.blob.size) {
            // Modify chunk within blob using slicing.
            const firstBoundary = Number(stream_position);
            const secondBoundary = Number(stream_position) + chunk.size;
            const blobs = [];
            if (firstBoundary > 0){
                blobs.push(indexEntry.blob.slice(0, firstBoundary));
            }
            blobs.push(chunk);
            if (secondBoundary < indexEntry.blob.size){
                blobs.push(indexEntry.blob.slice(secondBoundary));
            }
            indexEntry.blob = new Blob(blobs, {type:'text/plain'});
        } else if (stream_position > indexEntry.blob.size) {
            // Writes past the end of the file should fill the intermediate zone with zeros.
            const buf = new Uint8Array(Number(stream_position) - indexEntry.blob.size);
            const zeros = new Blob(buf);
            indexEntry.blob = new Blob([indexEntry.blob, zeros, chunk], {type:'text/plain'});
        }
    }


    function blobSize(blob_desc){
        /* Gets the size of the buffer idenfied by blob_desc.

        Arguments:
            blob_desc: Integer blob descriptor that identifies the IO stream.
        Returns:
            The size of the blob, in bytes.
        */
        const indexEntry = blobIndex.get(blob_desc);
        const size = 0;
        if (typeof indexEntry !== 'undefined'){
            size = indexEntry.blob.size;
        }
        return BigInt(size);
    }

    function printString(buffer_ptr, buffer_size){
        const buffer = new Uint8Array(wasmMemory.buffer, buffer_ptr, buffer_size);
        const string = new TextDecoder().decode(buffer);
        console.log(string);
    }

    // Partial environment object - requires memory import too.
    var wasmImports = {
        env: {
            js_print_int: console.log,
            js_print_string: printString,
            js_fetch_buffer: fetchBuffer,
            js_flush_buffer: flushBuffer,
            js_seek_end: blobSize,
        },
    }
    interface.wasmImports = wasmImports;

    return interface;
}());

