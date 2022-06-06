/** This module is used to analyze data produced by the Programmable
 *  Seawater Scanner. It is controlled through a remote console and
 *  allows users to analyze saved data.

 *  This program operates as a web service, listening for connections
 *  on the port specified on the command line, when it is started
 *  (typicaly 28201 when run on a Beaglebone 28101 when run on coolcloud).
 *  It uses http 1.0 (no persistent connections).
 *  It accepts commands embedded in html get requests.

 *  The file name in the GET request is interpreted as the command.
 *  All commands begin with the string 'dataAnalyzer_2047'. The remainder
 *  of the command specifies the operation to be performed. These include

 *  getSerialNumbers return list of serial numbers and list of fileIds
 *  		for the first serial number in list
 *  getFileIds_serialNumber return list of file ids for a specified serial
 *  		number, where an id is a pair containing a deployment record
 *  		index and a date/time string.
 *  loadData_serialNumber_index load data file for specified serial number and
 *  		deployment record index, return deployment record, spectrum list,
 *  		cycle summary records and first spectrum
 *  getSpectra_serialNumber_depIndex_indexList
 *  		return one or more spectra from the data file specified by a given
 *  		serial number and deployment record index; the index list is
 *  		a comma separated list or record indices
 *  getConfigRecs_serialNumber_depIndex return all config file records
 *  getScriptRecs_serialNumber_depIndex return all script records
 *  getMaintLogRecs_serialNumber_depIndex return all maintenance log records
 *  getCycleSumRecs_serialNumber_depIndex
 *  		return all cycleSummary records from the data file specified by
 *  		the given serialNumber and deployment record index
 *  getDebugString_serialNumber_depIndex
 *  		return the debug messages from the data file specified by
 *  		the given serialNumber and deployment record index
 *  getModelNames retrieve list of phytoplankton models
 *  getModel_model retrieve a specific phytoplankton model
 *  getModeLibNames return a list of mode libraries
 *  getModeLib_modeLib return a specified mode library as a string
 */

const dataPath = '/usr/local/physsData';
const modelPath = dataPath + '/models/unia';
const homePath = '/usr/local/physs/analysisConsole';
const cmdPrefix = '/dataAnalyzer_2047_';

const fs = require('fs');
const readline = require('readline');
const express = require('express');
const app = express();
const iface = (process.argv.length > 2 ? process.argv[2] : '');
const port = (process.argv.length > 3 ? parseInt(process.argv[3]) : 28101);

/** Return true if a path identifies a regular file */
function isFile(path) {
	try {
		let stats = fs.statSync(path);
		return stats.isFile();
	} catch(err) {
		return false;
	}
}

/** Return true if a path identifies a directory */
function isDir(path) {
	try {
		let stats = fs.statSync(path);
		return stats.isDirectory();
	} catch(err) {
		return false;
	}
}

/** Add leading zeros to a number to produce a 10 digit string. */
function pad10(n) {
	let s = '0000000000' + n;
	return s.slice(s.length-10);
}

/** Return file modification time in seconds since the epoch. */
function fileModTime(path) {
	const stats = fs.statSync(path);
	return (stats.mtime).getTime();
}

/** Build a list of serial numbers from a list of the files
 *  in the fizz data directory.
 */
function buildSerialList(flist) {
	let s = '';
	for (let fnam of flist) {
		if (!fnam.startsWith('sn')) continue
		if (s.length > 0) s+= ',';
		s += fnam.slice(2);
	}
	return '[' + s + ']';
}

function buildFileIdList(path, flist, returnValue) {
	let count = 0;
	for (fnam of flist) {
		if (fnam.startsWith('dep') || fnam.startsWith('new'))
			count++;
	}

	let pairs = [];
	for (fnam of flist) {
		if (!fnam.startsWith('dep') && !fnam.startsWith('new'))
			continue;
		firstRecord(path + '/' + fnam, (rec) => {
			if (rec != null)
				pairs.push([ rec.index , rec.dateTime ]);
			if (--count == 0) {
				pairs.sort((a,b) => (a[0]>b[0] ? -1 :
				  		 			 (a[0]<b[0] ? 1 : 0)));
				let s = '';
				for (p of pairs) {
					if (s.length != 0) s += ',';
					s += '[' + p[0] + ',"' + p[1] + '"]';
				}
				returnValue('[' + s + ']');
			}
		});
	}
}

/** Return the first record in a file.
 *  @param path is a path to a file containing json-formatted records.
 *  @param returnValue is a callback used to return the first record
 *  in the file along with the full path name of the file
 */
function firstRecord(path, returnValue) {
	let rstream = fs.createReadStream(path);
	let reader = readline.createInterface({
       	input: rstream, crlfDelay: Infinity
       });

	let handler = function(line) {
		if (line.length >= 2 && line[0] == '{' && line[line.length-1] == '}') {
			returnValue(JSON.parse(line));
            reader.removeListener('line', handler);
            reader.close(); rstream.close();
        }
    };
	reader.on('line', handler);
}

let dataStore = {}; // collection of recently loaded datasets, by sn_idx tag
let lru = [];	  // list of tags of recently used datasets

/** Load a dataset from a file to the dataStore object. 
 *  @param sn is the serial number of a fizz
 *  @param idx is a record index used to identify a file
 *  @param returnData is a callback function which is passed the
 *  deployment record of the loaded dataset, when the process completes.
 */
function loadData(sn, idx, returnData) {
	let tag = sn + '_' + idx;
	let pfx = dataPath + '/sn' + sn + '/raw';
	let sfx = pad10(idx);
	let path = pfx + '/dep' + sfx;
	if (!isFile(path)) path = pfx + '/new' + sfx;
	if (!isFile(path)) { returnData(null); return; }

	let timestamp = fileModTime(path);
	if (tag in dataStore &&
		dataStore[tag].timestamp == timestamp) {
		returnData(dataStore[tag].depRec);
		return;
	}

	// go ahead and load new dataset
	let depRec = null; let specRecs = [];
	let specList = []; let specDir = {};
	let configRecs = []; let scriptRecs = []; let maintLogRecs = [];
	let csumRecs = []; let debugRecs = [];

	let reader = readline.createInterface({
  		input: fs.createReadStream(path),
		crlfDelay: Infinity
		});

	reader.on('line', (line) => {
		if (!line.startsWith('{') || !line.endsWith('}')) return;
		let record = JSON.parse(line.replace(/\t/g, ' '));
		if (record.recordType == 'deployment') {
			depRec = record;
		} else if (record.recordType == 'cycleSummary') {
			specDir[record.index] = specRecs.length;
			csumRecs.push(record);
		} else if (record.recordType == 'config') {
			configRecs.push(record);
		} else if (record.recordType == 'script') {
			scriptRecs.push(record);
		} else if (record.recordType == 'maintLog') {
			maintLogRecs.push(record);
		} else if (record.recordType == 'debug') {
			debugRecs.push(record);
		} else if (record.recordType == 'spectrum') {
			specDir[record.index] = specRecs.length;
			specRecs.push(record);
			specList.push([record.index, record.prereq1index,
						   record.prereq2index, record.label]);
		} else if (record.recordType == 'reset') {
			if (csumRecs.length == 0) {
				while (specRecs.length > 0) {
					delete specDir[specRecs[specRecs.length-1].index];
					specRecs.pop(); specList.pop();
				}
			} else {
				let lastSx = csumRecs[csumRecs.length-1].index;
				while (specRecs.length > 0 &&
					   specRecs[specRecs.length-1].index > lastSx) {
					delete specDir[specRecs[specRecs.length-1].index];
					specRecs.pop(); specList.pop();
				}
			}
		}
	});

	reader.on('close', () => {
		if (lru.includes(tag)) {
			delete dataStore[tag]; lru.splice(lru.indexOf(tag), 1); 
		} else if (dataStore.length == 10) {
			delete dataStore[lru[0]]; lru.shift();
		}
		dataStore[tag] = { 'timestamp': timestamp,
					'depRec': depRec, 'specRecs': specRecs,
					'specList': specList, 'specDir': specDir,
					'configRecs': configRecs, 'scriptRecs': scriptRecs,
					'maintLogRecs': maintLogRecs,
					'csumRecs': csumRecs, 'debugRecs': debugRecs};
		lru.push(tag);
		returnData(depRec);
	});

	reader.on('error', console.error);
}

/** Find a spectrum record with a specified index.
 *  @parm dset is a previously loaded data set
 *  @parm x is the index of some record in dset
 *  @parm return the record with index x or null if no matching record
 */
function findSpectrum(dset, x) {
	if (!(x in dset.specDir)) return null;
	let i = dset.specDir[x];
	if (i >= dset.specRecs.length) return null;
	return dset.specRecs[i];
}

app.use(express.static('static'));

/** Return physs data files (requested via console download button).
 *  Request takes for physData_sn_idx where sn is the serial number
 *  of the physs for which data is being requested and idx is the
 *  record index of the first record in the requested file; this is
 *  used to construct the file name.
 */
app.get('/physsData_*', (req, res) => {
	let [ ,sn, idx] = req.path.split('_');
	let pfx = dataPath + '/sn' + sn + '/raw';
	let sfx = pad10(idx);
	let path = pfx + '/dep' + sfx;
	if (!isFile(path)) path = pfx + '/new' + sfx;
	res.sendFile(path);
});

app.get(cmdPrefix + 'getSerialNumbers*', (req, res) => {
	let flist = fs.readdirSync(dataPath).sort();
	let snlist = buildSerialList(flist);
	if (snlist.length <= 2) {
		res.send('Error: no serial numbers');
	} else {
		let i = snlist.indexOf(',');
		if (i < 0) i == snlist.indexOf(']');
		let sn0 = parseInt(snlist.slice(1,i));
		let path = dataPath + '/sn' + sn0 + '/raw';
		let flist = fs.readdirSync(path);
		buildFileIdList(path, flist, (idlist) => {
			res.send(idlist.length == 2 ? 'Error: no data files' :
										  snlist + '\n' + idlist);
		});
	}
});

app.get(cmdPrefix + 'getFileIds_*', (req, res) => {
	let [,,,sn] = req.path.split('_');
	let path = dataPath + '/sn' + sn + '/raw';
	let flist = fs.readdirSync(path);
	buildFileIdList(path, flist, (idlist) => {
		res.send(idlist.length == 2 ? 'Error: no data files found' : idlist);
	});
}); 

app.get(cmdPrefix + 'loadData_*', (req, res) => {
	let [,,,sn,idx] = req.path.split('_');
	loadData(sn, idx, (depRec) => {
		if (depRec == null) {
			res.send('Error: no such data file');
		} else {
			let dset = dataStore[sn + '_' + idx];
			let reply = JSON.stringify(depRec) + '\n';
			if (dset.specRecs.length > 0) {
				reply += JSON.stringify(dset.specRecs[0]) + '\n'
					  +  JSON.stringify(
							dset.specRecs[dset.specRecs.length-1]) + '\n'
					  +  JSON.stringify(dset.specList) + '\n';
			}
			res.send(reply);
		}
	});
});

app.get(cmdPrefix + 'getSpectra_*', (req, res) => {
	let [,,,sn,idx,xlist] = req.path.split('_');
	xlist = xlist.split(',');
	loadData(sn, idx, (depRec) => {
		if (depRec == null) {
			res.send('Error: no such data file');
		} else {
			let dset = dataStore[sn + '_' + idx];
			let s = '';
			for (x of xlist) {
				let rec = findSpectrum(dset, Number(x));
				if (rec != null) s += JSON.stringify(rec) + '\n';
			}
			res.send(s.length == 0 ? 'Error: no such spectra' : s);
		}
	});
});

app.get(cmdPrefix + 'getCycleSumRecs_*', (req, res) => {
	let [,,,sn,idx] = req.path.split('_');
	loadData(sn, idx, (depRec) => {
		if (depRec == null) {
			res.send('Error: no such data file');
		} else {
			let dset = dataStore[sn + '_' + idx];
			let s = ''
			for (rec of dset.csumRecs)
				s += JSON.stringify(rec) + '\n';
			res.send(s.length == 0 ? 'Error: no cycle summary records' : s);
		}
	});
});

app.get(cmdPrefix + 'getConfigRecs_*', (req, res) => {
	let [,,,sn,idx] = req.path.split('_');
	loadData(sn, idx, (depRec) => {
		if (depRec == null) {
			res.send('Error: no such data file');
		} else {
			let dset = dataStore[sn + '_' + idx];
			let s = ''
			for (rec of dset.configRecs)
				s += JSON.stringify(rec) + '\n';
			res.send(s.length == 0 ? 'Error: no config records' : s);
		}
	});
});

app.get(cmdPrefix + 'getScriptRecs_*', (req, res) => {
	let [,,,sn,idx] = req.path.split('_');
	loadData(sn, idx, (depRec) => {
		if (depRec == null) {
			res.send('Error: no such data file');
		} else {
			let dset = dataStore[sn + '_' + idx];
			let s = ''
			for (rec of dset.scriptRecs)
				s += JSON.stringify(rec) + '\n';
			res.send(s.length == 0 ? 'Error: no script records' : s);
		}
	});
});

app.get(cmdPrefix + 'getMaintLogRecs_*', (req, res) => {
	let [,,,sn,idx] = req.path.split('_');
	loadData(sn, idx, (depRec) => {
		if (depRec == null) {
			res.send('Error: no such data file');
		} else {
			let dset = dataStore[sn + '_' + idx];
			let s = ''
			for (rec of dset.maintLogRecs)
				s += JSON.stringify(rec) + '\n';
			res.send(s.length == 0 ? 'Error: no maintenance log records' : s);
		}
	});
});

app.get(cmdPrefix + 'getDebugString_*', (req, res) => {
	let [,,,sn,idx] = req.path.split('_');
	loadData(sn, idx, (depRec) => {
		if (depRec == null) {
			res.send('Error: no such data file');
		} else {
			let dset = dataStore[sn + '_' + idx];
			let s = ''
			for (rec of dset.debugRecs)
				s += rec.message + '\n';
			res.send(s.length == 0 ? 'Error: no cycle summary records' : s);
		}
	});
});

app.get(cmdPrefix + 'getModelNames*', (req, res) => {
	let names = '';
	if (isDir(modelPath)) {
		let flist = fs.readdirSync(modelPath).sort();
		for (fnam of flist) {
			if (names.length > 0) names += ',';
			names += '"' + fnam + '"';
		}
	}
	res.send(names.length > 0 ? '[' + names + ']' : 'Error: no models found');
});

app.get(cmdPrefix + 'getModel_*', (req, res) => {
	let model = req.path.slice((cmdPrefix + 'getModel_').length);
	fs.readFile(modelPath + '/' + model, (err, data) => {
		res.send(err ? 'Error: cannot find model' : data);
	});
});

app.get(cmdPrefix + 'getModeLibNames*', (req, res) => {
	let names = '';
	if (isDir(homePath + '/modeLibs')) {
		let flist = fs.readdirSync(homePath + '/modeLibs').sort();
		for (fnam of flist) {
			if (names.length > 0) names += ',';
			names += '"' + fnam + '"';
		}
	}
	res.send(names.length>0 ? '[' + names + ']' : 'Error: no mode libs found');
});

app.get(cmdPrefix + 'getModeLib_*', (req, res) => {
	let lib = req.path.slice((cmdPrefix + 'getModeLib_').length);
	fs.readFile(homePath + '/modeLibs/' + lib, (err, data) => {
		res.send(err ? 'Error: cannot read mode library' : data);
	});
});

app.listen(port, iface, () => {
	const ifc = (iface.length == 0 ? 'default' : iface);
	console.log(`analysis console server listening on ${ifc}:${port}`)
});
