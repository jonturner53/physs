/** Model objects represent phytoplankton models. */
class Model {
	constructor(name, dateTime, labName, techName, serialNum, recordIndex,
				spectSerialNum, wavelengths, nlcCoef,
				dark, filtered, unfiltered,
				concentration, chlorophyll, comment) {
		this.name = name;
		this.dateTime = dateTime;
		this.labName = labName;
		this.techName = techName;
		this.physsSerialNum = serialNum;
		this.recordIndex = recordIndex;
		this.spectSerialNum = spectSerialNum;
		this.wavelengths = wavelengths;
		this.nlcCoef = nlcCoef;
		this.dark = dark;
		this.filtered = filtered;
		this.unfiltered = unfiltered;
		this.concentration = concentration;		// cells per liter
		this.chlorophyll = chlorophyll;			// micrograms per liter
		this.comment = comment;
	}
};

/** Object for managing phytoplankton models. */
class PhytoModels {
	constructor() {
		this.inSet = {};		// set of models that are turned on
		this.exSet = {};	   // set of models that are turned off
		this.modelIds = [];		// list of model identifiers
		this.modelSet = {};	 	// dictionary of phytoplankton models, by name
		this.still2come = 10000;   // number of models expected from server
		this.modelCount = 0;	// number of models in set
		this.hstate = "";		// used by hide/restore methods
	}

	/** Determine if a model is part of the "included set".
	 *  @param name is the user-friendly name of a model.
	 *  @return true if the named model is in the included set, else false
	 */
	included(name) {
		return this.inSet.hasOwnProperty(name + ".unia");
	}

	/** Determine if a model is part of the "excluded set".
	 *  @param name is the user-friendly name of a model.
	 *  @return true if the named model is in the excluded set, else false
	 */
	excluded(name) {
		return this.exSet.hasOwnProperty(name + ".unia");
	}
	
	/** Temporarily hide a model by placing it into the excluded set.
	 */
	hide(name) {
		this.hstate = "-";
		if (this.excluded(name)) {
			this.hstate = "ex";
		} else {
			if (this.included(name)) this.hstate = "in";
			this.exclude(name + ".unia");
		}
	}
	/** Restore included/excluded satus of a previously hidden model.
	 */
	restore(name) {
			 if (this.hstate == "in") this.include(name + ".unia");
		else if (this.hstate == "-")  this.exclude(name + ".unia");
	}

	currentModel() {
		return this.modelSet[main.currentPhytoModel];
	}

	currentModelId() {
		return main.currentPhytoModel;
	}

	model(name) {
		return this.modelSet[name + ".unia"];
	}

	init() {
		return this.getModelNames();
	}

	/** Get the list of model ids from the server. */
	getModelNames() {
		serverRequest("getModelNames").then(function(reply) {
			this.modelIds = JSON.parse(reply);
			this.still2come = this.modelIds.length;
			main.reportPhytoModels(this.modelIds);
			if (this.modelIds.includes('Dino_Kar_brev_g.unia') >= 0)
				main.currentPhytoModel = 'Dino_Kar_brev_g.unia';
			this.getModel(main.currentPhytoModel);
			let j = 1;
			for (let i = 0; i < this.modelIds.length; i++) {
				if (this.modelIds[i] != main.currentPhytoModel) {
					setTimeout(this.getModel.bind(this), 100*j++,
							   this.modelIds[i]);
				}
			}
		}.bind(this));
	}

	addModel(model) {
		let id = model.name + '.unia';
		if (this.modelSet.hasOwnProperty(id)) return;
		this.modelSet[id] = model;
		if (!this.modelIds.includes(id)) {
			this.modelIds.push(id);
			main.reportPhytoModels(this.modelIds);
		}
		this.modelCount++;
	}

	/** Get a specific model from the server. */
	getModel(modelId) {
		serverRequest("getModel_" + modelId, [modelId])
		.then(function([reply, modelId]) {
			// unia file in form of dictionary
			let m = JSON.parse(reply);
			this.addModel(new Model(
				('modelName' in m ? m.modelName : modelId.slice(0,-5)),
				('dateTime' in m ? m.dateTime : ''),
				('labName' in m ? m.labName : ''),
				('techName' in m ? m.techName : ''),
				('physsSerialNum' in m ? m.spectSerialNum : '-'),
				('recordIndex' in m ? m.recordIndex : '-'),
				('spectSerialNum' in m ? m.spectSerialNum : '-'),
				m.wavelengths,
				('nlcCoef' in m ? m.nlcCoef : [1]),
				m.dark, m.cdom, m.disc,
				('concentration' in m ? m.concentration : 0),
				m.chlorophyll,
				('comment' in m ? m.comment : '')));
			this.still2come = (this.still2come>0 ? this.still2come-1 : 0);
			if (this.allModelsPresent()) main.allModelsRcvd();
		}.bind(this));
	}

	allModelsPresent() {
		return this.still2come == 0;
	}

	/** Respond to change in menu.
	 *  Retrieves model file if necessary.
	 */
	selectPhytoModel() {
		let modelId = main.currentPhytoModel;
		if (!this.modelSet.hasOwnProperty(modelId))
			this.getModel(modelId);
	}
	
	/** Toggle status of modelId in set of included models.  */
	include(modelId) {
		if (this.inSet.hasOwnProperty(modelId)) {
			delete this.inSet[modelId];
		} else {
			this.inSet[modelId] = 0;	// dummy value
			if (this.exSet.hasOwnProperty(modelId)) 
				delete this.exSet[modelId];
		}
	}

	/** Toggle status of modelId in set of excluded models.  */
	exclude(modelId) {
		if (this.exSet.hasOwnProperty(modelId)) {
			delete this.exSet[modelId];
		} else {
			this.exSet[modelId] = 0;	// dummy value
			if (this.inSet.hasOwnProperty(modelId)) 
				delete this.inSet[modelId];
		}
	}

	getModelInfoString() {
		let pmod = this.currentModel();
		let s = '';
		s += 'model name: ' + pmod.name + '\n';
		s += 'physs serial number: ' + pmod.serialNum + '\n';
		s += 'record index: ' + pmod.recordIndex + '\n';
		s += 'spectrum acquisition date/time: ' + pmod.dateTime + '\n';
		s += 'lab name: ' + pmod.labName + '\n';
		s += 'technician name: ' + pmod.techName + '\n';
		s += 'spectrometer serial number: ' + pmod.spectSerialNum + '\n';
		s += 'wavelengths: ' + pmod.wavelengths.slice(0,3) + ' ... ' +
							   pmod.wavelengths.slice(-3) + '\n';
		s += 'nonlinear correction coeficients: ' + 
			  JSON.stringify(pmod.nlcCoef) + '\n';
		s += 'cell concentration: ' + pmod.concentration + '\n';
		s += 'chlorophyll concentration: ' + pmod.chlorophyll + '\n';
		s += 'comment: ' + pmod.comment + '\n';

		return s;
	}
};
