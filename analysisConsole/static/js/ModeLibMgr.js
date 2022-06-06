class ModeLibMgr {
    constructor(userArgsArea) {
        this.userArgsArea = userArgsArea;
        this.currlib = null;
        this.userlib = null;

        this.modeGroup = {};
        this.spectrumModes = "";
        this.timeSeriesModes = "";
        this.statusModes = "";
    }

    init() {
        this.getModeLibNames();
    }

    /** Find the mode in the current library that matches a
     *  specified chart mode and (where relevant) the current spectrum label.
     */
    get currentMode() {
        let modeName = this.currentModeName;
        let group = this.currentGroupName;
        return (this.currlib == null ? null :
                this.currlib.getMode(modeName, group));
    }

    get currentModeName() { return main.currentMode; }
    get currentGroupName() { return this.modeGroup[main.currentMode]; }
    get currentLibName() { return main.currentModeLib; }

    currentModeString() {
        let mode = this.currentMode;
        return (mode == null ? "" : mode.modeString);
    }

    initModeString() {
        let mode = (this.currlib == null ? null :
                    this.currlib.getMode("initialize", "special"));
        return (mode == null ? null : mode.modeString);
    }

    allModesString() {
        return (this.currlib == null ? "" :
                this.currlib.allModesString);
    }

    modeOutputText() {
        let mode = this.currentMode;
        return (mode == null ? "" : mode.outString);
    }

    /** Sets the user arguments for the current mode and assigns
     *  initial values. Called when current mode changes.
     */
    setUserArgs() {
        let mode = this.currentMode;
        let userMenus = mode.userMenus;
        let s = "";
        for (let i = 0; i < userMenus.length; i++) {
            let menu = userMenus[i];
			let name = (menu.name[0] == '.' ? menu.name.slice(1) : menu.name);
            s += '<div style="font-family: Verdana; font-size: 100%; ' +
                 'position: absolute; top:' + (22*i + 4) + 'px; right:4px;">' +
                 name + "&nbsp" + '<select id="userMenu_' + i +
                 '" onchange="modeLibMgr.updateUserMenu(' + i + ');" ' +
                 'style="font-size: 100%;">';
            for (let j = 0; j < menu.choices.length; j++) {
                let choice = menu.choices[j];
                s += '<option value="' + choice.value + '"';
                if (choice.value == menu.currentValue) s += ' selected';
                s += '>' + choice.label + '</option>';
            }
            s += '</select></div><br>';
        }
        let userArgs = mode.userArgs;
        for (let i = 0; i < userArgs.length; i++) {
			let name = userArgs[i][0];
			if (name[0] == '.') name = name.slice(1);
            s += '<div style="font-family: Verdana; font-size: 100%; ' +
                 'position: absolute; top:' +
                 (22*(i+userMenus.length) + 6) + 'px; right:4px;">' +
                 name + '&nbsp;' +
                 '<input type="text" size="10" style="height:20px; ' +
                 'font-size: 100%;" ' +
                 'value="' + userArgs[i][1] + '" id="userArg_' + i + '" ' +
                 'onchange="modeLibMgr.updateUserArg(' + i + ');"><br>' +
                 '</div>';
        }
        this.userArgsArea.innerHTML = s;
    }

    /** Respond to a change in a user argument.  */
    updateUserArg(i) {
        this.currentMode.updateUserArg(i);
        main.userArgChanged();
    }

    /** Respond to a change in a user menu.  */
    updateUserMenu(i) {
        this.currentMode.updateUserMenu(i);
        main.userArgChanged();
    }

    startTimeSeries() {
        if (this.currlib != null) this.currlib.startTimeSeries();
    }
    stopTimeSeries() {
        if (this.currlib != null) this.currlib.stopTimeSeries();
    }
    pauseTimeSeries() {
        if (this.currlib != null) this.currlib.pauseTimeSeries();
    }
    resumeTimeSeries() {
        if (this.currlib != null) this.currlib.resumeTimeSeries();
    }

    /** Get list of mode library names from server and the first library.
     */
    getModeLibNames() {
        serverRequest("getModeLibNames")
        .then(function(reply) {
            let libNames = JSON.parse(reply);
            libNames.push("user");
            main.reportModeLibNames(libNames);
            if (reply.indexOf("\"standard\"") >= 0) {
                main.currentModeLib = "standard";
            }
            this.getModeLib(main.currentModeLib);
        }.bind(this));
    }

    /** Retrieve a library from the server and make it the current library.
     *  @param modeLibName is the name of the library to be retrieved.
     */
    getModeLib(modeLibName) {
        this.stopTimeSeries();
        serverRequest("getModeLib_" + modeLibName, [modeLibName])
        .then(function([reply, modeLibName]) {
            let modeLib = new ModeLib();
            if (modeLib.parse(reply)) {
                main.currentModeLib = modeLibName;
                this.update(modeLib);
            }
        }.bind(this));
    }


    /** Switch to a specified mode library.
     *  Replaces current mode library with the specified one.
     *  @param nulib is a mode library
     */
    update(nulib) {
        this.stopTimeSeries();
        let currentModeName = this.currentModeName;
        let gotMatch = false;
        this.currlib = nulib;

        let modeList = [];
        for (let i = 0; i < nulib.spectrumModes.length; i++) {
            modeList.push([nulib.spectrumModes[i].name, "spectrum"]);
            this.modeGroup[nulib.spectrumModes[i].name] = "spectrum";
            if (nulib.spectrumModes[i].name == currentModeName)
                gotMatch = true;
        }
        for (let i = 0; i < nulib.timeSeriesModes.length; i++) {
            modeList.push([nulib.timeSeriesModes[i].name, "time-series"]);
            this.modeGroup[nulib.timeSeriesModes[i].name] = "time-series";
            if (nulib.timeSeriesModes[i].name == currentModeName)
                gotMatch = true;
        }
        for (let i = 0; i < nulib.statusModes.length; i++) {
            modeList.push([nulib.statusModes[i].name, "status"]);
            this.modeGroup[nulib.statusModes[i].name] = "status";
            if (nulib.statusModes[i].name == currentModeName)
                gotMatch = true;
        }
        main.reportModes(modeList);
        if (gotMatch) main.currentMode = currentModeName;

        main.swapSupportLib(main.currentModeLib);
                    // must come before initialize call so that
                    // global lib refers to support library for
                    // new mode library, during initialization

        for (let i = 0; i < nulib.specialModes.length; i++) {
            if (nulib.specialModes[i].name == "initialize")
                nulib.specialModes[i].initialize();
        }

        this.setUserArgs();
        main.newModeLib();
    }

    lockPages() {
        info.lock("currMode");
        info.lock("initMode");
        info.lock("allModes");
    }

    unlockPages() {
        info.unlock("currMode");
        info.unlock("initMode");
        info.unlock("allModes");
    }

    /** Respond to change in modeLibMenu.
     */
    changeModeLib() {
        this.stopTimeSeries();
        if (main.currentModeLib != "user") {
            this.getModeLib(main.currentModeLib);
            this.lockPages();
            return;
        }
        this.unlockPages();
        if (this.userlib != null) this.update(this.userlib);
        return;
    }

    drawChart() {
        let mode = this.currentMode;
        if (mode != null) {
            mode.drawChart();
        }
    }

    get userlibString() {
        return user_lib_string;
    }

    /** Import the user library.
     *  If all modes are successfully parsed, update internal version of
     *  user library and make it the current library.
     */
    import() {
        if (this.currlib != this.userlib) {
            alert("import is ignored sinced user lib is not selected");
            return;
        }
        let s = "";
        let currentPage = info.currentPage;
        if (currentPage == "currMode") {
            this.currentMode.modeString = info.pageText;
            s = this.userlib.allModesString;
            info.refresh("currMode");
        } else if (currentPage == "initMode") {
            let thisMode = this.currlib.getMode("initialize", "special");
            thisMode.modeString = info.pageText;
            s = this.userlib.allModesString;
            info.refresh("initMode");
        } else if (currentPage == "allModes") {
            s = info.pageText;
            info.refresh("allModes");
        } else {
            return;
        }
        let nulib = new ModeLib();
        if (!nulib.parse(s)) return;
        this.userlib = nulib;
        this.update(this.userlib);
        main.currentModeLib = "user";
    }

    /** Export the current library to the user library.
     *  Make the user library the current library.
     */
    export() {
        this.userlib = new ModeLib();
        this.userlib.parse(this.currlib.allModesString);
        this.currlib = this.userlib;
        main.currentModeLib = "user";
        this.unlockPages();
    }
}
