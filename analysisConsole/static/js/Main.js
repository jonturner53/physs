/** Main module for analysis console.
 *  Provides framework in which other modules interact.
 *  Mostly consists of event handlers, which take event reports from
 *  user interface elements or other modules and react to them.
 *  Separates user interface elments from main modules.
 */
class Main {
    constructor() {
		// flags to signal first occurrence of key events
        this.gotPhyto = false;          // true when all phyto models received
        this.gotData = false;           // true when first data chunk received
        this.gotModeLib = false;        // true when first mode lib received

		// define properties for all gui elements
        this.serialNumMenu = document.getElementById("serialNumMenu");
        this.fileMenu = document.getElementById("fileMenu");
        this.modeMenu = document.getElementById("modeMenu");
        this.phytoMenu = document.getElementById("phytoMenu");
        this.modeLibMenu = document.getElementById("modeLibMenu");

        this.canvas = document.getElementById("canvas");
        this.showHideButton = document.getElementById("showHideButton");
        this.xycoord = document.getElementById("xycoord");
        this.label_filter = document.getElementById("labelFilter");
        this.jump_count = document.getElementById("jumpCount");

        this.progressCounter = document.getElementById("progressCounter");
        this.downloadButton = document.getElementById("downloadButton");

        this.footerLeft = document.getElementById("footerLeft");
        this.footerRight = document.getElementById("footerRight");

		this.logString = "";
    }

    //
    // General utility methods
    //

    get ready() { return this.gotPhyto && this.gotData && this.gotModeLib; }

    get currentSerialNum() { return this.serialNumMenu.value; };
    get currentFile() { return this.fileMenu.value; };
    get currentMode() { return this.modeMenu.value; };
    set currentMode(modeName) { this.modeMenu.value = modeName; };
    get currentPhytoModel() { return this.phytoMenu.value; };
    set currentPhytoModel(modelId) { this.phytoMenu.value = modelId; };
    get currentModeLib() { return this.modeLibMenu.value; };
    set currentModeLib(lib) { this.modeLibMenu.value = lib; };

	get labelFilter() { return this.label_filter.value; };
	get jumpCount() { return this.jump_count.value; };

	log(s) {
		this.logString += s ;
		if (s[s.length-1] != '\n') s += '\n';
	};

	getLogString() { return this.logString; }

    //
    // Methods called to react to menu selections
    //

    serialNumSelected() { dset.getFileIds(); }

    datasetSelected() {
		modeLibMgr.stopTimeSeries();
		dset.load();
	}

    modeSelected() {
        modeLibMgr.setUserArgs();
        if (!this.ready) return;
        modeLibMgr.drawChart();
    }

    phytoSelected() {
        phyto.selectPhytoModel();
		modeLibMgr.drawChart();
    }

    pageSelected() {
        info.display();
    }

    modeLibSelected() {
        modeLibMgr.changeModeLib();
    }

    //
    // Methods called to react to button pushes
    //

    /** Add/remove currently selected phyto model to/from active set.
     *  @param addit is 1 if phyto model is to be added, otherwise its removed.
     */
    phytoActive(addit) {
        let model = this.currentPhytoModel;
        if (addit) {
			phyto.include(model);
		} else {
			phyto.exclude(model);
		}
		this.markPhytoModelList();
        if (!this.ready) return;
        modeLibMgr.drawChart();
        modeLibMgr.startTimeSeries();
    }

    markPhytoModelList() {
        let s = this.phytoMenu.innerHTML;
        let v = this.currentPhytoModel;
        let lines = s.split("\n");
        s = "";
        for (let i = 0; i < lines.length-1; i++) {
            let j = lines[i].indexOf(">");
            let k = lines[i].indexOf("<", j);
            let name = lines[i].slice(j+3,k);
            lines[i] = lines[i].slice(0,j+1) +
                        (phyto.included(name) ? "+ " :
                         (phyto.excluded(name) ? "- " : ". ")) +
                        lines[i].slice(j+3);
            s += lines[i] + "\n";
        }
        this.phytoMenu.innerHTML = s;
        this.currentPhytoModel = v;
    }

    reload() {
        dset.reload();
    }

    backgroundButton(tag) {
        if (tag[0] == 'a') chart.addBGcurve(tag[1]);
        else if (tag == 'd') chart.dropBGcurve();
        else if (tag == 's') {
            chart.showBGcurves();
            this.showHideButton.innerHTML = "hide"; 
        } else {
            chart.hideBGcurves();
            this.showHideButton.innerHTML = "show";
        }
		info.display();
    }

    navButton(tag) {
        if (this.gotData) dset.gotoNextSpec(tag);
    }

    importUserLib() {
        modeLibMgr.import();
        if (this.ready) modeLibMgr.drawChart();
    }

    exportToUserLib() {
        modeLibMgr.export();
    }

    mouseEvent(event, tag) {
        if (tag == 'u') chart.mouseRelease(event);
        else if (tag == 'd') chart.mousePress(event);
        else if (tag == 'm') chart.mouseMove(event);
        else chart.mouseOut();
    }

    //
    // Methods called by modules to announce occurrence of events.
    //

    buildMenu(items) {
        if (items.length == 0) return"<option> --- </option> ";
        let optGroup = (items[0].length == 3);
        let s = '';
        for (let i = 0; i < items.length; i++) {
            if (optGroup) {
                if (i == 0 || items[i][2] != items[i-1][2]) {
                    if (i > 0) s += "</optgroup>\n";
                    s += `<optgroup label="${items[i][2]}">`;
                }
            }
            let [val, tag] = items[i];
            s += "<option value=\"" + val + "\">" + tag + "</option>\n";
        }
        return s;
    }

    /** Report reception of serial numbers.
     *  @param is a list of fizz serial numbers
     */
    reportSerialNumbers(serialNumbers) {
        let list = [];
        for (let sn of serialNumbers) list.push([sn, sn]);
        this.serialNumMenu.innerHTML = this.buildMenu(list);
    }

    /** Report reception of list of data files.
     *  @param files is a list of (fileId, dateTime) pairs to be added
     *  to user interface
     */
    reportFiles(files) {
        this.fileMenu.innerHTML = this.buildMenu(files);
		if (files.length > 0)  {
			modeLibMgr.stopTimeSeries(); dset.load();
		}
    }

    /** Report reception of list of mode libraries.
     *  @param files is a list of (name, label) pairs to be added
     *  to user interface
     */
    reportModeLibNames(libs) {
        let list = [];
        for (let l of libs) list.push([l, l]);
        this.modeLibMenu.innerHTML = this.buildMenu(list);
    }

    /** Report reception of list of chart modes.
     *  @param modes is a list of mode (name, group) pairs to be
     *  added to user interface
     */
    reportModes(modes) {
        let list = [];
        for (let m of modes) list.push([m[0], m[0], m[1]]);
        this.modeMenu.innerHTML = this.buildMenu(list);
    }

    /** Report reception of phyto model list.
     *  @param phytoModels is a list of strings denoting model file names
     *  (with .unia extension) to be added to user interface
     */
    reportPhytoModels(modelIds) {
        let list = [];
        for (let m of modelIds) list.push([m, '. ' + m.slice(0,-5)]);
        this.phytoMenu.innerHTML = this.buildMenu(list);
    }

    datasetRcvd(firstLoad, waveguideLength, rawWavelengths,
                serialNum, fileId, deploymentLabel, dateTime, nlcCoef) {
        chart.clearZoom;
        if (firstLoad) {
            this.footerLeft.innerHTML =
                    deploymentLabel + " (sn" + serialNum + ")";
            this.footerRight.innerHTML = dateTime;
	        lib.setWaveguideLength(waveguideLength);
	        lib.setWavelengths(rawWavelengths);
			lib.setNlcCoef(nlcCoef);
	        let tag = "physsData_" + serialNum + "_" + fileId;
            this.downloadButton.href = tag;
            this.downloadButton.download = tag + ".txt";
        }
        if (!this.gotData) {
            this.gotData = true;
            if (this.gotModeLib) {
                // assume that very first chart does not need phyto models
                // typically true and avoids big initial delay,
                // while waiting for phyto models to load
                modeLibMgr.drawChart();
		        modeLibMgr.startTimeSeries();
            }
        } else if (this.ready) {
            modeLibMgr.drawChart();
	        if (firstLoad) {
	            modeLibMgr.startTimeSeries();
	        } else {
	            modeLibMgr.resumeTimeSeries();
	        }
        }
    }

	/** Called by chart mode after a chart has been drawn.
	 *  Needed because chart drawing can be interrupted if required spectra
	 *  must be retrieved from server.
	 */
	reportChartComplete() {
		info.display();
	}

	reportDataRecs() {
		info.display();
	}

	reportProgress(count, total) {
		this.progressCounter.innerHTML = "" + count + "/" + total;
	}

	reportMouse(x, y, hide=false) {
        this.xycoord.innerHTML = (hide ? "" :
				"(" + lib.format(x, 3) + ", " + lib.format(y, 3) + ")");
	}

    moreDataRcvd() {
        modeLibMgr.resumeTimeSeries();
    }

    spectrumChange() {
        if (this.ready) {
            modeLibMgr.drawChart();
			modeLibMgr.setUserArgs();
        }
    }

    /** Assign lib global to appropriate support library.
     *  Called before initialization of a new support library
     */
    swapSupportLib(libName) {
        lib = (libName == "classic" ? classicLib :
               (libName == "standard" ? standardLib :
                (libName == "experimental" ? experimentalLib : userLib)));
        lib.setWavelengths(dset.wavelengths);
        lib.setWaveguideLength(dset.waveguideLength);
		lib.setNlcCoef(dset.nlcCoef);
    }

    newModeLib() {
        this.gotModeLib = true;
        if (this.ready) {
            modeLibMgr.drawChart();
            modeLibMgr.startTimeSeries();
        }
    }

    allModelsRcvd() {
        this.gotPhyto = true;
        if (this.ready) {
            modeLibMgr.drawChart();
            modeLibMgr.startTimeSeries();
        }
    }

    userArgChanged() {
        if (this.ready) {
            modeLibMgr.drawChart();
        }
    }

    chartMod() {
        if (this.ready) {
            modeLibMgr.drawChart(); 
        }
    }
}
