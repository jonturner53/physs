/** Curve objects define a single curve and its parameters. */
function Curve(xvec, yvec, color, style) {
    this.xvec = xvec; this.yvec = yvec;
    this.color = color; this.style = style;
};

class Mode {
    constructor(name, group, mode_string,
                labels="", prereqStrings="", userMenus=[], userArgs=[]) {
        this.name = name;
        this.group = group;
        this.mode_string = mode_string;
        this.labels = labels;
        this.prereqStrings = prereqStrings;
        this.user_args = userArgs;   // pairs: [name, current value]
        this.user_menus = userMenus; // objects: { name,current value,options }

        this.prereq = [];
        this.currentSpectrum = null;

        this.x = new AxisSpec();
        this.y = new AxisSpec();
        this.markup = new ChartMarkup();
        this.timeSpan = "0 0";
        for (let i = 0; i < userArgs.length; i++)
            if (userArgs[i][0] == 'timeSpan')
                this.timeSpan = userArgs[i][1];

        this.out_string = "";
        this.qa = -2;

        this.boundAddCurve = this.addCurve.bind(this);
        this.boundAddLine = this.addLine.bind(this);
        this.boundOut = this.out.bind(this);
        this.boundSetUserMenu = this.setUserMenu.bind(this);
    }

    inRange(t) {
        let words = this.timeSpan.trim().split(" ");
        let tmin = parseInt(words[0]); let tmax = parseInt(words[1]);
        let endTime = (dset.endTime - dset.startTime) / 3600;
        if (tmin < 0) {
            tmin = Math.max(0, endTime + tmin);
        }
        if (tmax <= 0) {
            tmax = Math.max(0, endTime + tmax);
        }
        tmax = Math.max(tmin+1, tmax);
        return tmin <= t && t <= tmax;
    }

    get userArgsString() {
        let cs = '\n'; let ds = '';
        for (let i = 0; i < this.user_menus.length; i++) {
            let name = this.user_menus[i].name;
            if (name[0] == '.') {
                cs += '    let ' + name.slice(1) + ' = ' +
                      'this.user_menus[' + i + '].currentValue;\n';
                name = name.slice(1);
            } else {
                cs += '    let ' + name + ' = ' +
                      'this.user_menus[' + i + '].currentValue;\n';
                ds += '    let ' + name + ' = ' +
                      'this.user_menus[' + i + '].currentValue;\n';
            }
        }
        for (let i = 0; i < this.userArgs.length; i++) {
            let name = this.user_args[i][0];
            if (name[0] == '.') {
                cs += '    let ' + name.slice(1) + ' = ' +
                      'this.user_args[' + i + '][1];\n';
                name = name.slice(1);
            } else {
                cs += '    let ' + name + ' = ' +
                      'this.user_args[' + i + '][1];\n';
                ds += '    let ' + name + ' = ' +
                      'this.user_args[' + i + '][1];\n';
            }
        }
        ds += '    if ((typeof timeSpan)!="undefined") ' +
                       'this.timeSpan=timeSpan\n';
        return [cs, ds];
    }

    callChart() {
        chart.setup(this.x, this.y, this.markup)
        chart.drawChart();
        main.reportChartComplete();
    }

    /** Add to text output window.
     *  @param s is a string
     */
    out(s) { this.out_string += s; }

    get outString() { return this.out_string; }

    get modeString() { return this.mode_string; }

    set modeString(s) { this.mode_string = s; }

    /** Search for a specified label in this mode's label set.
     *  @return true if the mode has an empty label set or if the
     *  specified label is in its set, else false
     */
    hasLabel(label) {
        if (this.labels.length == 0) return true;
        for (let i = 0; i < this.labels.length; i++)
            if (label == this.labels[i]) return true;
        return false;
    }

    /** Get the list of user arguments.  */
    get userArgs() {
        return this.user_args;
    }

    /** Get the list of user menus.  */
    get userMenus() {
        return this.user_menus;
    }

    /** Update the value of a user argument.
     *  @param i is the index of the user argument to be updated
     *  @param value is the new value for the specified argument
     */
    updateUserArg(i) {
        this.user_args[i][1] = document.getElementById("userArg_" + i).value;
        let name = this.user_args[i][0];
        if (name == 'timeSpan')
            this.timeSpan = this.user_args[i][1];
        if ((this.group == "status" || this.group == "time-series") &&
            name[0] != '.')
            this.resetTimeSeries();
    }

    /** Update the value of a user menu in response to user selection.
     *  @param i is the index of the user menu to be updated
     */
    updateUserMenu(i) {
        this.user_menus[i].currentValue =
                document.getElementById("userMenu_" + i).value;
        if ((this.group == "status" || this.group == "time-series") &&
            this.user_menus[i].name[0] != '.')
            this.resetTimeSeries();
    }

	/** Set a user menu to a particular value.
	 *  Used by modes to change the value of a menu item.
	 *  @param menuName is the name of the menu to update
	 *  @param value is the new value of the menu item
	 */
	setUserMenu(menuName, value) {
		for (let i = 0; i < this.user_menus.length; i++) {
			let menu = this.user_menus[i];
			if (menu.name == menuName || menu.name == '.' + menuName) {
				modeLibMgr.stopTimeSeries();
				document.getElementById("userMenu_" + i).value = value;
				modeLibMgr.updateUserMenu(i);
				lib.preprocess();
				modeLibMgr.startTimeSeries();
				break;
			}
		}
	}

    addUserMenu(menuName, initValue) {
        this.user_menus.push({name: menuName, currentValue: initValue,
                              choices: []});
    }

    extendMenu(choiceValue, choiceLabel) {
        let menu = this.user_menus[this.user_menus.length-1];
        menu.choices.push({value: choiceValue, label: choiceLabel});
    }

    addCurve(xvec, yvec, color="auto", style="solid") {
        chart.addCurve(new Curve(xvec, yvec, color, style));
    };

    addLine(xvec, y, color="auto", style="solid") {
        let yvec = []; yvec.length = xvec.length; yvec.fill(y);
        chart.addCurve(new Curve(xvec, yvec, color, style));
    };
}

class SpectrumMode extends Mode {
    constructor(name, mode_string, labels, prereqStrings,
                userMenus, userArgs, chartFunctionString) {
        super(name, "spectrum", mode_string,
              labels, prereqStrings, userMenus, userArgs);
        eval("this.chartFunction = function(record, spectrum, " +
             "prereq, x, y, markup, out, addCurve, addLine, " +
			 "setMenu) {\n" + this.userArgsString[0] +
			 chartFunctionString + "\n}\n");
    }

    drawChart() {
        // form a list of prerequisites that must be retrieved from server
        let reqVec = []; let xlist = [];
        reqVec.push(dset.currSpecIndex);
        for (let i = 0; i < this.prereqStrings.length; i++) {
            let p = this.prereqStrings[i];
            let x = dset.currSpecIndex;
            for (let j = 0; j < p.length; j++) {
                     if (p[j] == "1") x = dset.prereq1index(x);
                else if (p[j] == "2") x = dset.prereq2index(x);
                else { x = 0; break; }
            }
            if (x != 0) reqVec.push(x);
            xlist.push(x);
        }
        if (dset.getSpectra(reqVec, this.drawChart, this) == 0)
            return;  // get callback when spectra available

        // now initialize prereq array to contain specified spectra
        let prereq = [];
        for (let i = 0; i < xlist.length; i++) {
            let rec = dset.find(xlist[i]);
            prereq.push(rec == null ? null : rec.spectrum);
        }

        this.x.scaling = "full"; this.y.scaling = "expand";
        this.x.ticks = 9; this.x.precision = 3;
        this.y.ticks = 6; this.y.precision = 3;
        this.x.interval = 50;
        this.markup.leftMargin = 70;

        let x = dset.currSpecIndex;
        let rec = dset.find(x);
        this.currentSpectrum = rec.spectrum;

        let t0 = dset.startTime / 3600; let t = rec.time / 3600;
        this.markup.leftHeader = rec.label + "  " + rec.dateTime +
                                    " (" + (t-t0).toFixed(0) + ")";
        this.sumRec = dset.findCycleSum(x);
        if (this.sumRec != null && this.sumRec.hasOwnProperty("location"))
            this.markup.leftHeader += "  " + this.sumRec.location;
        if (this.sumRec != null && this.sumRec.hasOwnProperty("depth"))
            this.markup.leftHeader += "  " + this.sumRec.depth + "m";
        this.markup.rightHeader = "" + x + ":" + rec.prereq1index +
                                           ":" + rec.prereq2index;
        this.markup.label = "";
        this.markup.qa = -2;

        chart.clearCurveSet();
        this.out_string = "";
        this.chartFunction(rec, rec.spectrum, prereq,
                           this.x, this.y, this.markup, this.boundOut,
                           this.boundAddCurve, this.boundAddLine,
						   this.boundSetUserMenu);
        this.callChart();
    }
}

class StatusMode extends Mode {
    constructor(name, mode_string, labels, prereqStrings, userMenus, userArgs,
                chartFunctionString, datapointFunctionString,
                initFunctionString) {
        super(name, "status", mode_string,
              labels, prereqStrings, userMenus, userArgs);
        let argStrings = this.userArgsString;
        eval("this.chartFunction = function(xvec, yvecs, x, y, markup, out, " +
             "addCurve, addLine, setMenu) {\n" + argStrings[0] +
             chartFunctionString + "\n}\n");
        eval("this.datapointFunction = function(record) {\n" +
              argStrings[1] + datapointFunctionString + "\n}\n");
        eval("this.initFunction = function() {\n" +
              initFunctionString + "\n}\n");
        this.xvec = []; this.yvecs = [];
        this.cycleSumRecord = null;
    }

    extendTimeSeries(x, y) {
        this.xvec.push(x);
        if (this.yvecs.length == 0) {
            for (let i = 0; i < y.length; i++) this.yvecs.push([ y[i] ]);
        } else {
            let m = Math.min(y.length, this.yvecs.length);
            for (let i = 0; i < m; i++) this.yvecs[i].push(y[i]);
        }
    };

    resetTimeSeries() {
        this.xvec.length = 0; this.yvecs.length = 0;
        this.initFunction();
    };


    drawChart() {
        let cycleSumRecs = dset.getCycleSumRecs();
        if (cycleSumRecs.length == 0) 
            setTimeout(this.drawChart.bind(this), 1000);

        this.resetTimeSeries();
        for (let i = 0; i < cycleSumRecs.length; i++) {
            let rec = cycleSumRecs[i];
            let t0 = dset.startTime / 3600;
            let t = rec.time / 3600;
            this.currentTime = t - t0;
            if (this.inRange(this.currentTime)) 
                this.extendTimeSeries(t-t0, this.datapointFunction(rec));
        }
        if (this.xvec.length == 0) return;
    
        this.x.scaling = "full";
        this.x.min = 0; this.x.max = 0;
        this.x.ticks = 9; this.x.interval = 10;
        this.x.precision = 2;
        this.y.scaling = "expand";
        this.y.ticks = 6; this.y.precision = 3;
        this.markup.leftMargin = 70;

        this.markup.leftHeader = dset.startDateTime;
        this.markup.rightHeader = "";
        this.markup.label = "";

        chart.clearCurveSet();
        this.out_string = "";
        this.chartFunction(this.xvec, this.yvecs, this.x, this.y, this.markup,
                           this.boundOut, this.boundAddCurve,
                           this.boundAddLine, this.boundSetUserMenu);
        this.callChart();
    }
}

class TimeSeriesMode extends Mode {
    constructor(name, mode_string, labels, prereqStrings, userMenus, userArgs,
                chartFunctionString, datapointFunctionString,
                initFunctionString) {
        super(name, "time-series", mode_string,
              labels, prereqStrings, userMenus, userArgs);
        let argStrings = this.userArgsString;
        eval("this.chartFunction = function(xvec, yvecs, x, y, markup, out, " +
             "addCurve, addLine, setMenu) {\n" + argStrings[0] +
             chartFunctionString + "\n}\n");
        eval("this.datapointFunction = function(record, spectrum, " +
             "prereq) {\n" + argStrings[1] +
             datapointFunctionString + "\n}\n");
        eval("this.initFunction = function() {\n" +
             initFunctionString + "\n}\n");
        this.xvec = []; this.yvecs = [];
        this.currentIndex = 0;
        this.cycleSumRecord = null;
    }

    extendTimeSeries(x, y) {
        this.xvec.push(x);
        if (this.yvecs.length == 0) {
            for (let i = 0; i < y.length; i++) this.yvecs.push([ y[i] ]);
        } else {
            let m = Math.min(y.length, this.yvecs.length);
            for (let i = 0; i < m; i++) this.yvecs[i].push(y[i]);
        }
    };

    resetTimeSeries() {
        this.xvec.length = 0; this.yvecs.length = 0;
        this.currentIndex = 0;
        this.initFunction();
    };

    getTimeSeriesData(record) {
        let currentSpectrum = record.spectrum;
        let spectrumLabel = record.label;

        let prereq = [];
        for (let i = 0; i < this.prereqStrings.length; i++) {
            let p = this.prereqStrings[i];
            let x = record.index;
            for (let j = 0; j < p.length; j++) {
                     if (p[j] == "1") x = dset.prereq1index(x);
                else if (p[j] == "2") x = dset.prereq2index(x);
                else { x = 0; break; }
            }
            let rec = dset.find(x);
            prereq.push(rec == null ? null : rec.spectrum);
        }

        let yValues = this.datapointFunction(record, record.spectrum, prereq);

        if (yValues != null) {
            let t0 = dset.startTime / 3600; let t = record.time / 3600;
            this.extendTimeSeries(t-t0, yValues);
        }
    }

    /** Compute some datapoints for this time-series.
     *  Add data points to a time-series vector. Continue so long as
     *  there is data to process or until 50 ms has passed.
     *  @param currentModeName is the name of the currently selected chart mode
     *  @return true if data has been added to series, else false
     */
    computeDatapoints(currentModeName) {
        let doneSomething = false;
        let t0 = dset.startTime / 3600;
        let startTime = (new Date()).getTime();
        let rx = dset.nextSpectrum(this.currentIndex);
        while (rx > 0) {
            let now = (new Date()).getTime();
            let rec = dset.find(rx);
            if (now - startTime > 50 || rec == null)
                return doneSomething;

            this.currentTime = (rec.time / 3600) - t0;
            if (this.inRange(this.currentTime) && this.hasLabel(rec.label)) {
                this.getTimeSeriesData(rec);
                doneSomething = true;
            }

            this.currentIndex = rx;
            rx = dset.nextSpectrum(this.currentIndex);
        }
        return doneSomething;
    }

    drawChart() {
        if (this.xvec.length == 0) return;

        this.x.scaling = "full";
        this.x.ticks = 9; this.x.interval = 10;
        this.x.precision = 2;
        this.y.scaling = "expand";
        this.y.ticks = 6; this.y.precision = 3;
        this.markup.leftMargin = 70;

        this.startDateTime = dset.startDateTime;
        this.markup.leftHeader = this.startDateTime;
        this.markup.rightHeader = "";
        this.markup.label = "";

        chart.clearCurveSet();
        this.out_string = "";
        this.chartFunction(this.xvec, this.yvecs, this.x, this.y, this.markup,
                          this.boundOut, this.boundAddCurve, this.boundAddLine,
						  this.boundSetUserMenu);
        this.callChart();
    }
}

class SpecialMode extends Mode {
    constructor(name, mode_string, initFunctionString) {
        super(name, "special", mode_string);
        eval("this.initialize = function() {\n" + initFunctionString + "\n}\n");
    }
}
