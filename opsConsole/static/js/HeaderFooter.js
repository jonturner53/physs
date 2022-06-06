
/** Get the string for the left footer and display it.  */
class HeaderFooter {
	constructor() {
		this.header = document.getElementById("header");
		this.leftFooter = document.getElementById("footerLeft");
		this.rightFooter = document.getElementById("footerRight");

		this.serialNumber = "";
		this.versionNumber = "";
		this.deploymentLabel = "";
		this.location = "";
		this.dateTime = "";
	}

	init() { }

	update(snap) {
		this.dateTime = snap.dateTime;
		this.location = snap.location;
		this.deploymentLabel = snap.deploymentLabel;
		this.serialNumber = snap.serialNumber;
		this.versionNumber = snap.versionNumber
	}

	show() {
		this.header.innerHTML = "PHySS " + this.serialNumber +
								" Operations Console" +
								" (v" + this.versionNumber + ")";
		this.leftFooter.innerHTML = this.location + " " + this.deploymentLabel;
		this.rightFooter.innerHTML = this.dateTime;
	}
}
