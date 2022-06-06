class ScreenSaver {
	constructor() {
	    this.overlay = document.getElementById("screenSaver");
	    this.wakeupButton = document.getElementById("wakeupButton");
	    this.overlay.style.visibility = "hidden";
	    this.wakeupButton.style.visibility = "hidden";
	    
	    this.active = false;      // true when screenSaver activated
	    this.count = 0;            // incremented by update() method
	    this.limit = 2 * 15 * 60; // 15 minutes
	}

    reset() { this.count = 0; }
    
    /** Wakeup in response to button press.  */
    wakeup() { this.active = false; this.count = 0; this.show(); }

	/** advance timer */
    tick() {
        if (this.count < this.limit) {
            this.count++;
        } else if (login.inSession) {
            login.inOut(); this.active = true;
        }
        this.show();
    }

	/** Update gui */
    show() {
        if (this.active) {
            this.overlay.style.visibility = "visible";
            this.wakeupButton.style.visibility = "visible";
        } else {
            this.overlay.style.visibility = "hidden";
            this.wakeupButton.style.visibility = "hidden";
        }
    }
}

