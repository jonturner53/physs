/** This module is used to transfer the data output by the
 *  data collector to a remote server (such as coolcloud.mote.org).
 *  It uses the file nextRecord to keep track of
 *  which records have been sent. As new records are added to the
 *  stored data files, it sends them.
 *
 *  This module can be controlled through the opsConsole
 *  using a pair of control files. The ON/OFF file is controlled
 *  by opsConsole and is named ON to enable trarnser, or OFF to
 *  disable transfer. A parallel on/off file is controlled by this
 *  module, in response to changes in ON/OFF.
 *
 *  usage: node xferClient.js remoteServerIP remoteServerPort
 *
 *  default parameters: localhost 28109
 *
 *  Protocol between client and server. Client marshals a set of
 *  records, then opens a connection to server, sends the records
 *  (preceded by a header line containing the first and last record
 *  indices and a record count) and waits for an ack or nack from
 *  the server
 *
 *  Implementation notes: The server must handle transfers from multiple
 *  physs units. To avoid overloading it, each client sends data in batches
 *  of up to 20 records with a 10 second delay in between.
 */

const fs = require('fs');
const net = require('net');
const { exec } = require("child_process");

const homepath = '/usr/local/physs/xfer';
const datapath = '/usr/local/physsData';
const serialNumber = fs.readFileSync('/usr/local/physs/serialNumber', 'utf8');

// process command line arguments
const server = (process.argv.length > 2 ? process.argv[2] : 'localhost');
const port = (process.argv.length > 3 ? parseInt(process.argv[3]) : 28209);

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

/** Return current time in millseconds since epoch. */
function currentTime() {
    return (new Date()).getTime();
}

function fileSize(path) {
    return fs.statSync(path).size;
}

/** Find data file that may contain record with specified record index.
 * 	If none, return name of oldest data file.
 */
function findDataFile(rx) {
	try {
		let path = datapath + '/sn' + serialNumber + '/raw';
		let flist = fs.readdirSync(path).sort((a,b)=>(a>b?-1:(a<b?1:0)));
		let oldest = '';
		for (let fnam of flist) {
			if (!fnam.startsWith('new')) continue;
			oldest = fnam;
			let x = parseInt(fnam.slice(3));
			if (x <= rx) return path + '/' + fnam;
/*
if file is not the oldest, it may not have the record
we want, so we need to go onto next.
*/
		}
		return (oldest.length == 0 ? '' : path + '/' + oldest);
	} catch(err) {
		console.log('could not find data file for ' + rx + ' (' +
					err.msg + ')\n');
	}
	return ''
}

/** Find deployment index of first deployment record following given record.
 *  @param rx is a record index
 *  @return record index of first deployment record that comes after rx or 0
 *  if no such record
 */
function nextDeploymentIndex(rx) {
	try {
		let path = datapath + '/sn' + serialNumber + '/raw';
		let flist = fs.readdirSync(path).sort((a,b)=>(a>b?-1:(a<b?1:0)));
		let lastx = 0;
		for (let fnam of flist) {
			if (!fnam.startsWith('new')) continue;
			let x = parseInt(fnam.slice(3));
			if (x <= rx) return lastx;
			lastx = x;
		}
		return lastx;
	} catch(err) {
		console.log('could not find data file for ' + rx + ' (' +
					err.msg + ')\n');
	}
	return rx;
}

class LineReader {
	constructor() {
		this.rawbuf = new Buffer.alloc(32768);
		this.lines = [];
		this.clear();
	}

	clear() {
        this.frag = '';			// fragment (partial line) from last read
		this.lines.length = 0;	// complete lines waiting to be read
		this.readp = 0;			// file position of first byte of first line
		this.writep = 0;		// file position of next byte to write to frag
		this.linesRead = 0;		// # of lines returned in most recent readlines
        this.path = '';			// path name of file
		this.size = 0;			// length of file
        this.isopen = false;	// true whenever file is open
		this.modtime = 0;		// file modification time
	}

	/** Open a file for reading.
	 *  @param path is the full pathname of the file to be opened.
	 *  @return 1 if the file has been opened, 0 if the file was not opened
	 *  because it has not changed since last open and there are no complete
	 *  lines available to be read; return -1 if an error occurred while
	 *  attempting to open it.
	 */
	open(path) {
		let size = fileSize(path);
		let modtime = fileModTime(path);
		if (path != this.path) {
			this.clear();
		} else {
			if (modtime == this.modtime && this.lines.length == 0 &&
				this.writep == size) {
				return 0;
			}
			if (size < this.size) this.clear();
		}
		this.path = path; this.size = size; this.modtime = modtime;
		try {
			this.fd = fs.openSync(path, 'r');
			this.isopen = true;
			return 1;
		} catch(err) {
			console.log('Error while opening file: ' + path);
			return -1;
		}
	}

	close() {
		if (this.isopen) fs.closeSync(this.fd);
		this.isopen = false;
	}

	/** Advance the read pointer to account for last read.
	 *  This is called when nextRecord is written to file.
	 */
	advance() {
		let n = Math.min(this.linesRead, this.lines.length);
		for (let i = 0; i < n; i++)
			this.readp += this.lines[i].length;
		this.lines.splice(0, n);
	}

	/** Read a specified number of lines.
	 *  @param n is the requested number of lines
	 *  @param returnLines is a callback function used to return an array
	 *  of up to n lines; if no complete lines are available, the array is
	 *  empty; if the file has not been opened or an error occurs,
	 *  it returns null
	 */
	readlines(n, returnLines) {
		this.linesRead = 0;
		if (!this.isopen) return null;
		if (this.lines.length >= n) {
			this.linesRead = n;
			returnLines(this.lines.slice(0,n));
			return;
		}

		let handler = function(err, bytesRead, buf) {
			// called on completion of a read operation
			if (err) {
				console.log(`Error while reading lines (${err.msg})`);
				this.close(); this.clear(); // start over
				returnLines(null);	
			} else if (bytesRead == 0) {
				if (this.lines.length == 0) returnLines([]);	
				this.linesRead = this.lines.length;
				returnLines(this.lines.slice(0));
				return;
			} else {
				let s = this.rawbuf.toString('utf8', 0, bytesRead);
				this.frag += s;
				this.writep += bytesRead;
				let i = this.frag.indexOf('\n');
				while (i >= 0) {
					this.lines.push(this.frag.slice(0,i));
					this.frag = this.frag.slice(i+1);
					i = this.frag.indexOf('\n');
				}
				if (this.lines.length >= n) {
					this.linesRead = n;
					returnLines(this.lines.slice(0, n));
					return;
				}
				fs.read(this.fd, this.rawbuf, 0, this.rawbuf.length,
						this.writep, handler);
				return;
			}
		}.bind(this);

		fs.read(this.fd, this.rawbuf, 0, this.rawbuf.length,
				this.writep, handler);
	}
}

let reader = new LineReader();

// initialize control files used to signal on/off status with ops console server
if (isFile(homepath + '/on'))
	fs.renameSync(homepath + '/on', homepath + '/off')
else if (!isFile(homepath + '/off'))
	fs.writeFileSync(homepath + '/off', 'xfer control file');
let on = false;

let delay = 0;			// used to control interval between batches
let nextRecord = 0;		// index of next record to send to server

/** Start processing of a batch of records. */
function startBatch() {
	// first update on/off status in response to console input
	// note, this is done once per second
	if (on != isFile(homepath + '/ON')) {
		if (on) {
			on = false;
			fs.renameSync(homepath + '/on', homepath + '/off');
		} else {
			on = true; delay = 0;
			fs.renameSync(homepath + '/off', homepath + '/on');
			reader.clear();
		}
	}

	// proceed after 10 seconds, when on
	if (!on) {
		setTimeout(startBatch, 1000); return;
	} else if (delay > 0) {
		delay--; setTimeout(startBatch, 1000); return;
	}
	delay = 10;		// yields 10 second delay between batches when on

	// retrieve nextRecord
	nextRecord = parseInt(fs.readFileSync(homepath + '/nextRecord', 'utf8'));

	path = findDataFile(nextRecord);
	if (reader.open(path) > 0) {
		reader.readlines(20, processBatch);
	} else {
		setTimeout(startBatch, 1000);
	}
}
    
/** Process group of lines from data file.
 *  Received lines are converted to records and sent to server.
 *  @param lines is an array of lines (with newlines removed)
 */
function processBatch(lines) {
	reader.close();
	if (lines.length == 0) {
		// advance nextRecord to next data file (if there is one)
		let dx = nextDeploymentIndex(nextRecord);
		if (dx > 0) fs.writeFileSync(homepath + '/nextRecord', '' + dx);
		setTimeout(startBatch, 1000);
	}

	// convert lines to records
	let records = []; let rec;
	for (let line of lines) {
		if (line[0] != '{' || line[line.length-1] != '}') {
			continue;
		}
		try {
			rec = JSON.parse(line);
			if (rec.index >= nextRecord) records.push(rec);
		} catch(err) {
			console.log(`json conversion error (${err.msg})`);
			continue;
		}
	}
	let n = records.length;
	if (n == 0) {
		// start over, skipping 10 second delay
		reader.advance();
		delay = 0; setTimeout(startBatch, 100);
		return;
	}

	// send records to server
	let batch = records[0].index + ' ' + records[n-1].index + ' ' + n + '\n';
	for (let r of records) batch += JSON.stringify(r) + '\n';
	nextRecord = records[n-1].index + 1;
	numberRecordsSent = n;
		// tentative until ACK received

	let sock = new net.Socket();
	sock.setEncoding('utf8');
	try {
		sock.connect(port, server); 
		sock.once('data', finishBatch);
		sock.setTimeout(1000, () => {
			console.log('No ack received, will retry\n');
			setTimeout(startBatch, 1000);
			sock.destroy();
		});
		sock.on('error', (err) => {
			console.log(`error on socket (${err.msg})`);
			setTimeout(startBatch, 1000);
			sock.destroy();
		});
		sock.end(batch);
	} catch(err) {
		console.log(`Exception while sending data (${err.msg})`);
		setTimeout(startBatch, 0);
		sock.end();
	}
}

/** Complete the processing of batch on receipt of reply from server.
 *  On successful completion, update nextRecord file.
 */
function finishBatch(reply) {
	reply = reply.toString();
	if (reply == 'ACK\n') {
		reader.advance();
		fs.writeFileSync(homepath + '/nextRecord', '' + nextRecord);
	} else {
		console.log(`unexpected reply (${reply.slice(0,reply.length-1)})`);
	}
	setTimeout(startBatch, 1000);
}

startBatch();
