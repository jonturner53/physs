
/** Constructor for Supply objects.
 *  @param name is a string specifying the supply name.
 */
class Supply {
	constructor(name) {
		this.name = name;
		this.level = "500";
		this.maxLevel = "500";
		this.textBox = document.getElementById(name);
	}

	/** Get the level for the supply reservoir.  */
	getLevel() {
		serverRequest(this.name)
		.then(function(reply) {
			if (reply.startsWith("fluid level")) {
				let i = reply.indexOf(" is ", 11);
				let j = reply.indexOf("ml",i);
				this.level = reply.slice(i+4,j-1);
				this.textBox.value = this.level;
			}
		}.bind(this));
	}
	
	/** Change the level for  the supply reservoir.  */
	changeLevel() {
		screenSaver.reset();
		if (!inControl()) return;

		serverRequest(this.name + '_' + this.textBox.value)
		.then(function(reply) {
			if (reply.startsWith("setting")) {
				let i = reply.indexOf(" to ");
				let j = reply.indexOf("ml",i);
				this.level = reply.slice(i+4,j-1);
				this.textBox.value = this.level;
			} else {
				this.textBox.value = this.level;
			}
		}.bind(this));
	}

	update(level, pumpState) {
		if (!inControl() || pumpState == 1) {
			this.level = level; this.textBox.value = level;
		}
	}
}

