/** Login class provides api for login controls. */
class Login {
	constructor() {
		this.inSession = false
		this.sessionCode = "0000000000";
		this.loginIcon = document.getElementById("loginIcon");
		this.loginArea = document.getElementById("loginArea");
		this.loginButton = document.getElementById("loginButton");
		this.newpassButton = document.getElementById("newpassButton");
		this.userNameBox = document.getElementById("userNameBox");
		this.passwordBox = document.getElementById("passwordBox");
	}

	get cmdPfx() { return "/opsConsole_127_" + this.sessionCode + "_"; }

	/** Login or logout.  */
	inOut() {
		if (!this.inSession) {
			let uname = userNameBox.value;
			let pword = passwordBox.value;
			serverRequest("login_" + this.userNameBox.value + "_" +
						  this.passwordBox.value)
				.then(function(reply) {
					if (reply.startsWith("session accepted ")) {
						this.inSession = true;
						this.sessionCode = reply.slice(17);
						this.show(); fizz.show();
					}
				}.bind(this));
		} else {
			serverRequest("logout");
			this.inSession = false; this.sessionCode = "0000000000";
			this.show(); fizz.show();
		}
	}

	/** Update status reflect disconnection from physs. */
	disconnect() {
		this.inSession = false; this.sessionCode = "0000000000";
        this.show(); fizz.show();
	}

	showHide() {
		if (loginArea.style.visibility == "hidden")
			loginArea.style.visibility = "visible";
		else
			loginArea.style.visibility = "hidden";
	}
	
	/** Change the current password.  */
	newpass() {
		screenSaver.reset();
		if (!this.inSession) {
			info.updateText("console", "must be logged in to change password");
			return;
		}
		serverRequest("newpass_" + this.userNameBox.value + "_" +
								   this.passwordBox.value);
	}

	/** Update gui components */
	show() {
		this.loginButton.style.backgroundColor = "beige";
		if (this.inSession) {
			this.loginButton.innerHTML = "logout";
			this.newpassButton.style.backgroundColor = "beige";
		} else {
			this.loginButton.innerHTML = "login";
			this.newpassButton.style.backgroundColor = "LightGray";
		}
	}
}
