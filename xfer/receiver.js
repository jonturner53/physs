/** This module is used to receive the raw data output by the
 *  fizz data collector and store it on a remote server (such as
 *  coolcloud.mote.org).
 *  
 *  The server handles one connection at a time. When a remote client
 *  has records to send, it opens a connection and sends the records
 *  preceded by a line containing the first and last record indices
 *  plus a count. If the records are all received, server sends an
 *  ack, closes the socket and saves the records.
 *  
 *  Each received record is in the form of a json string,
 *  with a serial number, identifying the fizz unit that sent
 *  the data, a record index, identifying a unique record in the
 *  data stream produced by that fizz unit and a deployment index,
 *  identifying the deployment record of the fizz deployment that
 *  the given record belongs to. This information is used to organize
 *  the data into various directories and files. Specifically,
 *  the data repository is divided into a set of directories,
 *  one for each fizz unit and with names of the form snX,
 *  where X is a serial number. Each of these
 *  directories contains a file for each distinct
 *  deployment of that fizz unit with names of the form depX
 *  where X is a record index, corresponding to the index
 *  of the deployment record for that deployment.
 *  
 *  usage: node receiver.js ip port
 *
 *  Port defaults to 28109, ip defaults to localhost.
 */

const fs = require('fs');
const net = require('net');

const datapath = '/usr/local/physsData';

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

class ConnectionInfo {
	constructor() {
		this.frag = ''; this.records = [];
		this.serialNum = 0; this.firstRx = 0;
		this.lastRx = 0; this.recCount = 0;
	}
}

let connections = {};

let server = net.createServer();
server.maxConnections = 100;

server.on('connection', (sock) => {
	sock.setEncoding('utf8');

	let id = 'id_' + sock.localPort;
	if (id in connections) delete connections[id];
	let conn = new ConnectionInfo();
	connections[id] = conn;

	sock.on('data', (data) => {
		conn.frag += data;
		let i = conn.frag.indexOf('\n');
		while (i >= 0) {
			let line = conn.frag.slice(0,i);
			if (line.length > 32768) {
				sock.write('NACK'); sock.destroy();
				delete connections[id];
				return;
			}
			conn.frag = conn.frag.slice(i+1);
			i = conn.frag.indexOf('\n');
			if (conn.firstRx == 0) {
				// line contains header
				first = false;
				let words = line.split(/[ \t]+/);
				try {
					if (words.length < 3) throw(line);
					conn.firstRx = parseInt(words[0]);
					conn.lastRx = parseInt(words[1]);
					conn.recCount = parseInt(words[2]);
					if (conn.firstRx <= 0 || conn.lastRx <= 0 ||
						conn.recCount <= 0 || conn.firstRx > conn.lastRx ||
						conn.recCount > 100) {
						throw(line);
					}
				} catch(err) {
					console.log(`Error in header (${err.msg})`);
					sock.write('NACK\n'); sock.destroy();
					delete connections[id];
					return;
				}
			} else {
				// line contains a record (or at least, it should)
				try {
					if (line.length < 2 || line[0] != '{' ||
						line[line.length-1] != '}')
						throw(`misformatted line: ${line.slice(0,50)} ... ` +
							  'line.slice(line.length-50)');
					let rec = JSON.parse(line);
					if (rec.index < conn.firstRx || rec.index > conn.lastRx)
						throw(`bad index ${rec.index} in ` +
							  `record ${records.length-1}`);
					if (conn.records.length == 0) {
						// first record
						conn.serialNum = rec.serialNumber;
						if (rec.index != conn.firstRx)
							throw(`bad index ${rec.index} in first record`);
					} else if (rec.serialNumber != conn.serialNum) {
						throw(`bad serial number ${rec.serialNumber} in ` +
							  `record ${conn.records.length-1}`);
					}
					conn.records.push(rec);
					if (conn.records.length == conn.recCount) {
						// last line of block
						sock.write(wrapup(conn) ? 'ACK\n': 'NACK\n');
						sock.destroy(); 
						delete connections[id];
						return;
					}
				} catch(err) {
					console.log(`Error in data (${err.msg})`);
					sock.write('NACK\n'); sock.destroy();
					delete connections[id];
					return;
				}
			}
		}
	});
});

function wrapup(conn) {
	try {
		// setup to copy records to data file
		let path = datapath + '/sn' + conn.serialNum
		if (!isDir(path)) {
			fs.mkdirSync(path + '/raw', { recursive: true, mode: 0o755 });
		}
		let nextRecord = 1;
		if (isFile(path + '/xfer.nextRecord')) {
			nextRecord = parseInt(
							fs.readFileSync(path + '/xfer.nextRecord', 'utf8'));
		} else {
			fs.writeFileSync(path + '/xfer.nextRecord', '1\n');
		}

		// now assemble all new records into a string
		let block = '';
		for (let rec of conn.records) {
			if (rec.index < nextRecord) continue; // must be a repeat
			block += JSON.stringify(rec) + '\n';
		}
		// and update nextRecord
		if (block.length != 0) {
			let rec = conn.records[0];
			let pfx = path + '/raw/';
			let sfx = pad10(rec.recordType == 'deployment' ?
							rec.index : rec.deploymentIndex);
			if (rec.recordType == 'deployment')
				fs.writeFileSync(pfx + 'new' + sfx, block);
			else if (isFile(pfx + 'dep' + sfx))
				fs.appendFileSync(pfx + 'dep' + sfx, block);
			else if (isFile(pfx + 'new' + sfx))
				fs.appendFileSync(pfx + 'new' + sfx, block);
			nextRecord = conn.lastRx + 1;
			fs.writeFileSync(path + '/xfer.nextRecord', '' + nextRecord);
		}
	} catch(err) {
		console.log(`Error while writing data to file (${err})`);
		return false;
	}
	return true;
}

const iface = (process.argv.length > 2 ? process.argv[2] : 'localhost');
const port = (process.argv.length > 3 ? parseInt(process.argv[3]) : 28109);

server.listen(port, iface, 10, () => {
    const ifc = (iface.length == 0 ? 'default' : iface);
    console.log(`transfer receiver listening on ${ifc}:${port}`);
});
