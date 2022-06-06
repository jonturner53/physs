
/** Fizz object provides api for a miscellaneous controls.  */
class Fizz  {
	constructor() {
		this.cycleNumber = document.getElementById("cycleNumber");
	
		this.startStopButton = document.getElementById("startStopButton");
		this.resumeButton = document.getElementById("resumeButton");
		this.quitButton = document.getElementById("quitButton");
		this.clearButton = document.getElementById("clearButton");
		
		this.pumpValvePowerButton =
				document.getElementById("pumpValvePowerButton");
		this.lightSourcePowerButton =
				document.getElementById("lightSourcePowerButton");
		
		this.samplingEnabled = 0;
	
		this.power = { state: '00' };
		this.temperature = document.getElementById("temperature");
		this.depth = document.getElementById("depth");
		this.leak = document.getElementById("leak");
		this.batteryVoltage = document.getElementById("batteryVoltage");
		this.filterPressure = document.getElementById("filterPressure");
		this.maxPressure = document.getElementById("maxPressure");
		this.location = document.getElementById("location");
	
		this.hardwareConfig = "BASIC";
		this.basicConfigImage = document.getElementById("basicConfigImage");
		this.twoReagentsStuff = document.getElementById("twoReagentsStuff");

		this.commandArea = document.getElementById("commandArea");
		this.commandLine = document.getElementById("commandLine");
	}

	showHideCommand() {
		this.commandArea.style.visibility = 
			this.commandArea.style.visibility == "hidden" ?
			"visible" : "hidden";
	}

	/** Send a command to the physs. Must be logged in and not
	 *  collecting samples.
	 */
	enterCommand() {
		screenSaver.reset();
		if (!login.inSession) {
			info.updateText('console',
							'must be logged in to send text commands\n');
			return;
		}
	
		serverRequest(commandLine.value.trim().replace(/ +/g, "_"));
	}

	/** Clear the debug and data files on the physs. */
	clear() {
		screenSaver.reset();
		if (login.inSession && snapshot.fizzConnected)
			serverRequest("clear");
	}

	/** Change the run state of the collector.
	 *  @param button is one of the gui buttons used to control the state
	 */
	newState(button) {
		screenSaver.reset();
		if (!login.inSession || !snapshot.collectorConnected)
			return;
		serverRequest(button.innerHTML);
	}

	/** Update power state in response to button press.
	 *  @param button is the button that was pressed.
	 */
	updatePower(button) {
		screenSaver.reset();
		if (!inControl()) return;
	
		// on button push, send power control command to collector,
		// flipping state for selected button while maintaining the rest.
		let state = this.power.state;
		if (button == this.pumpValvePowerButton)
			state = (state[0] == '0' ? '1' : '0') + state[1];
		else if (button == this.lightSourcePowerButton)
			state = state[0] + (state[1] == '0' ? '1' : '0');
		serverRequest('power_' + state)
		.then(function(reply) {
			if (reply.startsWith("setting power to ")) {
				this.power.state = reply.slice(-3);
				this.show();
			}
		}.bind(this));
	}

	/** Display all the Fizz object widgets on the gui. */
	show() {
		this.startStopButton.innerHTML = 
			this.samplingEnabled ? "stop" : "start";
	
		if (!login.inSession || !snapshot.fizzConnected) {
			this.startStopButton.style.backgroundColor = "LightGray";
			this.resumeButton.style.backgroundColor = "LightGray";
			this.quitButton.style.backgroundColor = "LightGray";
		} else {
			this.startStopButton.style.backgroundColor = "beige";
			this.quitButton.style.backgroundColor = "beige";
			this.resumeButton.style.backgroundColor =
				this.samplingEnabled ? "LightGray" : "beige";
		}

		this.clearButton.style.backgroundColor =
			(inControl() ? "beige" : "LightGray");

		this.pumpValvePowerButton.style.backgroundColor =
			this.power.state[0]=='0' ? 'black' : 'yellow';
		this.lightSourcePowerButton.style.backgroundColor =
			this.power.state[1]=='0' ? 'black' : 'yellow';

		if (this.hardwareConfig == "BASIC") {
			this.basicConfigImage.style.visibility = "visible";
			this.twoReagentsStuff.style.visibility = "hidden";
		} else {
			this.basicConfigImage.style.visibility = "hidden";
			this.twoReagentsStuff.style.visibility = "visible";
		}
	}

	/** This function is called by Snapshot object to update gui. */
	update(snap) {
		this.cycleNumber.innerHTML = "cycle: " + snap.cycleNumber;
		this.samplingEnabled = snap.samplingEnabled; 
		this.hardwareConfig = snap.hardwareConfig; 
	
		this.power.state = snap.power;
		this.filterPressure.innerHTML = snap.filterPressure.toFixed(2);
		this.maxPressure.innerHTML = snap.maxPressure.toFixed(2);
		this.temperature.innerHTML =
			"temperature: " + snap.temperature.toFixed(1) + "C";
		this.depth.innerHTML = "depth: " + snap.depth.toFixed(1) + "M";
		this.leak.innerHTML = snap.leak ? "leak detected!!!" : "no leak";
		this.batteryVoltage.innerHTML =
			"battery: " + snap.batteryVoltage.toFixed(1)+ "V";
	}
}
