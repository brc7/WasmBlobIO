# Wasm Blob IO


## Introduction
This is a minimal IO implementation for large blobs in WASM. It implements most of the C standard library functions on top of a blob-backed read/write system in JavaScript.

## Build
You'll need a reasonably modern version of clang to run the Makefile. This will produce a compiled library as well as a small web app that runs tests.

## Tests 
In the top-level project directory, run:

```make ```

This will build the library (in the `/lib` subdirectory) and a test app (in the `/test` subdirectory). The test app can be served using any webserver - we use Python. 

```python3 -m http.server --directory test```

This will serve the app at `http://localhost:8000/test.html`, which should display the following results.

![App before running tests.](https://github.com/brc7/WasmBlobIO/blob/main/img_0.png?raw=true)

After running the tests:

![App after running tests, listing successful tests.](https://github.com/brc7/WasmBlobIO/blob/main/img_1.png?raw=true)


