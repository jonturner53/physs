
/** Spectrometer object provides api for controls related to spectrometer.  */
class Spectrometer {
	constructor() {
		this.deutButton = document.getElementById("deutButton");
		this.tungButton = document.getElementById("tungButton");
		this.shutButton = document.getElementById("shutButton");
		this.intTime = document.getElementById("intTime");
		this.lights = { state: '000' }
	}

	/** Retrieve the current integration time and display it.
	 */
	getIntTime() {
		serverRequest("integrationTime")
		.then(function(reply) {
			if (reply.startsWith("integration time")) {
				var i = reply.indexOf(" is ", 16);
				this.intTime.value = reply.slice(i+4)
			}
		}.bind(this));
	}
	
	/** Set the integration time in response to change in intTime input box.
	 */
	setIntTime() {
		screenSaver.reset();
		if (!inControl()) return;

		serverRequest("integrationTime_" + intTime.value)
		.then(function(reply) {
			if (reply.startsWith("setting integration time")) {
				let i = reply.indexOf(" to ", 24);
				this.intTime.value = reply.slice(i+4)
			}
		}.bind(this));
	}

	setLights(button) {
		screenSaver.reset();
		if (!inControl()) return;

		// on button push, send lights command to collector,
		// flipping state for selected button while maintaining the rest.
		let state = this.lights.state;
		if (button == this.deutButton)
			state = (state[0] == '0' ? '1' : '0') + state[1] + state[2];
		else if (button == this.tungButton)
			state = state[0] + (state[1] == '0' ? '1' : '0') + state[2];
		else if (button == this.shutButton)
			state = state[0] + state[1] + (state[2] == '0' ? '1' : '0');

		serverRequest("lights_" + state)
		.then(function(reply) {
			if (reply.startsWith("light status is now")) {
				this.lights.state = reply.slice(-3);
				this.show();
			}
		}.bind(this));
	}
	
	show () {
		this.deutButton.style.backgroundColor =
			this.lights.state[0] == '0' ? 'black' : 'yellow';
		this.tungButton.style.backgroundColor =
			this.lights.state[1] == '0' ? 'black' : 'yellow';
		this.shutButton.style.backgroundColor =
			this.lights.state[2] == '0' ? 'black' : 'yellow';
	}

	update(snap) {
		this.lights.state = snap.lights;
		if (!inControl())
			this.intTime.value = snap.integrationTime.toFixed(
									snap.integrationTime < 99.5 ? 1 : 0);
	}
}

