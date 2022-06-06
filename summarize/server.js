/*  This module is used to deliver summary data values to remote data
 *  repositories (GCOOS and/or SECOORA)..

 *  This program operates as a web service, listening for connections
 *  on the port specified on the command line, when it is started
 *  (typicaly 28108).

 *  It responds to http GET requests. If the filename portion of the
 *  GET request is /, it returns a list of data files that can be
 *  retrieved, along with their modification dates. The file names
 *  are of the form ss_x..x, where ss is a serial number and x..x is
 *  a record index. On receiving a GET request for one of these filenames,
 *  the server responds with the contents of that file.

 *  The files are kept in the summary subdirectory of the data area for
 *  selected physs units.
 *  Each file consists of a set of json-formatted records, one for
 *  each sample cycle in the file. Each record is on a separate line.
 */

const dataPath = '/usr/local/physsData';
const homePath = '/usr/local/physs/summarize';

const fs = require('fs');
const express = require('express');
const app = express();

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

/** Return file modification time in seconds since the epoch. */
function fileModTime(path) {
    const stats = fs.statSync(path);
    return (stats.mtime).getTime();
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

/** Get list of the deployment files in a specified directory.
 *  @param path specifies the path to a physs summary directory.
 */
function getFileList(path) {
	let flist = fs.readdirSync(path).sort();
	let lst = []
	for (let fnam of flist)
		if (fnam.startsWith('dep')) {
			let t = fileModTime(path + '/' + fnam);
			lst.push([fnam.slice(3), (t/1000).toFixed(2)]);
		}
	return lst;
}

//
// handlers for web requests
//

/** Handler for root request returns list of pseudo-file names.
 *  These take the form sn_index where sn is the serial number of a
 *  physs and index is a 10 digit record index identifying a raw data
 *  file for that physs.
 */
app.get('/', (req, res) => {
	let snlist = getSerialList();
	let s = '';
	for (let sn of snlist ) {
		let path = dataPath + '/sn' + sn + '/summary';
		if (!isDir(path)) continue;
		let flist = getFileList(path);
		for (let f of flist) {
			s += '<a href="' + sn + '_' + f[0] + '">' +
				 sn + '_' + f[0] + '</a> ' + f[1] + '<br>\n';
		}
	}
	res.send(s.length > 0 ? s : 'no summary data available');
});

/** Handler for specific file.
 *  Path specifies a pseudo-file name of form sn_index (see above).
 */
app.get('/*', (req, res) => {
	let fnam = req.path.slice(1);
	let snx = fnam.split('_');
	if (snx.length != 2) {
		res.send('invalid file name');
	} else {
		let path = dataPath + '/sn' + snx[0] + '/summary/dep' +  snx[1];
		if (isFile(path)) { 
			res.send(fs.readFileSync(path, 'utf8'));
		}
	}
});

const iface = (process.argv.length > 2 ? process.argv[2] : '');
const port = (process.argv.length > 3 ? parseInt(process.argv[3]) : 28108);

app.listen(port, iface, () => {
    const ifc = (iface.length == 0 ? 'default' : iface);
    console.log(`summary server listening on ${ifc}:${port}`);
});
