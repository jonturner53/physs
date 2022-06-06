
/** Info object provides api for elements used to control the display
 *  of files.
 */
class Info {
	constructor() {
		this.selector = document.getElementById("fileSelector");
		this.logLevel = document.getElementById("logLevel");
		this.text = document.getElementById("infoText");
		this.text.value = "";
		this.consoleText = "";
		this.scriptText = "";
		this.configText = "";
		this.maintLogText = "";

		this.currentLine = 1;	// used to determine position of line marker
		
		this.shButton = document.getElementById("showHideButton");
		this.esrButton = document.getElementById("editSaveRefreshButton");
		this.configEditMode = false;
		this.scriptEditMode = false;
		this.maintLogEditMode = false;
		this.shFlag = true;  // show script comments when true, otherwise hide
	}

	/** Called by extractReply to save file text or console text */
	updateText(name, text) {
		if (name == "collector.script") {
			this.scriptText = text;
			this.text.value = this.formatScript(this.scriptText);
			return;
		} else if (name == "collector.config") {
			this.configText = text;
			this.text.value = this.configText;
			return;
		} else if (name == "collector.maintLog") {
			this.maintLogText = text;
			this.text.value = this.maintLogText;
			return;
		} else if (name != "console") {
			this.text.value = text;
			return;
		}
		this.consoleText += text;
		if (this.selector.value == "console") {
			this.text.value += text;
			this.text.scrollTop = this.text.scrollHeight;
		}
	}
	
	/** Retrieve and display the file in the fileText box.  */
	get() {
		serverRequest("read_" + this.selector.value)
			.then(function(reply) {
				if (!reply.startsWith("END_FILE")) {
					console.log("ERROR when retrieving file");
					return;
				}
			});
	}
	
	/** Show or hide the comments in the script.
	 *  Invoked by the show/hide button in the gui.
	 */
	enableComments() {
		screenSaver.reset();
		if (this.selector.value != 'collector.script' || this.scriptEditMode)
			return;
		this.shFlag = !this.shFlag;
		this.text.value = this.formatScript(this.scriptText);
	}
	
	/** Save, edit or refresh a display file.
	 *  Invoked by the edit/save/refresh button in the gui.
	 */
	editSaveRefresh() {
		screenSaver.reset();
		if (this.esrButton.innerHTML == 'refresh') {
			this.refresh();
		} else if (inControl()) {
			let request = new XMLHttpRequest();
			if (this.selector.value == 'collector.config') {
				if (!this.configEditMode) { // entering edit mode
					this.configEditMode = true;
					this.text.value = this.configText;
				} else { // saving file
					this.configEditMode = false;
					this.configText = this.text.value;
					if (this.configText[this.configText.length-1] != '\n')
						this.configText += '\n';
					request.open("POST", login.cmdPfx
									  + "write_collector.config", true);
					request.setRequestHeader("Content-type", "text/html");
					request.send('START_FILE\n' + this.configText +
								 'END_FILE\n');
					this.reload('config');
				}
			} else if (this.selector.value == 'collector.maintLog') {
				if (!this.maintLogEditMode) { // entering edit mode
					this.maintLogEditMode = true;
					this.text.value = this.maintLogText;
				} else { // saving file
					this.maintLogEditMode = false;
					this.maintLogText = this.text.value;
					if (this.maintLogText[this.maintLogText.length-1] != '\n')
						this.maintLogText += '\n';
					request.open("POST", login.cmdPfx
									  + "write_collector.maintLog", true);
					request.setRequestHeader("Content-type", "text/html");
					request.send('START_FILE\n' + this.maintLogText +
								 'END_FILE\n');
					this.reload('maintLog');
				}
			} else if (this.selector.value == 'collector.script') {
				if (!this.scriptEditMode) { // entering edit mode
					this.scriptEditMode = true;
					this.shFlag = true;
					this.text.value = this.scriptText;
				} else { // saving file
					this.scriptEditMode = false;
					this.scriptText = this.text.value
					this.text.value = this.formatScript(this.scriptText);
					if (this.scriptText[this.scriptText.length-1] != '\n')
						this.scriptText += '\n';
					request.open("POST", login.cmdPfx
									  + "write_collector.script", true);
					request.setRequestHeader("Content-type", "text/html");
					request.send('START_FILE\n' + this.scriptText +
								 'END_FILE\n');
					this.reload('script');
				}
			}
		}
	}
	
	/** Signal collector to reload a file, following a save operation.
	 */
	reload(filename) {
		screenSaver.reset();
		if (!inControl()) return;
		serverRequest("reload_" + filename);
	}
	
	/** Refresh the display file area to show console log or selected file.
	 *  Show script comments if the shFlag is true, otherwise hide them.
	 *  Mark the current line in the script.
	 */
	refresh() {
		if (this.selector.value == "console") {
			this.text.value = this.consoleText;
			this.text.scrollTop = this.text.scrollHeight;
		} else {
			this.get();
		}
	}
	
	/** Format a script for display in the fileText area.
	 *  @param script is the plain script file
	 *  @return a string containing the formatted script
	 */
	formatScript(script) {
		let i = 0;   // start if currentLine
		let n = 1;   // index of current line
		let fs = '';
		while (i < script.length) {
			let j = script.indexOf('\n',i);
			let line = script.slice(i,j+1);
			if (!this.shFlag) {
				let k = line.indexOf('#');
				if (k >= 0) line = line.slice(0,k-1) + '\n';
				let s = line.trim();
				if (s.length == 0) line = '';
			}
			if (line.length > 0) {
				if (n == this.currentLine) fs += '=>' + line;
				else				  fs += '  ' + line;
			}
			i = j + 1;
			n++;
		}
		return fs;
	}

	/** Retrieve the current log level and display it. */
	getLevel() {
		serverRequest("logLevel")
			.then(function(reply) {
				if (reply.startsWith("log level")) {
					let i = reply.indexOf(" is ", 9);
					this.logLevel.value = reply.slice(i+4)
				}
			}.bind(this));
	}
	
	/** Set the log level based in response to change in logLevel selector.
	 */
	setLevel() {
		screenSaver.reset();
		if (!login.inSession) return;
		serverRequest("logLevel_" + this.logLevel.value)
			.then(function(reply) {
				if (reply.startsWith("setting log level")) {
					let i = reply.indexOf(" to ", 17);
					this.logLevel.value = reply.slice(i+4)
				}
			}.bind(this));
	}

	show() {
		let flag = inControl();
	
		this.esrButton.style.backgroundColor = 
			this.selector.value == "console" ? "LightGray" : "beige";
		this.text.style.backgroundColor = 'white';
		this.shButton.style.backgroundColor = "LightGray";
		this.shButton.innerHTML = (this.shFlag ? 'hide' : 'show');

		if (this.selector.value == 'collector.script') {
			if (flag && this.scriptEditMode) {
				this.text.disabled = false;
			} else {
				this.text.style.backgroundColor = 'seashell';
				this.text.disabled = true;
				this.shButton.style.backgroundColor = "beige";
			}
			this.esrButton.innerHTML =
				(flag && this.scriptEditMode ? 'save' :
				 (flag && !this.scriptEditMode ? 'edit' : 'refresh'));
		} else if (this.selector.value == 'collector.config') {
			if (flag && this.configEditMode) {
				this.text.disabled = false;
			} else {
				this.text.style.backgroundColor = 'seashell';
				this.text.disabled = true;
			}
			this.esrButton.innerHTML =
				(flag && this.configEditMode ? 'save' :
				 (flag && !this.configEditMode ? 'edit' : 'refresh'));
		} else if (this.selector.value == 'collector.maintLog') {
			if (flag && this.maintLogEditMode) {
				this.text.disabled = false;
			} else {
				this.text.style.backgroundColor = 'seashell';
				this.text.disabled = true;
			}
			this.esrButton.innerHTML =
				(flag && this.maintLogEditMode ? 'save' :
				 (flag && !this.maintLogEditMode ? 'edit' : 'refresh'));
		} else {
			this.text.disabled = true;
			this.esrButton.innerHTML = 'refresh';
		}
	}

	update(snap) {
		let oldLine = this.currentLine;
		this.currentLine = snap.currentLine;
		if (this.currentLine != oldLine &&
			this.selector.value == 'collector.script' && !this.editMode) {
			this.refresh();
		}
		this.logLevel.value = snap.logLevel;
	}
}

