/** Snapshot object for managing retrieval of snapshots from fizz. */
class Snapshot {
	constructor() {
		this.snapFailures = 0;	// incremented when snapshot request fails
		this.commFailures = 0;	// incremented when snapshot request fails
		this.collectorConnected = false;
		this.fizzConnected = true;
		this.footerRight = document.getElementById("footerRight");
	}

	/** This method is called twice per second and retrieves snapshot
	 *  data from fizz, then updates user interface.
	 */
	snapit() {
		if (screenSaver.active) {
			setTimeout(this.snapit.bind(this), 2000);
			return;
		}
		screenSaver.tick();

		serverRequest("snapshot")
			.then(function(reply) {
					this.commFailures = 0;
					this.fizzConnected = true;
					if (reply.startsWith('snapshot reply ')) {
						this.processSnap(reply.slice(15));
						this.show();
						this.snapFailures = 0;
						this.collectorConnected = true;
						setTimeout(this.snapit.bind(this), 500);
					} else {
						if (this.snapFailures < 8) this.snapFailures++;
						if (this.snapFailures == 1)
							info.updateText(
								"console", "WARNING: lost snapshot\n");
						if (this.snapFailures < 7) {
							this.snapFailures++;
							setTimeout(this.snapit.bind(this), 2000);
						} else if (this.snapFailures == 7) {
							info.updateText(
								"console", "WARNING: collector disconnected\n");
							this.collectorConnected = false;
							setTimeout(this.snapit.bind(this), 5000);
						}
					}
				}.bind(this))
			.catch(function() {
					if (this.commFailures < 7) { 
						this.commFailures++;
						setTimeout(this.snapit.bind(this), 2000);
					} else {
						if (this.fizzConnected) {
							info.updateText(
								"console", "FATAL: lost contact with physs\n");
							this.fizzConnected = false;
							login.disconnect();
						}
						setTimeout(this.snapit.bind(this), 5000);
					}
				}.bind(this));
	}

	processSnap(snapshot) {
	   let snap = JSON.parse(snapshot);
	
		samplePump.update(snap.samplePump.toFixed(2));
		referencePump.update(snap.referencePump.toFixed(2));
		reagent1Pump.update(snap.reagent1Pump.toFixed(2));
		reagent2Pump.update(snap.reagent2Pump.toFixed(2));
	
		referenceSupply.update(snap.referenceSupply, referencePump.state);
		reagent1Supply.update(snap.reagent1Supply, reagent1Pump.state);
		reagent2Supply.update(snap.reagent2Supply, reagent2Pump.state);
	
		filterValve.update(snap.filterValve); portValve.update(snap.portValve);
		mix1Valve.update(snap.mixValves.charAt(0) == '0' ? 0 : 1);
		mix2Valve.update(snap.mixValves.charAt(1) == '0' ? 0 : 1);
	
		spectrometer.update(snap);
	
		fizz.update(snap);
		info.update(snap);

		xfer.update(snap);

		headerFooter.update(snap);
	}

	/** Show gui changes in all objects. */
	show() {
		screenSaver.show();
		if (screenSaver.active) return;
	
		fizz.show(); login.show(); xfer.show(); info.show(); 
	
		samplePump.show(); referencePump.show();
		reagent1Pump.show(); reagent2Pump.show();
	
		portValve.show(); filterValve.show();
		mix1Valve.show(); midValve.show(); mix2Valve.show();
	
		spectrometer.show();

		headerFooter.show();
	}
}
