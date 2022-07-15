var TESTCASES = (function () {

    const testList = [];
    var interface = {};

    function handleTest(name, passed, percentDone){
        let testObject = {
            passed:passed,
            name:name,
            progress:percentDone,
        };
        self.postMessage({
            eventType: "TESTCASE",
            eventData: testObject
        });
    }

    /* ADD TESTS HERE */
    function testWriteBPuts(wasmModule){
        let blob = new Blob([], {type:'text/plain'});
        // Register and write to blob IO.
        let blob_desc = BLOB_IO.registerBlob(blob);
        let blob_ptr = BLOB_IO.openBlob(blob_desc, 'w', wasmModule);
        wasmModule.instance.exports.testWriteBPuts(blob_ptr);
        blob = BLOB_IO.getBlob(blob_desc);
        // Clean up afterward.
        BLOB_IO.closeBlob(blob_ptr, wasmModule);
        BLOB_IO.unregisterBlob(blob_desc);
        const reader = new FileReaderSync();
        let results = reader.readAsText(blob);
        return (results === "Hello world!");
    }
    testList.push({test:testWriteBPuts, name:'testWriteBPuts'});

    function testReadBgets(wasmModule){
        let blob = new Blob(["abcd"]);
        let blob_desc = BLOB_IO.registerBlob(blob);
        let blob_ptr = BLOB_IO.openBlob(blob_desc, 'r', wasmModule);
        let value = wasmModule.instance.exports.testReadBgets(blob_ptr);
        BLOB_IO.closeBlob(blob_ptr, wasmModule);
        BLOB_IO.unregisterBlob(blob_desc);
        return (value === 0);
    }
    testList.push({test:testReadBgets, name:'testReadBgets'});

    function testReadToEOF(wasmModule){
        let blob = new Blob(["abcd"]);
        let blob_desc = BLOB_IO.registerBlob(blob);
        let blob_ptr = BLOB_IO.openBlob(blob_desc, 'r', wasmModule);
        let value = wasmModule.instance.exports.testReadToEOF(blob_ptr);
        BLOB_IO.closeBlob(blob_ptr, wasmModule);
        BLOB_IO.unregisterBlob(blob_desc);
        return (value === 0);
    }
    testList.push({test:testReadToEOF, name:'testReadToEOF'});

    function testReadToEOF(wasmModule){
        let blob = new Blob(["abcdefg"]);
        let blob_desc = BLOB_IO.registerBlob(blob);
        let blob_ptr = BLOB_IO.openBlob(blob_desc, 'r', wasmModule);
        let value = wasmModule.instance.exports.testReadBRead(blob_ptr);
        BLOB_IO.closeBlob(blob_ptr, wasmModule);
        BLOB_IO.unregisterBlob(blob_desc);
        return (value === 0);
    }
    testList.push({test:testReadToEOF, name:'testReadBRead'});



    function testWriteBWrite(wasmModule){
        let blob = new Blob([], {type:'text/plain'});
        // Register and write to blob IO.
        let blob_desc = BLOB_IO.registerBlob(blob);
        let blob_ptr = BLOB_IO.openBlob(blob_desc, 'w', wasmModule);
        wasmModule.instance.exports.testWriteBWrite(blob_ptr);
        blob = BLOB_IO.getBlob(blob_desc);
        // Clean up afterward.
        BLOB_IO.closeBlob(blob_ptr, wasmModule);
        BLOB_IO.unregisterBlob(blob_desc);
        const reader = new FileReaderSync();
        let results = reader.readAsText(blob);
        return (results === "abcd");
    }
    testList.push({test:testWriteBWrite, name:'testWriteBWrite'});

    function testWriteLongBWrite(wasmModule){
        let blob = new Blob([], {type:'text/plain'});
        // Register and write to blob IO.
        let blob_desc = BLOB_IO.registerBlob(blob);
        let blob_ptr = BLOB_IO.openBlob(blob_desc, 'w', wasmModule);
        wasmModule.instance.exports.testWriteLongBWrite(blob_ptr);
        blob = BLOB_IO.getBlob(blob_desc);
        // Clean up afterward.
        BLOB_IO.closeBlob(blob_ptr, wasmModule);
        BLOB_IO.unregisterBlob(blob_desc);
        const reader = new FileReaderSync();
        let results = reader.readAsText(blob);
        if (results.length !== 2000000){
            console.log("A",results.length);
            console.log(results);
            console.log(blob);
            return false;
        }
        for (var i = 0; i < results.length; i++){
            if (results[i] !== 'c'){
                console.log("B",results[i]);
                console.log(results);
                return false;
            }
        }
        return true;
    }
    testList.push({test:testWriteLongBWrite, name:'testWriteLongBWrite'});


    function testFreeLargePage(wasmModule){
        wasmModule.instance.exports.testFreeLargePage();
        return true;
    }
    testList.push({test:testFreeLargePage, name:'testFreeLargePage'});



    interface.run = function(wasmModule){
        for (var i = 0; i < testList.length; i++){
            let result = testList[i].test(wasmModule);
            let percentDone = (i + 1.0) / testList.length;
            handleTest(testList[i].name, result, percentDone);
        }
        self.postMessage({
            eventType:"FINISHED",
        });
    }
    return interface;
}());