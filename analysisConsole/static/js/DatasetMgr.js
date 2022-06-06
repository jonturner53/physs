/** Object for managing current data set. */
class DatasetMgr {
    /** Constructor for DatasetMgr object.  */
    constructor() {
        this.serialNum = '';    // sn for current data file
        this.fileId = '';       // deployment record index for current data file
    
        this.depRec = {};       // deployment record

		this.spectSerialNum = '-'; // spectrometer serial number
        this.wavelengths = [];  // raw wavelength values, set on first load
		this.nlcCoef = [];		// nonlinearity correction coefficients
        this.waveguideLength = .28;  // set on first load

        this.currSpecIndex = 0; // index of current spectrum record
        this.spectra = {};      // spectrum records, accessed using record index

        this.specList = [];     // spectrum list (index,prereq1,prereq2,label) 
        this.nextSpec = 0;      // records in specList[nextSpec..] are yet to be
                                // retrieved from server

        this.cycleSumRecs = []; // cycle summary records
        this.debugString  = ''; // debug messages, as string
        this.configRecs = [];   // config records (usually just one)
        this.scriptRecs = [];   // script records (usually just one)
        this.maintLogRecs = []; // maintenance log records (usually just one)
    
        this.startDateTime = null;  // earliest time in current dataset
        this.endDateTime = null;    // latest time in current dataset
        this.startTime = 0;     // in seconds since epoch
        this.endTime = 0;     // in seconds since epoch
    
        this.firstLoad = true;   // cleared after first chunk of data received
    }

    init() {
        this.getSerialNumbers();
    }

	/** Get list of fizz serial numbers from server, along with list of
	 *  fileIds for first fizz in list.
	 *  Invoked when page is opened or refreshed.
	 */
	getSerialNumbers() {
        serverRequest("getSerialNumbers")
        .then(function(reply) {
            let lines = reply.split('\n');
            let snList = JSON.parse(lines[0]);
            let fileList = JSON.parse(lines[1]);
			main.reportSerialNumbers(snList);
			main.reportFiles(fileList);
        }.bind(this));
	}
	
	/** Get list of file identifiers for current fizz from server.
	 *  Invoked when user selects new fizz from sn menu.
	 */
	getFileIds() {
        serverRequest("getFileIds_" + main.currentSerialNum)
        .then(function(reply) {
            let fileList = JSON.parse(reply);
			main.reportFiles(fileList);
		}.bind(this));
	}

    /** Load a new data file from the server. */
    load() {
        this.serialNum = main.currentSerialNum;
        this.fileId = main.currentFile;
        this.firstLoad = true; // signals initial load of dataset (not reload)
        this.reload();
    }

    /** Reload the current data set.  */
    reload() {
        if (this.fileId == "0") return;
            // a zero entry means no data files

        serverRequest("loadData_" + this.serialNum + "_" + this.fileId)
        .then(function(reply) {
            let lines = reply.split('\n');
			if (lines.length < 4) {
                main.log('loadData: incomplete response'); return;
			}
    
            if (this.firstLoad) {
                // lines[0] is deployment record
                this.depRec = JSON.parse(lines[0]);
                this.depRec.time = (new Date(this.depRec.dateTime)).getTime()
                                    / 1000;  // seconds since epoch
                this.wavelengths = this.depRec.wavelengths;
                this.waveguideLength =
                    (this.depRec.hasOwnProperty("waveguideLength") ?
                     this.depRec.waveguideLength : .28)
				this.spectSerialNum = "-";
				this.nlcCoef = [1];
				if ("spectSerialNumber" in this.depRec) {
					this.spectSerialNum = this.depRec.spectSerialNumber;
					this.nlcCoef = this.depRec.correctionCoef;
				}
    
                this.spectra = {};

                // lines[1] is first spectrum record
                let rec = JSON.parse(lines[1]);
                this.spectra[rec.index] = rec;
                rec.time = (new Date(rec.dateTime)).getTime() / 1000;
                this.startDateTime = rec.dateTime;
                this.startTime = rec.time
                    // time in seconds since the epoch
            }
    
            // lines[2] is last spectrum record
            let rec = JSON.parse(lines[2]);
            this.spectra[rec.index] = rec;
            rec.time = (new Date(rec.dateTime)).getTime() / 1000;
            this.endDateTime = rec.dateTime;
            this.endTime = rec.time;
        
            // lines[3] is list of all spectra
            this.specList = JSON.parse(lines[3]);
            this.currSpecIndex = (this.specList.length > 0 ?
                    this.specList[this.specList.length-1][0]  : 0);
            this.nextSpec = 0;
//console.log('specList: ', this.specList);
    
            // schedule retrieval of remaining records
            this.cycleSumRecs = []; this.scriptRecs = [];
            this.configRecs = [];   this.maintLogRecs = []; this.debugString = "";    
            setTimeout(this.getAllSpectra.bind(this), 50);
            setTimeout(this.getConfigRecs.bind(this), 500);
            setTimeout(this.getScriptRecs.bind(this), 1000);
            setTimeout(this.getDebugString.bind(this), 1500);
            setTimeout(this.getCycleSumRecs.bind(this), 2000);
    
            // announce load (or reload) of a dataset (first chunk)
            main.datasetRcvd(this.firstLoad, this.waveguideLength,
                             this.wavelengths, this.serialNum, this.fileId,
                             this.depRec.label, this.depRec.dateTime,
							 this.nlcCoef);
            main.reportProgress(Object.keys(this.spectra).length,
                                            this.specList.length);
            this.firstLoad = false;
        }.bind(this));
    }

    /** Get one or more spectrum records from server.
     *  @param xvec is a vector of valid spectrum record indices
     *  @param callback is a function that is called after the requested
     *  records have been retrieved; it is not called if all records are
     *  already available
     *  @param context is the context in which the callback should be invoked
     *  @return 1 if all records are available on return; 0 if one or more
     *  records is being retrieved from the server; -1 if no records are
     *  being retrieved and at least one record index is invalid
     */
    getSpectra(xvec, callback, context) {
        let args = '';
        for (let i = 0; i < xvec.length; i++) {
            if (this.find(xvec[i]) == null) args += xvec[i] + ',';
        }
        if (args.length == 0) return 1;
        args = this.serialNum + '_' + this.fileId + '_' + args.slice(0,-1);

        serverRequest("getSpectra_" + args, [callback, context])
        .then(function([reply, callback, context]) {
            // record per line
            let lines = reply.split('\n');
            for (let i = 0; i < lines.length - 1; i++) {
                // note, we ignore last value in lines,
                // since split finishes with an empty string
                let rec = JSON.parse(lines[i]);
                rec.time = (new Date(rec.dateTime)).getTime() / 1000;
                if (rec.serialNumber == this.serialNum &&
                    rec.deploymentIndex == this.fileId)
                    this.spectra[rec.index] = rec;
            }
            main.moreDataRcvd();
            main.reportProgress(Object.keys(this.spectra).length,
                                            this.specList.length);
            setTimeout(callback.bind(context), 10);
        }.bind(this));

        return 0;
    }

    /** Get all spectrum records listed in specList.
     *  Iterate through specList, retrieving all records we don't already
     *  have. Request records from server in batches of 5.
     */
    getAllSpectra() {
        // first, advance nextSpec past all the spectra we already have
        let xvec = []; let x;
        if (this.specList.length == 0) return;
            // handles case of left-over getAllSpectra call while
            // the loading a new data file
        while (this.nextSpec < this.specList.length) {
            x = (this.specList[this.nextSpec])[0];
            if (this.find(x) == null) {
                xvec.push(x); break;
            }
            this.nextSpec++;
        }
        // now, find up to 5 spectra to retrieve
        let i = this.nextSpec + 1;
        while (i < this.specList.length && xvec.length < 5) {
            x = this.specList[i][0];
            if (this.find(x) == null) xvec.push(x);
            i++;
        }

        if (xvec.length > 0) this.getSpectra(xvec, this.getAllSpectra, this);
    }
/*
If I attempt to open new file, while spectra are still being retrieved, can get
conflict.
*/

    /** Get the index of a given spectrum's first prerequisite.
     *  @param x is the index of a spectrum record.
     *  @return the index of the given spectrum's first prerequisite,
     *  or 0 if x is not a valid index
     */
    prereq1index(x) {
        let i = this.locate(x);
        if (i == -1) return 0;
        return this.specList[i][1];
    }

    /** Get the index of a given spectrum's second prerequisite.
     *  @param x is the index of a spectrum record.
     *  @return the index of the given spectrum's first prerequisite,
     *  or 0 if x is not a valid index
     */
    prereq2index(x) {
        let i = this.locate(x);
        if (i == -1) return 0;
        return this.specList[i][2];
    }

    /** Get the label of a given spectrum.
     *  @param x is the index of a spectrum record.
     *  @return the label of the spectrum, or 0 if x is not a valid index
     */
    label(x) {
        let i = this.locate(x);
        if (i == -1) return 0;
        return this.specList[i][3];
    }

    /** Find a spectrum record with a specified index.
     *  @param x is a record index
     *  @return a spectrum record with index x, or null if we
     *  do not have such a record
     */
    find(x) {
        return (this.spectra.hasOwnProperty(x) ? this.spectra[x] : null);
    }

    /** Check the validity of a spectrum record index.
     *  @param is a record index
     *  @return true if the current data set has a spectrum record with index x
     */
    valid(x) {
        return this.locate(x) != -1;
    }
        
    /** Find the location of spectrum in the specList.
     *  @param x is a record index
     *  @return the array index of the specified spectrum in specList
     *  or -1 if no valid entry
     */
    locate(x) {
        if (this.specList.length == 0) return -1
        let lo = 0; let hi = this.specList.length-1;
        while (lo <= hi) {
            let mid = Math.floor((hi+lo)/2);
            let val = (this.specList[mid])[0];
            if (val == x) return mid;
            else if (val < x) lo = mid+1;
            else hi = mid-1;
        }
        return -1;
    }

    /** Find the next index of a spectrum record.
     *  @param x is an index of a spectrum, or 0
     *  @return the index of the next valid record index for a spectrum
     *  or 0 if no such record; if x == 0, return the index of the first 
     *  spectrum.
     */
    nextSpectrum(x) {
        if (x == 0)
            return (this.specList.length == 0 ? 0 : this.specList[0][0]);
        let i = this.locate(x);
        if (i == -1 || i == this.specList.length-1) return 0;
        return this.specList[i+1][0];
    }

    filterMatch(tag) {
        if (main.labelFilter == "") return true;
        let f = main.labelFilter;
        let parts = f.split(/ +/);
        for (let i = 0; i < parts.length; i++)
            if (tag.startsWith(parts[i].trim())) return true;
        return false;
    }

    /** Respond to navigation buttons.
     *  Determine the record index of the next spectrum to be charted
     *  and update the currSpecIndex property.
     *  @param tag is a character encoding which of the navigation
     *  buttons has been pressed.
     */
    gotoNextSpec(tag) {
        // find current record position in specList
        let crp = this.locate(this.currSpecIndex);
        // find index of next spectrum tag
        let ix = 0; let i; let cnt;
        let n = this.specList.length;
        if (tag == 'f') {
            ix = this.specList[0][0];
        } else if (tag == 'l') {
            ix = this.specList[n-1][0];
        } else if (tag == '>' || tag == '>>') {
            cnt = (tag == '>' ? 1 : main.jumpCount);
            for (i = crp+1; i < n; i++) {
                if (this.filterMatch("any") || 
                    this.filterMatch(this.specList[i][3])) {
                    crp = i;
                    cnt--;
                    if (cnt == 0) break;
                }
            }
            if (i < n)
                ix = this.specList[i][0];
            else if (main.labelFilter == "any" || main.labelFilter == "")
                ix = this.specList[n-1][0];
            else
                ix = this.specList[crp][0]; // don't change if no match
        } else if (tag == '<' || tag == '<<') {
            cnt = (tag == '<' ? 1 : main.jumpCount);
            for (i = crp-1; i >= 0; i--) {
                if (this.filterMatch("any") || 
                    this.filterMatch(this.specList[i][3])) {
                    crp = i;
                    cnt--;
                    if (cnt == 0) break;
                }
            }
            if (i >= 0)
                ix = this.specList[i][0];
            else if (main.labelFilter == "any" || labelFilter == "")
                ix = this.specList[0][0];
            else
                ix = this.specList[crp][0];
        }
        this.currSpecIndex = ix;
        main.spectrumChange();
    }

    /** Retrieve cycle summary records from server.
     *  @return a reference to a vector of cycle summary records, or an
     *  empty  vector if the records have not yet been retrieved from
	 *  the server
     */
    getCycleSumRecs() {
        if (this.cycleSumRecs.length != 0) return this.cycleSumRecs;

        serverRequest('getCycleSumRecs_' + this.serialNum + '_' + this.fileId)
        .then(function(reply) {
            let lines = reply.split('\n'); // record per line
            for (let i = 0; i < lines.length - 1; i++) {
                let rec = JSON.parse(lines[i]);
                rec.time = (new Date(rec.dateTime)).getTime() / 1000;
                this.cycleSumRecs.push(rec);
            }
			main.reportDataRecs();
        }.bind(this));
        return this.cycleSumRecs;
    }

    /** Find the cycle summary for the cycle containing a given spectrum.
     *  @param x is the index of a spectrum record
     *  @return the first cycle summary record with an index larger than x;
     *  or null if no such record
     */
    findCycleSum(x) {
        if (this.cycleSumRecs.length == 0) return null
        let rec = this.cycleSumRecs[0];
        if (rec.index > x) return rec;
        let lo = 0; let hi = this.cycleSumRecs.length-1;
        while (lo <= hi) {
            let mid = Math.floor((hi+lo)/2);
            rec = this.cycleSumRecs[mid];
            let prev = this.cycleSumRecs[mid-1];
            if (rec.index > x && prev.index < x) return rec;
            else if (rec.index < x) lo = mid+1;
            else hi = mid-1;
        }
        return null;
    }

    /** Retrieve config records from server. 
     *  @return a reference to a vector of config records, or an empty vector
     *  if the records have not yet been retrieved from the server
     */
    getConfigRecs() {
        if (this.configRecs.length != 0) return this.configRecs;

        serverRequest('getConfigRecs_' + this.serialNum + '_' + this.fileId)
        .then(function(reply) {
            let lines = reply.split('\n'); // record per line
            for (let i = 0; i < lines.length - 1; i++) {
                let rec = JSON.parse(lines[i]);
                rec.time = (new Date(rec.dateTime)).getTime() / 1000;
                this.configRecs.push(rec);
            }
			main.reportDataRecs();
        }.bind(this));
        return this.configRecs;
    }

    /** Retrieve script records from server. 
     *  @return a reference to a vector of script records, or null
     *  if the records have not yet been retrieved from the server
     */
    getScriptRecs() {
        if (this.scriptRecs.length != 0) return this.scriptRecs;
        serverRequest('getScriptRecs_' + this.serialNum + '_' + this.fileId)
        .then(function(reply) {
            let lines = reply.split('\n'); // record per line
            for (let i = 0; i < lines.length - 1; i++) {
                let rec = JSON.parse(lines[i]);
                rec.time = (new Date(rec.dateTime)).getTime() / 1000;
                this.scriptRecs.push(rec);
            }
			main.reportDataRecs();
        }.bind(this));
        return this.scriptRecs;
    }

    /** Retrieve maintenance log records from server. 
     *  @return a reference to a vector of maintenance log records, or null
     *  if the records have not yet been retrieved from the server
     */
    getMaintLogRecs() {
        if (this.maintLogRecs.length != 0) return this.maintLogRecs;
        serverRequest('getMaintLogRecs_' + this.serialNum + '_' + this.fileId)
        .then(function(reply) {
            let lines = reply.split('\n'); // record per line
            for (let i = 0; i < lines.length - 1; i++) {
                let rec = JSON.parse(lines[i]);
                rec.time = (new Date(rec.dateTime)).getTime() / 1000;
                this.maintLogRecs.push(rec);
            }
			main.reportDataRecs();
        }.bind(this));
        return this.maintLogRecs;
    }

    getConfigString() {
        let recs = this.getConfigRecs();
		if (recs.length == 0) return "no config records";
        let s = '';
        for (let i = 0; i < recs.length; i++) {
                if (i > 0)
                    s += "\n*** " + recs[i].index + " ****************\n\n";
                s += this.decodeConfigScript(recs[i].configString);
        }
        return s;
    }

    getScriptString() {
        let recs = this.getScriptRecs();
		if (recs.length == 0) return "no script records";
        let s = '';
        for (let i = 0; i < recs.length; i++) {
                if (i > 0)
                    s += "\n*** " + recs[i].index + " ****************\n\n";
                s += this.decodeConfigScript(recs[i].scriptString);
        }
        return s;
    }

    getMaintLogString() {
        let recs = this.getMaintLogRecs();
		if (recs.length == 0) return "no mainteance log records";
        let s = '';
        for (let i = 0; i < recs.length; i++) {
            s += this.decodeConfigScript(recs[i].maintLogString);
        }
        return s;
    }

    decodeConfigScript(s) {
        let rval = "";
        let i = 0; let j = s.indexOf("%%");
        while (j >= 0) {
            rval += s.slice(i,j) + "\"";
            i = j+2; j = s.indexOf("%%", i);
        }
        i = 0; j = s.indexOf("@@");
        while (j >= 0) {
            rval += s.slice(i,j) + "\n";
            i = j+2; j = s.indexOf("@@", i);
        }
        return rval;
    }

    /** Retrieve debug messages from server. 
     *  the debug string is already present.
     *  @return a reference to the debug string, or null
     *  if it has not yet been retrieved from the server
     */
    getDebugString() {
        if (this.debugString.length != 0) return this.debugString;

        serverRequest('getDebugString_' + this.serialNum + '_' + this.fileId)
        .then(function(reply) {
            this.debugString = reply;
        }.bind(this));
        return this.debugString;
    }

    getCycleSumString() {
        let recs = this.getCycleSumRecs();
		if (recs.length == 0)
			return "no cycle summary records";
        let s = "";
        for (let i = 0; i < recs.length; i++) {
            s += JSON.stringify(recs[i]) + "\n";
        }
        return s;
    }

	getPhyssInfoString() {
		let s = "PHySS: " + this.serialNum + "\n\n" +
				"spectrometer: " + this.spectSerialNum + "\n" +
				"nonlinear correction coefficients\n    ";
		for (let i = 0; i < this.nlcCoef.length; i++) {
			if (i == 4) s += "\n    ";
			s += this.nlcCoef[i].toExponential(4) + " ";
		}
		s += "\n";
		s += "waveguide length: " + this.waveguideLength + "\n\n";
		s += "deployment record index: " + this.depRec.index + "\n";
		s += "deployment start time: " + this.depRec.dateTime + "\n";
		s += "deployment label: " + this.depRec.label + "\n";
		return s;
	}
};
