const worker = new Worker('test_worker.js');
worker.addEventListener('message', handleWorkerUpdate);

const actionButton = document.getElementById("button");
const actionText = document.getElementById("buttonText");
const resultsText = document.getElementById("results");

actionButton.addEventListener("click", handleAction);

function handleAction(event) {
    if (actionText.innerText == "Run Tests") {
        worker.postMessage({eventType: "SUBMIT", eventData: {}});
        actionButton.className = "form-button";
        actionText.innerText = "Running Tests";
    }
}

function handleWorkerUpdate(event) {
    const { eventType, eventData, eventId } = event.data;
    if (eventType === "FINISHED"){
        actionButton.classList.add("hoverable");
        document.getElementById("loadingBar").style.width = "100%";
        document.getElementById("loadingBar").style.background = "transparent";
        actionText.innerText = "Run Tests";
    } else if (eventType === "TESTCASE"){
        document.getElementById("loadingBar").style.width = eventData.progress.toString()+"%";
        let passedString = "FAIL";
        if (eventData.passed){
            passedString = "OK";
        }
        resultsText.innerText = resultsText.innerText + eventData.name.toString() + ": "+passedString + "\n";
    }
}
