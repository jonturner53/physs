/** Summarize data from fizz raw data files.
 *
 *  It is designed to be run periodically as a batch job.
 *  When run, it scans the /data/fizz/snXX directories
 *  and in each directory that has a summary sub-directory, it looks
 *  for changes in the raw data files. If it finds a raw file with
 *  no corresponding summary file, or a raw file which is newer than
 *  the summary file, it processes that file, either creating a new
 *  summary file or adding additional records to it.
 *
 *  Summary files consist of json-formatted records containing summary
 *  information on cdom and phytoplankton similarity index values, one per
 *  sample cycle.
 */

let lib;	// instance Library from Library.js as extended by standard
const fs = require('fs');
const readline = require('readline');

async function main() {
	// read Library.js into a string and instantiate it as lib
	let homepath = '/usr/local/physs/analysisConsole'
	let libString;
	try {
		libString = fs.readFileSync(homepath + '/static/js/Library.js',
					'utf8');
	} catch (err) {
		console.error(err);
	}
	eval(libString + "lib = new Library();");

	// read standard library, extract generic properties/methods
	// and then execute them to extend lib
	let xlibString;
	try {
		xlibString = fs.readFileSync('./analysisLibrary', 'utf8')
	} catch (err) {
		console.error(err)
	}	
	eval(xlibString);

	// setup path to fizz data files
	let datapath = '/usr/local/physsData';

	// read phytoplankton model files and create object indexed by
	// model name
	let phytoModels = readPhytoModels(datapath + '/models/unia');

	// for each directory ..data/fizz/snNN look for new raw data files or
	// changes to raw data files and create or append to summary files
	let dlist = fs.readdirSync(datapath);
	dlist.sort();
	for (let dirname of dlist) {
		if (!dirname.startsWith('sn')) continue;
		let rawpath = datapath + '/' + dirname + '/raw';
		let sumpath = datapath + '/' + dirname + '/summary';
		if (!fs.existsSync(rawpath) || !fs.existsSync(sumpath))
			continue;
		let rawlist = fs.readdirSync(rawpath); rawlist.sort();
		let sumlist = fs.readdirSync(sumpath); sumlist.sort();
		for (let fname of rawlist) {
			if (!fname.startsWith('dep')) continue;
			let rawfile = rawpath + '/' + fname;
			let sumfile = sumpath + '/' + fname;
			if (sumlist.includes(fname) &&
				fileModTime(sumfile) > fileModTime(rawfile))
				continue;
			let lastIndex = 0;
			if (sumlist.includes(fname)) {
				let s = fs.readFileSync(sumfile, 'utf8');
				let i = s.lastIndexOf('"_index":');
				let j = s.indexOf(',', i);
				lastIndex = parseInt(s.slice(i+9, j));
			}

			let dset = await readDataset(rawfile);
			lib.setWavelengths(dset.depRec.wavelengths);
			lib.setWaveguideLength('waveguideLength' in dset.depRec ?
									dset.depRec.waveguideLength : 0.28);
			lib.setNlcCoef('correctionCoef' in dset.depRec ?
						   dset.depRec.correctionCoef : [1]);

			let summary = summarize(dset, phytoModels, lastIndex+1);
			fs.appendFileSync(sumfile, summary);
		}
	}
}

/** Return file modification time in seconds since the epoch. */
function fileModTime(path) {
  const stats = fs.statSync(path);
  return (stats.mtime).getTime();
}

/** Read a dataset from a file and return as an object. */
async function readDataset(path) {
	let reader = readline.createInterface({
  		input: fs.createReadStream(path),
		crlfDelay: Infinity
		});
	let depRec = null; let csumRecs = [];
	let specRecs = [];let dir = {};
	for await (const line of reader) {
		if (!line.startsWith('{') || !line.endsWith('}')) continue;
		let record = JSON.parse(line.slice());
		if (record.recordType == 'deployment') {
			depRec = record;
		} else if (record.recordType == 'cycleSummary') {
			dir[record.index] = specRecs.length;
			csumRecs.push(record);
		} else if (record.recordType == 'spectrum') {
			dir[record.index] = specRecs.length;
			specRecs.push(record);
		} else if (record.recordType == 'reset') {
			if (csumRecs.length == 0) {
				while (specRecs.length > 0) {
					delete dir[specRecs[specRecs.length-1].index];
					specRecs.pop();
				}
			} else {
				let lastSx = csumRecs[csumRecs.length-1].index;
				while (specRecs.length > 0 &&
					   specRecs[specRecs.length-1].index > lastSx) {
					delete dir[specRecs[specRecs.length-1].index];
					specRecs.pop();
				}
			}
		}
	}
	return {'depRec': depRec, 'csumRecs': csumRecs,
			'specRecs': specRecs, 'dir': dir};
}


/** Read all phytoplankton model files and return object containing
 *  models indexed by long model name
 */
function readPhytoModels(path) {
	// longer versions of phyto model names
	let longNames = {
		'Phaeo_tri' : 'Bacillar_Phaeodac',
		'Pseudo_del' : 'Bacillar_P-nitz_del',
		'Pseudo_frau' : 'Bacillar_P-nitz_fraud',
		'Shel_cos (should be Skel_cos)' : 'Bacillar_Skeleto',
		'Thal_pseudonana' : 'Bacillar_Thal_pseudo',
		'Thal_weissflogii_n' : 'Bacillar_Thal_weiss',
		'Dunal' : 'Chloro_Dunali',
		'Hemisel_1' : 'Crypto_Hemisel',
		'Tricho' : 'Cyano_Tricho_eryt',
		'Amphid_cart' : 'Dino_per_Amphidi',
		'KB_GI' : 'Dino_Kar_brevis_GI',
		'KB_TL' : 'Dino_Kar_brevis_TL',
		'KB_mrh04n' : 'Dino_Kar_brevis_mrh04n',
		'Kar_miki' : 'Dino_Kar_miki',
		'Proro_min' : 'Dino_per_Prorocent',
		'Tetrasel' : 'Prasino_Tetrasel',
		'EmillIan_hux (should be Emilian_hux)' : 'Prymnes_Emilli',
		'Pleurochrysis' : 'Prymnes_Pleurochry',
		'Iso_galb' : 'Pyrmnes_Isochry',
		'Heterosig_akash' : 'Raphido_Heterosig'
	};
	let modelSet = {};
	let flist = fs.readdirSync(path); flist.sort();
	for (let fnam of flist) {
		if (!fnam.endsWith('.unia')) continue;
		let pmod = JSON.parse(fs.readFileSync(path + '/' + fnam, 'utf8'));
		let stem = fnam.slice(0,-5);
		if (stem in longNames) modelSet[longNames[stem]] = pmod;
		else modelSet[stem] = pmod;
	}
	return modelSet;
}

/** Produce summary records for all sample cycles in dset for which
 *  the sample cycle index is at least equal to start
 */
function summarize(dset, phytoModels, start=0) {
	if (dset.csumRecs[dset.csumRecs.length-1].index < start)
		return "";

	// find first cycle summary with index >= start and latest
	let csumi = 0;
	if (start > 1) {
		// scan backwards
		let i = dset.csumRecs.length-1;
		while (i >= 0 && dset.csumRecs[i].index >= start) i--;
		csumi = i+1;
	}

	let specj = (csumi == 0 ? 0 : dset.dir[dset.csumRecs[csumi-1].index] + 1);
	let specrec = dset.specRecs[specj];
	
	// for each cycle summary from starting point to end of dataset
	// compute absorption parameters and QA value for filtered spectrum,
	// and SI values and QA value for unfiltered spectrum
	let sumString = '';
	while (csumi < dset.csumRecs.length) {
		let csumrec = dset.csumRecs[csumi];

		// find last filtered record and concentrate record preceding
		// current cycle summary
		let filtrecj = -1; let concrecj = -1;
		while (specj < dset.specRecs.length && specrec.index < csumrec.index) {
			if (specrec.label == 'filtered') filtrecj = specj;
			if (specrec.label == 'concentrate') concrecj = specj;
			specj++; specrec = dset.specRecs[specj];
		}
		if (filtrecj < 0 && concrecj < 0) continue;

		// create summary record
		let sumrec = {};
		sumrec._serialNumber = csumrec.serialNumber;
		sumrec._index = csumrec.index;
		sumrec._dateTime = csumrec.dateTime;
		sumrec._gpsCoord = ('gpsCoord' in csumrec ? csumrec.gpsCoord :
							('location' in csumrec ? csumrec.location :
							 ('gpsCoord' in dset.depRec ?
							  dset.depRec.gpsCoord : "[N0.0,E0.0]")));
		sumrec._depth = 'depth' in csumrec ? csumrec.depth : 0;
		
		if (filtrecj >= 0) {
			// compute absorption params for current filtered spectrum
			let filtrec = dset.specRecs[filtrecj];
			let prereq = getPrereqs(filtrec, dset);
			if (prereq[0] != null && prereq[1] != null && prereq[2] != null) {
				let absorp = lib.absorption(filtrec.spectrum, prereq,
											dset.depRec.wavelengths);
				let [a440, slope] = lib.absorptionParameters(absorp);
				let qa = lib.qaAbsorption(filtrec.spectrum, prereq);
				sumrec.cdomAbsorp440 = Number(a440.toFixed(4));
				sumrec.cdomSlope = Number(slope.toFixed(5));
				sumrec.cdom_QA = 
					(qa < 0 ? 'invalid' : (qa == 0 ? 'marginal' : 'ok'));
			}
		}

		if (concrecj >= 0) {
			// compute sim index values for current concentrate spectrum
			let concrec = dset.specRecs[concrecj];
			let prereq = getPrereqs(concrec, dset);
			if (prereq[0] != null && prereq[1] != null && prereq[2] != null) {
				let absorb = lib.absorbance(concrec.spectrum, prereq,
											dset.depRec.wavelengths);
				let deriv = lib.deriv(absorb, 4);
				for (let key in phytoModels) {
					let pmod = phytoModels[key];
					let mabsorb = lib.absorbance(pmod.disc,
									[pmod.dark, pmod.cdom, pmod.dark],
									pmod.wavelengths);
					let mderiv = lib.deriv(mabsorb, 4);
					let simx = lib.simIndex(deriv, mderiv);
					sumrec['simIndex/' + key] = Number(simx.toFixed(4));
				}
				let qa = lib.qaAbsorbance(concrec.spectrum, prereq);
				sumrec.simIndex_QA =
					(qa < 0 ? 'invalid' : (qa == 0 ? 'marginal' : 'ok'));
			}
		}
		sumString += JSON.stringify(sumrec) + '\n';

		csumi++;
	}
	return sumString;
}

/** Get array of prerequisite spectra for given record. */
function getPrereqs(record, dset) {
	let prereq = [null, null, null];
	let p1 = record.prereq1index;
	if (p1 in dset.dir)
		prereq[0] = dset.specRecs[dset.dir[p1]].spectrum;
	let p2 = record.prereq2index;
	if (p2 in dset.dir) {
		let p2rec = dset.specRecs[dset.dir[p2]];
		prereq[1] = p2rec.spectrum;
		let p21 = p2rec.prereq1index;
		if (p21 in dset.dir)
			prereq[2] = dset.specRecs[dset.dir[p21]].spectrum;
	}
	return prereq;
}

main();
