/** \file DataStore.h
 *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef DATASTORE_H
#define DATASTORE_H

#include "stdinc.h" 
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <queue>
#include "Logger.h"
#include "Config.h"
#include "CollectorState.h"

using namespace std;

namespace fizz {

/** This class implements a DataStore object that saves sample data to
 *  an external file in json format.
 */
class DataStore {
public:		DataStore();
	void	initState();

	void	run();		///< function called by thread constructor

	bool	open();
	void	close();

	int getSpectrumCount() {
	    unique_lock<mutex> lck(dataStoreMtx);
	    return spectrumCount;
	}

	void	saveDeploymentRecord();
	void	saveScriptRecord();
	void	saveConfigRecord();
	void	saveMaintLogRecord();
	void	saveResetRecord();
	void	saveSpectrumRecord(vector<double>&, const string&,
				   const string& = "", const string& = "");
	void	saveCycleSummary();
	void	saveDebugRecord(const string&);

	friend	class CollectorState;

private:
	bool	privateOpen();

	int		currentIndex;		///< index of current record
	int		deploymentIndex;	///< index of deployment record
	int		spectrumCount;		///< # of spectra since last deployment record

	unordered_map<string,int> recordMap;
					///< map used to track record labels

	string	filePath(const string&, int);	
							///< path name to raw data file
	ofstream dataFile;		///< stream for output file
	bool	openFlag;		///< true if rawFile is open
	bool	indexFlag;		///< set when deployment index is set

	mutex   dataStoreMtx;		///< used to synchronize methods

	queue<string> debugStrings;	///< used to dodge deadlock
	mutex   dbsMtx;				///< used to lock debugStrings

	string	encodeConfigScript(string&);
};

} // ends namespace

#endif
