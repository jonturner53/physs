class ModeLib {
    constructor() {
        this.spectrumModes = [];
        this.statusModes = [];
        this.timeSeriesModes = [];
        this.specialModes = [];
        this.libString = "";

        // these are used to control the timeSeries driver
        this.timeSeriesOn = false;
        this.timeSeriesEnabled = false;
        this.currentTimeSeries = 0;
    }

    get allModesString() {
        let s = ""; let n = 0;
        let pad = "========================================================" +
                  "======================\n";
        for (let i = 0; i < this.spectrumModes.length; i++) {
            if (n++ > 0) s += pad;
            s += this.spectrumModes[i].modeString;
        }
        for (let i = 0; i < this.timeSeriesModes.length; i++) {
            if (n++ > 0) s += pad;
            s += this.timeSeriesModes[i].modeString;
        }
        for (let i = 0; i < this.statusModes.length; i++) {
            if (n++ > 0) s += pad;
            s += this.statusModes[i].modeString;
        }
        for (let i = 0; i < this.specialModes.length; i++) {
            if (n++ > 0) s += pad;
            s += this.specialModes[i].modeString;
        }
        return s;
    }
    
    /** Parse a mode library.
     *  @param s is a string representing a mode library
     *  @return true on success, false on failure
     */
    parse(s) {
        this.spectrumModes = [];
        this.statusModes = [];
        this.timeSeriesModes = [];
        this.specialModes = [];
        this.libString = "";

        let modeStrings = s.split(/\n====*\n/);
        for (let i = 0; i < modeStrings.length; i++) {
            let mode = this.parseMode(modeStrings[i] + "\n");
            if (mode == null) return false;
        }
        this.libString = s;
        return true;
    }

    /** Process string to configure this Mode object.
     *  @param s is a string that represents a single mode
     *  @return a Mode object defined by s or null, in the event of
     *  an error
     */
    parseMode(s) {
        let status = true;
        let lineNumber = 0;
        let modeString = s;
        let modeName = "";
        let modeGroup = "";
        let labels = "";
        let prereqStrings = "";
        let userArgs = [];
        let userMenus = [];
        let chartFunctionString = "";
        let datapointFunctionString = "";
        let initFunctionString = "";

        let i = 0;
        while (i < s.length) {
            let j = s.indexOf("\n",i);
            let line = s.slice(i, (j >= i ? j : s.length));
            lineNumber++;
            let k = line.indexOf("//");
            if (k >= 0) line = line.slice(0,k).trim();
            if (line.length == 0) { i = j+1; continue; }
            let words = line.split(" ");
            let n = words.length;
            if (n == 0) { i = j+1; continue; }
            if (n < 2) {
                alert("Error in line " + lineNumber + " in mode " + modeName);
                return null;
            }
            switch (words[0]) {
            case "mode":
                modeName = words[1];
                for (let k = 2; k < words.length; k++)
                    modeName += " " + words[k];
                break;
            case "group":
                if (words[1] != "spectrum" && words[1] != "time-series" &&
                    words[1] != "status" && words[1] != "special") {
                    alert("Bad group name at line " + lineNumber +
                          " in mode " + modeName);
                    return null;
                }
                modeGroup = words[1];
                break;
            case "labels":
                labels = words.slice(1,n);
                break;
            case "prereqs":
                prereqStrings = words.slice(1,n);
                for (let ps = 0; ps < prereqStrings.length; ps++) {
                    if (prereqStrings[ps].search(/[^12]/) >= 0) {
                        alert("Bad prereq at line " + lineNumber +
                              " in mode " + modeName);
                        return null;
                    }
                }
                break;
            case "userArg":
                if (words.length < 2) {
                    alert("Error in line " + lineNumber +
                          " in mode " + modeName);
                    return null;
                }
                let value = (words.length >= 2 ? words.slice(2).join(" ") : "");
                userArgs.push([words[1], value]);
                break;
            case "userMenu":
                if (words[3] != "[[") {
                    alert("Error in line " + lineNumber +
                          " in mode " + modeName);
                    return null;
                }
                userMenus.push({name: words[1], currentValue: words[2],
                                choices: []});
                i = j+1;
                for (i = j+1; i < s.length; i = j+1) {
                    j = s.indexOf("\n",i);
                    line = s.slice(i, (j >= i ? j : s.length));
                    lineNumber++;
                    if (line == "]]") break;
                    let optionWords = line.trim().split(" ");
                    if (optionWords.length < 2) {
                        alert("Error in line " + lineNumber +
                              " in mode " + modeName);
                        return null;
                    }
                    if (optionWords.length < 2) continue;
                    let value = optionWords[0];
                    let label = optionWords.slice(1).join(" ");
                    userMenus[userMenus.length-1].choices.push(
                                            {value: value, label: label});
                }
                break;
            case "chart":
            case "datapoint":
            case "init":
                if (words[1] != "{{") {
                    alert("Error in line " + lineNumber +
                          " in mode " + modeName);
                    return null;
                }
                let codeString = "";
                i = j+1;
                while (i < s.length) {
                    j = s.indexOf("\n",i);
                    line = s.slice(i, (j >= i ? j : s.length));
                    lineNumber++;
                    if (line == "}}") {
                        if (words[0] == "chart") {
                            chartFunctionString = codeString;
                        } else if (words[0] == "init") {
                            initFunctionString = codeString;
                        } else if (words[0] == "datapoint") {
                            datapointFunctionString = codeString;
                        } else {
                            alert("Bad codeblock type at line " + lineNumber +
                                  " in mode " + modeName);
                            return null;
                        }
                        break;
                    }
                    codeString += line + "\n";
                    i = j+1;
                }
                break;
            default:
                alert("Error in line " + lineNumber +
                      " in mode " + modeName);
                return null;
            }
            i = j+1;
        }
        let mode = null;
        if (modeName == "") {
            alert("Error: missing modeName");
        } else if (modeGroup == "") {
            alert("Error: no group specified for mode " + modeName);
        } else if (modeGroup == "spectrum") {
            mode = new SpectrumMode(modeName, modeString, labels, prereqStrings,
                                    userMenus, userArgs, chartFunctionString);
            this.spectrumModes.push(mode);
        } else if (modeGroup == "status") {
            mode = new StatusMode(
						modeName, modeString, labels, prereqStrings,
                        userMenus, userArgs, chartFunctionString,
                        datapointFunctionString, initFunctionString);
            this.statusModes.push(mode);
        } else if (modeGroup == "time-series") {
            mode = new TimeSeriesMode(
						modeName, modeString, labels, prereqStrings,
                        userMenus, userArgs, chartFunctionString,
                        datapointFunctionString, initFunctionString);
            this.timeSeriesModes.push(mode);
        } else if (modeGroup == "special") {
            mode = new SpecialMode(modeName, modeString, initFunctionString);
            this.specialModes.push(mode);
        }
        return mode;
    }

    getMode(modeName, group) {
        if (group == "spectrum") {
            let lab = dset.label(dset.currSpecIndex);
            for (let i = this.spectrumModes.length-1; i >= 0; i--) {
                if (this.spectrumModes[i].name == modeName &&
                    (this.spectrumModes[i].hasLabel(lab) ||
                     this.spectrumModes[i].labels.length == 0)) {
                    return this.spectrumModes[i];
                }
            }
            return this.spectrumModes[0]; // default case
        } else if (group == "time-series") {
            for (let i = 0; i < this.timeSeriesModes.length; i++) {
                if (this.timeSeriesModes[i].name == modeName)
                    return this.timeSeriesModes[i];
            }
        } else if (group == "status") {
            for (let i = 0; i < this.statusModes.length; i++) {
                if (this.statusModes[i].name == modeName)
                    return this.statusModes[i];
            }
        } else if (group == "special") {
            for (let i = 0; i < this.specialModes.length; i++) {
                if (this.specialModes[i].name == modeName)
                    return this.specialModes[i];
            }
        }
        return null;
    }

    // methods to manage timeSeries computations

    /** Reset all the time-series modes */
    resetTimeSeries() {
        for (let i = 0; i < this.timeSeriesModes.length; i++ ) {
            this.timeSeriesModes[i].resetTimeSeries();
        }
        this.currentTimeSeries = 0;
    }

    startTimeSeries() {
        this.resetTimeSeries(); this.timeSeriesOn = true;
        this.timeSeriesEnabled = true;
        setTimeout(this.timeSeriesDriver.bind(this), 200);
    }

    stopTimeSeries() { this.timeSeriesOn = false; }

    // these are used to temporarily pause time-series computations
    pauseTimeSeries() { this.timeSeriesEnabled = false; }
    resumeTimeSeries() { this.timeSeriesEnabled = true; }

    /** Call time-series modes in turn, giving priority to the
     *  currently selected mode and equal opportunities to others.
     *  If time-series computation has been stopped, then just return
     *  and let modeLibMgr restart us when its ready.
     *  If time-series computation is paused, check periodically to
     *  see if we've been re-enabled.
     *  Take a short sleep after a chunk of work has been done.
     *  If no time-series work needs to be done, take a long sleep
     *  before checking again.
     */
    timeSeriesDriver() {
        if (!this.timeSeriesOn) return;
        if (!this.timeSeriesEnabled) {
            setTimeout(this.timeSeriesDriver.bind(this), 500); return;
        }

        let tsModes = this.timeSeriesModes;
        let currentMode = modeLibMgr.currentModeName;
		let currentGroup = modeLibMgr.currentGroupName;

        // first check for more work to do on current chart
        if (currentGroup == "time-series") {
            for (let i = 0; i < tsModes.length; i++) {
                if (currentMode == tsModes[i].name) {
                    if (!tsModes[i].computeDatapoints()) break;
                    modeLibMgr.drawChart();
                    setTimeout(this.timeSeriesDriver.bind(this), 200);
                    return;
                }
            }
        }

        // try to find another mode with more to do
        let count = 0; let something2do = true;
        let i = this.currentTimeSeries;
        while (!tsModes[i].computeDatapoints()) {
            i = (i == tsModes.length-1 ?  0 : i+1);
            if (++count >= tsModes.length) {
                this.currentTimeSeries = i;
                setTimeout(this.timeSeriesDriver.bind(this), 500);
                return;
            }
        }
        this.currentTimeSeries = (i == tsModes.length-1 ?  0 : i+1);
        setTimeout(this.timeSeriesDriver.bind(this), 200);
    }
}
