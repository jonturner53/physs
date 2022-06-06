/** This module sends email alerts when it detects excessively high
 *  similarity index values for karenia brevis. It is invoked as a cron job,
 *  once every 24 hours.
 *
 *  Alerts are sent to all email addresses listed in the file
 *  /usr/local/physs/alert/recipients
 *
 *  For each fizz, it maintains a file alertStatus containing two lines.
 *  The first contains the record index of the most recent record
 *  processed. The second contains the index of the associated deployment
 *  record.
 */

const dataPath = '/usr/local/physsData';
const homePath = '/usr/local/physs/alert';
const fs = require('fs');
const { exec } = require("child_process");

//
// Assorted support functions
//

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

/** Get list of physs serial numbers for which there are data directories. */
function getSerialList() {
	let flist = fs.readdirSync(dataPath, 'utf8').sort();
	let lst = [];
	for (let fnam of flist) {
		if (fnam.startsWith('sn'))
			lst.push(fnam.slice(2));
	}
	return lst;
}

/** Get list of the deployment files for a specified fizz.
 *  @param sn specifies the serial number of the fizz.
 *  @return list of deployment record index values for dep-files
 *  in the summary directory for that fizz.
 */
function getFileList(sn) {
	if (!isDir(dataPath + '/sn' + sn + '/summary')) return [];
	let flist = fs.readdirSync(dataPath + '/sn' + sn + '/summary').sort();
	let lst = []
	for (let fnam of flist) {
		if (fnam.startsWith('dep'))
			lst.push(fnam.slice(3));
	}
	return lst;
}

/** Retrieve a status record for a specific fizz
 *  @param sn is the serial number of a fizz
 *  @return a pair [ lastx, depx ] where lastx is the index of
 *  the most recently processed record and depx is the record index
 *  index for the associated deployment record.
 */
function getAlertStatus(sn) {
	let status = 0; let dep = 0;
	let path = dataPath + '/sn' + sn + '/alertStatus';
	if (isFile(path)) {
		let lines = fs.readFileSync(path, 'utf8').split('\n');
		lastx = parseInt(lines[0]);
		depx = parseInt(lines[1]);
	}
	return [ lastx, depx ];
}

/** Process a specific summary data file.
 *  @param path is path to the file
 *  @param lastx specifies the record index of the last record
 *  that has been processed.
 */
function processSummaryFile(path, lastx) {
	let lines = fs.readFileSync(path, 'utf8').split('\n');
	let n = lines.length;
	if (n == 0) return [ '', lastx ];
	// examine all new records in file and list all with elevated KB
	let msg = '';
	for (let line of lines) {
		if (!line.startsWith('{') || !line.endsWith('}')) continue;
		let record = JSON.parse(line);
		let rx = parseInt(record._index);
		if (rx <= lastx) continue
		lastx = rx;
		let simx = []; let hiKB = false;
		for (let key in record) {
			let value = record[key];
			if (key.startsWith('simIndex/') && value >= 0.7) {
				let name = key.slice(9);
				simx.push([ value, name ]);
				if (name.startsWith('Dino_Kar_brevis')) hiKB = true;
			}
		}
		if (!hiKB) continue;
		simx.sort((a, b) => b[0]-a[0]);

		msg += record._serialNumber + ' ' + record._dateTime + ' ' +
			   record._gpsCoord;
		for (let j = 0; j < simx.length; j++) {
			if (j < 2 || simx[j][1].startsWith('Dino_Kar_brevis'))
				msg += ' ' + simx[j][1] + ':' + simx[j][0].toFixed(4);
		}
		msg += ' QA:' + record['simIndex_QA'] + '\n';
	}
	return [ msg, lastx ];
}

let emsg = '';	// email message string
let snlist = getSerialList();
for (let sn of snlist) {
	let summaryPath = dataPath + '/sn' + sn + '/summary';
	let snstring = '';
	let [ lastx, depx ] = getAlertStatus(sn);
	let rxlist = getFileList(sn);
	if (rxlist.length == 0) continue;
	for (let rx of rxlist) {
		if (parseInt(rx) < depx) continue;
		depx = parseInt(rx);
		let fstring;
		[ fstring, lastx ] =
			processSummaryFile(summaryPath + '/dep' + rx, lastx);
		snstring += fstring;
	}
	if (snstring.length > 0) emsg += snstring + '\n';
	fs.writeFileSync(dataPath + '/sn' + sn + '/alertStatus', 
					 lastx + '\n' + depx + '\n');
}

// send the email alert if there is anything to report
if (emsg.length > 0) {
	emsg += '\nfor more information, goto http://coolcloud.mote.org/data\n' +
			'\ncontact jonturner53@gmail.com to stop receiving PHySS ' +
			'email alerts\n';
	fs.writeFileSync(homePath + '/message', emsg);
	let lines = fs.readFileSync(homePath + '/recipients', 'utf8').split('\n');
	for (let recipient of lines) {
		if (lines.length > 0)
		exec('mailx -s "PHYSS alert" -r jonturner53@gmail.com ' +
			 recipient + ' <' + homePath + '/message',
			 (error, stdout, stderr) => {});
	}
}
