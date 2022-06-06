/** This module is a server that acts as an intermediary between a remote
 *  operations console and the data collector process.
 *  The server operates as a web service, listening for connections
 *  on port 28200.
 *  It accepts commands embedded in html get requests. Most of these
 *  commands are passed directly to the data collector process. Any
 *  response from the data collector is returned as the response to
 *  the get request.
 *
 *  The file name in the GET request is interpreted as the command.
 *  All commands begin with the string 'opsConsole_127_'. The remainder
 *  of the command specifies the operation to be performed.
 *
 *  usage: node opsServer.js listen_ip listen_port
 *
 *  The listen_ip argument is the ip address used for listening for
 *  connections (default: localhost). Listen_port is the port number
 *  used for listening for connections (default: 28200).
 */

const datapath = '/usr/local/physsData';
const modelpath = datapath + '/models/unia';
const rootpath = '/usr/local/physs';
const homepath = '/usr/local/physs/opsConsole';
const runpath = '/usr/local/physs/dataCollector';
const xferpath = '/usr/local/physs/xfer';
const cmdPrefix = '/opsConsole_127_[0-9]{10}_';

const fs = require('fs');
const net = require('net');
const { exec } = require("child_process");
const express = require('express');
const bodyParser = require('body-parser');
const app = express();

app.use(bodyParser.text({ type: 'text/html' }));

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

/** Verify a user name, password combination.
 *
 *  Return true if the given uname, pword combination is present in
 *  the password file, else false.
 */
function verifyUnamePassword(uname, pword) {
	let u = uname.toLowerCase()
	return u in passwords && passwords[u] == pword;
}

/** Read true if the autoRun parameter in the config file >= 0. */
function autoRun () {
	let config = fs.readFileSync(runpath + '/config', 'utf8');
	let lines = config.split('\n');
	for (let line of lines) {
		let words = line.split(/[ \t]+/);
		if (words.length >= 3 && words[0] == 'autoRun' &&
			parseInt(words[2]) >= 0)
			return true;
	}
	return false;
}

function isDigits(s) {
	for (let c of s)
		if (c < '0' || c > '9') return false;
	return true;
}

/** Collector class provides an interface to data collector.
 *  It maintains a socket to the collector, reconnecting if need be.
 *  Log messages from the collector are saved in a log string, while replies
 *  to commands are passed to a saved callback function. Command responses
 *  can be identified by a single '|' character at the start of the reply.
 */
class Collector {
	constructor() {
		this.frag = '';			// buffer for partial line fragment
		this.logString = '';	// complete log messages

		this.replyQueue = [];	// queue of callbacks for replies

		this.connected = false;		// true when connected to collector
		this.connecting = false;	// true when connection is in progress

		this.sock = new net.Socket();
		this.sock.setNoDelay();

		this.sock.on('connect', () => {
			this.connected = true;
			this.connecting = false;
		});

		this.sock.on('error', () => {
			this.connected = false;
			this.connecting = false;
		});

		this.sock.on('close', () => {
			this.connected = false;
			this.connecting = false;
		});

		/** Process data from data collector.
		 *  If a line contains a reply to a command, return the reply
		 *  using the queued reply function.
		 */
		this.sock.on('data', (data) => {
			this.frag += data;
			let i = this.frag.indexOf('\n');
			while (i >= 0) {
				let line = this.frag.slice(0, i+1);
				if (line[0] != '|') {
					this.logString += line;
				} else {
					if (this.replyQueue.length > 0) {
						let [isPeer, replyFunc] = this.replyQueue.shift();
						if (isPeer) {
							replyFunc(this.logString, line);
							this.logString = '';
						} else {
							replyFunc('', line);
						}
					}
				}
				this.frag = this.frag.slice(i+1);
				i = this.frag.indexOf('\n');
			}
		});

		// Connect to data collector and if connection drops, try to reconnect.
		setInterval(
			function() {
				if (!this.connected || this.replyQueue.length > 5) {
					// collector probably crashed, send Error replies
					// to pending requests
					while (this.replyQueue.length > 0) {
						let [isPeer, replyFunc] = this.replyQueue.shift();
						if (isPeer) {
							replyFunc(this.logString,
									  'ERROR: no reply from collector\n');
							this.logString = '';
						} else {
							replyFunc('', 'ERROR: no reply from collector\n');
						}
					}
				}
					
				if (!this.connected && !this.connecting) {
					this.connecting = true;
					this.sock.connect(6256, 'localhost');
				}
			}.bind(this), 2000);
	}

	/** Send a command to data collector and arrange for reply handling.
	 *  @param isPeer is true if the caller is logged into the server
	 *  @param replyFunc is function to call to handle reply; it takes two
	 *  arguments, a log string (containing log messages from the collector)
	 *  and the collector's response to the command. The returned log string is
	 *  empty if isPeer is false.
	 *  @return false if the connection has been lost, else true.
	 */
	sendCommand(cmd, isPeer, replyFunc) {
		if (!this.connected) return false;
		this.sock.write(cmd);
		this.replyQueue.push([isPeer, replyFunc]);
		return true;
	}
	
	/** Get the string of pending log messages.
	 *  @return the log messages and remove them.
	 */
	log() {
		let s = this.logString;
		this.logString = '';
		return s;
	}
}

//
// Handlers for http requests
//

app.use(express.static('static'));
	// for opsConsole in index.html, javascript source in js and image files

let sessionInProgress = false;
let sessionTimeout = 0;
let sessionUser = '';
let sessionCode = '';

app.get(cmdPrefix + 'login_*', (req, res) => {
	let [,,code,,uname, pword] = req.path.split('_');
	let now = currentTime() / 1000; // time in seconds
	if (sessionInProgress && now < sessionTimeout) {
		res.send('|session in progress: ' + sessionUser +
				 ' will be logged out in ' +
			 	 (parseFloat((sessionTimeout - now)/60).toFixed(2)) +
				 ' minutes\n');
	} else if (verifyUnamePassword(uname, pword)) {
		sessionCode =  Math.floor(10000000000 * Math.random());
		if (sessionCode == 0) sessionCode++;
		sessionCode = pad10(sessionCode);
		sessionInProgress = true;
		sessionUser = uname;
		sessionTimeout = now + (15+1)*60;  // 16 minutes to go
		res.send(collector.log() + '|session accepted ' + sessionCode + '\n');
	} else {
		res.send('|session rejected\n');
	}
});

app.get(cmdPrefix + 'logout', (req, res) => {
	let [,,code] = req.path.split('_');
	if (sessionInProgress && code == sessionCode) {
		sessionInProgress = false
		res.send(collector.log() + '|session terminated\n');
	} else {
		res.send('|invalid logout request\n');
	}
});

app.get(cmdPrefix + 'newpass_*', (req, res) => {
	let [,,code,,uname,pword] = req.path.split('_');
	if (sessionInProgress && code == sessionCode) {
		passwords[sessionUser.toLowerCase()] = pword;
		let s = '';
		for (let user in passwords) {
			s += user + ' ' + passwords[user] + '\n';
		}
		fs.writeFileSync(rootpath + '/password', s);
		res.send(collector.log() + '|new password accepted\n');
	} else {
		res.send('|must be logged in to change password\n');
	}
});

app.get(cmdPrefix + 'read_*', (req, res) => {
	let [,,code,,fname] = req.path.split('_');

	let isPeer = (sessionInProgress && code == sessionCode);
	if (fname == 'collector.script' || fname == 'collector.debug' ||
		fname == 'collector.stderr' || fname == 'opsServer.stderr' ||
		fname == 'collector.config' || fname == 'collector.state' ||
		fname == 'collector.maintLog') {
		let s = 'START_FILE ' + fname + '\n'
		filePath = (fname == 'collector.stderr' ? runpath + '/stderr' :
					(fname == 'opsServer.stderr' ? homepath + '/stderr' :
					 (runpath + '/' + fname.slice(10))));
		s += fs.readFileSync(filePath, 'utf8');
		if (s[s.length-1] != '\n') s += '\n';
		s += '|END_FILE\n'; 
		res.send((isPeer ? collector.log() : '') + s);
	} else {
		res.send((isPeer ? collector.log() : '') +
				 '|no file called ' + fname + '\n');
	}
});

app.post(cmdPrefix + 'write_*', (req, res) => {
	let [,,code,,fname] = req.path.split('_');

	let isPeer = (sessionInProgress && code == sessionCode);
	if (isPeer && fname == 'collector.script' ||
				  fname == 'collector.config' ||
				  fname == 'collector.maintLog') {
		let i = req.body.indexOf('START_FILE\n');
		let j = req.body.indexOf('END_FILE\n');
		if (i >= 0 && j > i) {
			fs.writeFileSync(runpath + '/' + fname.slice(10),
							 req.body.slice(i+'START_FILE\n'.length, j));
		}
		res.send(collector.log()); 
	}
});
	
app.get(cmdPrefix + 'clear', (req, res) => {
	let [,,code] = req.path.split('_');

	if (sessionInProgress && code == sessionCode) {
		let OFF = isFile(xferpath + '/OFF')
		let off = isFile(xferpath + '/off')
		if (!OFF || !off) {
			res.send(collector.log() + '|cannot clear data files ' +
					 'while data transfer is enabled\n');
		} else {
			fs.writeFileSync(runpath + '/debug', '');
			fs.writeFileSync(runpath + '/stderr', '');
			fs.writeFileSync(homepath + '/stderr', '');

			let sn = fs.readFileSync(rootpath + '/serialNumber',
									 'utf8').slice(0,2);
			let path = datapath + '/sn' + sn + '/raw';
			let flist = fs.readdirSync(path).sort();
			let count = 0;
			for (let fnam of flist) {
				if (isFile(path + '/' + fnam) && fnam.startsWith('new')) {
					fs.unlinkSync(path + '/' + fnam); count++;
				}
			}
			res.send(collector.log() +
					 '|cleared debug, stderr and ' + count + ' data files\n');
		}
	} else {
		res.send('|must be logged in to clear files\n');
	}
});

app.get(cmdPrefix + 'enableXfer', (req, res) => {
	let [,,code] = req.path.split('_');

	if (sessionInProgress && code == sessionCode) {
		if (isFile(xferpath + '/OFF'))
			fs.renameSync(xferpath + '/OFF', xferpath + '/ON');
		res.send(collector.log() + '|data transfer enabled\n');
	} else {
		res.send('|must be logged in to enable data transfer\n');
	}
});

app.get(cmdPrefix + 'disableXfer', (req, res) => {
	let [,,code] = req.path.split('_');

	if (sessionInProgress && code == sessionCode) {
		if (isFile(xferpath + '/ON'))
			fs.renameSync(xferpath + '/ON', xferpath + '/OFF');
		res.send(collector.log() + '|data transfer disabled\n');
	} else {
		res.send('|must be logged in to disable data transfer\n');
	}
});

app.get(cmdPrefix + 'changeSerialNumber_*', (req, res) => {
	let [,,code,,newSerialNum,xferRecord] = req.path.split('_');

	if (!sessionInProgress || code != sessionCode) {
		res.send('|must be logged in to change serial number\n');
	} else if (!(isDigits(newSerialNum) && isDigits(xferRecord) &&
				 newSerialNum.length == 2)) {
		res.send(collector.log() +
				 '|improper arguments: ' + newSerialNum + ' ' +
				 xferRecord + '\n');
	} else {
		// get old serial number and overwrite it
		let oldSerialNum = fs.readFileSync(rootpath + '/serialNumber',
										   'utf8').slice(0,2);
		fs.writeFileSync(rootpath + '/serialNumber', newSerialNum);

		// stop data collector and data transfer
		exec("systemctl stop collector", (error, stdout, stderr) => {});
		exec("systemctl stop xferSender", (error, stdout, stderr) => {});
		if (isFile(xferpath + '/ON'))
			fs.renameSync(xferpath + '/ON', xferpath + '/OFF')

		// clear debug and data files, create directory for new data files
		fs.writeFileSync(runpath + '/debug', '');
		exec('rm -rf ' + datapath + '/sn' + oldSerialNum,
			 (error, stdout, stderr) => {});
		exec('mkdir -p ' + datapath + '/sn' + newSerialNum + '/raw',
			 (error, stdout, stderr) => {});
		//fs.rmdirSync(datapath + '/sn' + oldSerialNum, { recursive: true });
		// recursive option not working, so switched to exec
		//fs.mkdirSync(datapath + '/sn' + newSerialNum + '/raw',
		//			 { recursive: true });

		// rewrite message of the day file
		fs.writeFileSync('/etc/motd', 'fizz ' + newSerialNum);

		// change port numbers used for port forwarding
		let sshConfig = fs.readFileSync('/root/.ssh/config', 'utf8');
		let lines = sshConfig.split('\n');
		if (lines[lines.length-1] == 0)
			lines = lines.slice(0,-1);
		let newSshConfig = '';
		for (let line of lines) {
			let words = line.trim().split(/[ \t]+/);
			if (words.length >= 3 && words[0] == 'RemoteForward') {
				let j = line.indexOf('RemoteForward');
				newSshConfig += line.slice(0,j) +
					'RemoteForward 28' + newSerialNum +
					 line.slice(j + 'RemoteForward 28xx'.length) + '\n';
			} else {
				newSshConfig += line + '\n';
			}
		}
		fs.writeFileSync('/root/.ssh/config', newSshConfig);

		// change xferRecord file
		fs.writeFileSync(xferpath + '/nextRecord', xferRecord);

		//change state file
		let state = fs.readFileSync(runpath + '/state', 'utf8');
		lines = state.split('\n');
		if (lines[lines.length-1] == 0)
			lines = lines.slice(0,-1);
		let newState = '';
		for (let line of lines) {
			let words = line.split(/[ \t]+/);
			if (words.length < 3) {
				newState += line + '\n'; continue;
			}
			let w0 = words[0];
			if (w0 == 'currentIndex')
				newState += 'currentIndex = ' + xferRecord + '\n';
			else if (w0 == 'deploymentIndex')
				newState += 'deploymentIndex = 1\n';
			else if (w0 == 'spectrumCount')
				newState += 'spectrumCount = 0\n';
			else if (w0 == 'recordMap')
				newState += 'recordMap = dark.1\n';
			else if (w0 == 'cycleNumber')
				newState += 'cycleNumber = 1\n';
			else
				newState += line + '\n';
		}
		fs.writeFileSync(runpath + '/state', newState);

		res.send(collector.log() +
				 '|reconfigured to serial number ' + newSerialNum +
				 ' with next record ' + xferRecord + '\n');
	}
}); 

app.get(cmdPrefix + 'version*', (req, res) => {
	let [,,code,,args] = req.path.split('_');
	if (sessionInProgress && code == sessionCode) {
		let log = 'git log -1 --date=format:"%Y/%m/%d %T" --format="%ad"';
		exec('git log -1 --date=format:"%Y/%m/%d %T" --format="%ad"',
		 	 (error, stdout, stderr) => { res.send('|' + stdout); } );
	} else {
		res.send('|must be logged in for this command\n');
	}
});

app.get(cmdPrefix + 'reboot', (req, res) => {
	let [,,code] = req.path.split('_');
	if (sessionInProgress && code == sessionCode) {
		res.send(collector.log() + '|rebooting in one minute\n');
		exec('/sbin/shutdown -r', function (msg) { console.log(msg) });
	} else {
		res.send('|must be logged in for this command\n');
	}
});

/** Handle commands that are intended for collector.
 *  The collector command is extracted from req.path and forwarded to
 *  the collector. The collector's reply is then returned to the console.
 *  Some requests are forwarded only if the user at the console has
 *  logged in. Simple information requests are forwarded, even for users
 *  who are not logged in.
 */
app.get(cmdPrefix + '*', (req, res) => {
	let [,,code,cmdName] = req.path.split('_');
	let isPeer = sessionInProgress && code == sessionCode;
	let cmd = req.path.slice(req.path.indexOf(cmdName));

	let infoRequest = (cmd == 'snapshot' || cmd == 'logLevel');
	if (isPeer) {
		if (cmd != 'snapshot') {
			sessionTimeout = (currentTime()/1000) + (15+1)*60  // 16 minutes
		}
		if (cmd == 'disableCommLink') {
			// first disable data transfer
			if (isFile(xferpath + '/ON'))
				fs.renameSync(xferpath + '/ON', xferpath + '/OFF');
		}
	}
	let status = collector.sendCommand(cmd.replace(/_/g, ' ') + '\n', isPeer,
		(logString, reply) => {
			if (reply.startsWith('|snapshot reply')) {
				// insert xferState and nextRecord into reply
				let report=JSON.parse(reply.slice('|snapshot reply'.length,-1));
				let xferState = 0;
				if (isFile(xferpath + '/ON')) xferState |= 2;
				if (isFile(xferpath + '/on')) xferState |= 1;
				report.xferState = xferState;
				nextRecord = fs.readFileSync(xferpath + '/nextRecord', 'utf8');
				report.nextRecord = nextRecord
				reply = '|snapshot reply ' + JSON.stringify(report) + '\n';
			}
			res.send(logString + reply);
		});
	if (!status) { // lost connection to collector
		res.send((isPeer ? collector.log() : '' ) +
				 '|ERROR: lost connection to collector\n');
	}
});

//
// Startup code
//

// read passwords
let passwords = {}; let pwfile;
try {
	pwfile = fs.readFileSync(rootpath + '/password', 'utf8');
	let lines = pwfile.split('\n');
	for (let line of lines) {
		words = line.split(/[ \t]+/);
		if (words.length >= 2)
			passwords[words[0].toLowerCase()] = words[1];
	}
} catch(err) {
	console.error('unable to read password file ' + err);
}

sessionCode = '' + Math.random();
	// session code must be included in all non-trivial commands
	// is used to distinguish between commands from current session-peer
	// and commands from users who are not part of the session
sessionInProgress = false;
	// true when a session is in progress
sessionUser = 'noUserName';
sessionTimeout = 0;
	// time after which current session can be replaced by new session

// setup file used to signal data transfers
if (autoRun()) {
	if (isFile(xferpath + '/OFF')) {
		fs.renameSync(xferpath + '/OFF', xferpath + '/ON');
	} else if (!isFile(xferpath + '/ON')) {
		fs.writeFileSync(xferpath + '/ON',
						 'control file used by xferSender and opsConsole server\n')
	}
} else {
	if (isFile(xferpath + '/ON')) {
		fs.renameSync(xferpath + '/ON', xferpath + '/OFF');
	} else if (!isFile(xferpath + '/OFF')) {
		fs.writeFileSync(xferpath + '/OFF',
						 'control file used by xferSender and opsConsole server\n')
	}
}

let collector = new Collector();
	// start interface to data collector

const iface = (process.argv.length > 2 ? process.argv[2] : 'localhost');
const port = (process.argv.length > 3 ? parseInt(process.argv[3]) : 28200);

app.listen(port, iface, () => {
	const ifc = (iface.length == 0 ? 'default' : iface);
	console.log(`operations console server listening on ${ifc}:${port}`);
});
