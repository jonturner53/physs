/** Valve class.
 *  @param name is the name for this particular valve;
 *  must be one of filterValve, portValve, mix1Valve, mix2Valve, midValve;
 *  behavior varies based on which valve it is.
 */
class Valve {
	constructor(name) {
		this.name = name;
		this.state = 0;
		this.image = document.getElementById(name);
	}

	/** Retrieve the state of a valve and show it.  */
	get() {
		let cmd = (this == filterValve || this == portValve ? this.name :
										  "mixValves");
		serverRequest(cmd)
		.then(function(reply) {
			if (reply.startsWith("state is ")) {
				if (this == filterValve || this == portValve) {
					this.state = (reply.slice(-1) == '0' ? 0 : 1);
					this.show();
				} else {
					mix1Valve.state = (reply.slice(-2,-1) == '0' ? 0 : 1);
					mix2Valve.state = (reply.slice(-1) == '0' ? 0 : 1);
					mix1Valve.show();
					mix2Valve.show();
					midValve.show();
				}
			}
		}.bind(this));
	};
	
	/** Flip the state of a valve. */
	flip() {
		screenSaver.reset();
		if (!inControl()) return;

		let cmd;
		if (this == filterValve || this == portValve) {
			cmd = this.name + '_' + (this.state == 0 ? '1' : '0');
		} else if (this == mix1Valve) {
			cmd = "mixValves_" + (mix1Valve.state == 0 ? '1' : '0') 
								+  mix2Valve.state;
		} else if (this == mix2Valve) {
			cmd = "mixValves_" +  mix1Valve.state
								+ (mix2Valve.state == 0 ? '1' : '0');
		}
		serverRequest(cmd)
		.then(function(reply) {
			if (reply.startsWith("setting valve state")) {
				if (this == filterValve || this == portValve) {
					this.state = (reply.slice(-1) == '0' ? 0 : 1);
					this.show();
				} else {
					mix1Valve.state = (reply.slice(-2,-1) == '0' ? 0 : 1);
					mix2Valve.state = (reply.slice(-1) == '0' ? 0 : 1);
					mix1Valve.show(); mix2Valve.show(); midValve.show();
				}
			}
		}.bind(this));
	}

	/** Display the valves in the gui according to their states.  */
	show() {
		if (fizz.hardwareConfig == "BASIC" && 
			(this == mix1Valve || this == mix2Valve || this == midValve)) {
			this.image.style.visibility = "hidden";
		} else if (this == midValve) {
			this.image.style.visibility
				= (mix1Valve.state == mix2Valve.state ? "hidden" : "visible");
		} else {
			this.image.style.visibility
				= (this.state == 0 ? "hidden" : "visible");
		}
	}

	update(state) { this.state = state; };
}

