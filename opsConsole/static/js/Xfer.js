
/** Class for data transfer controls. */
class Xfer {
	constructor() {
	    this.xferButton = document.getElementById("xferButton");
	    this.state = 0; // 3=ON/on, 0=OFF/off, 1=OFF/on, 2=ON/off
	    this.nextRecordText = document.getElementById("nextRecordText");
	    this.nextRecord = 0; // next record to be transferred to coolcloud
	}

    /** Enable/disable data transfer from physs to coolcloud */
    enableDisable() {
        screenSaver.reset();
        if (!login.inSession) return;
        serverRequest(this.state&1 ? "disableXfer" : "enableXfer");
    }
    
    update(snap) {
        this.state = snap.xferState; this.nextRecord = snap.nextRecord;
    }

    show() {
        if (this.state == 0) {
            this.xferButton.style.backgroundColor = "white";
        } else if (this.state == 3) {
            this.xferButton.style.backgroundColor = "yellow";
        } else {
            this.xferButton.style.backgroundColor = "LightGray";
        }
        this.nextRecordText.innerHTML = "next record " + this.nextRecord;
    }
}
