/** Constructor for Pump object.
 *  @param name is a string that is used as the pump name
 */
class Pump {
	constructor(name) {
	    this.name = name;
	    this.state = 0;
	    this.stateButton = document.getElementById(name + "State");
	    this.rate = "1.000";          // current rate
	    this.rateBox = document.getElementById(name + "Rate");
	    this.calibrationInProgress = false;
	    this.calibrateButton = document.getElementById(name + "Calibrate");
	}

    /** Respond to button push by turning pump on or off.  */
    changeState() {
        screenSaver.reset();
        if (!inControl()) return;

        let cmd = this.name + (this.state == 0 ? "_on_" : "_off_") +
                  this.rateBox.value;

		serverRequest(cmd)
			.then(function(reply) {
	            if (reply.startsWith("turning on")) {
	                let i = reply.indexOf("rate");
	                let j = reply.indexOf("ml/m",i);
	                this.rate = reply.slice(i+5,j-1);
	                this.state = 1;
	            } else if (reply.startsWith("turning off")) {
	                this.state = 0;
	            }
			}.bind(this))
    }

    /** Change the pump rate value, activated by rate box change.  */
    changeRate() {
        screenSaver.reset();
        if (!inControl() || this.state == 0) return;

        serverRequest(this.name + "_on_" + this.rateBox.value)
		.then(function(reply) {
            if (reply.startsWith("turning on")) {
                let i = reply.indexOf("rate");
                let j = reply.indexOf("ml/m",i);
                this.rate = reply.slice(i+5,j-1);
        		this.rateBox.value = this.rate;
                this.state = (this.rate == 0 ? 0 : 1);
            }
        }.bind(this));
    }
    
    calibrate() {
        screenSaver.reset();
        if (!inControl()) return;

        let cmd = "";
        if (!this.calibrationInProgress) {
            cmd = this.name + "_calibrateStart";
            this.calibrationInProgress = true;
        } else {
            cmd = this.name + "_calibrateFinish";
        }

        serverRequest(cmd)
		.then(function(reply) {
            if (!reply.startsWith("starting"))
                this.calibrationInProgress = false;
        }.bind(this));
    }

    /** Update the gui to reflect the pump control variables. */
    show() {
        if (this.state == 0) {
            this.stateButton.style.backgroundColor = "black";
        } else {
            this.stateButton.style.backgroundColor = "yellow";
        }
        if (this.calibrationInProgress) {
            this.calibrateButton.style.backgroundColor = "red";
        } else {
            this.calibrateButton.style.backgroundColor = "beige";
        }
        if (!inControl()) this.rateBox.value = this.rate;
    };

    update(rate) {
        this.state = (rate == 0. ? 0 : 1);
        if (!inControl()) this.rate = rate;
    }
}

