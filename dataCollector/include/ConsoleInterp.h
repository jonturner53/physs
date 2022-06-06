/** \file ConsoleInterp.h *  @author Jon Turner
 *  @date 2017
 *
 *  This software was developed for Mote Marine Research Laboratory.
 */

#ifndef CONSOLEINTERP_H
#define CONSOLEINTERP_H

#include "stdinc.h" 
#include <mutex> 
#include "Interrupt.h"
#include "ScriptInterp.h"

using namespace std;

namespace fizz {

/** This class implements a console interpreter that runs as a separate
 *  thread and responds to commands from a remote console program.
 *  The console interpreter listens 
 */
class ConsoleInterp{
public:		ConsoleInterp();

	void	begin();
	void	end();
	void	join();
	bool	zombie() { return zombieFlag; }

private:
	void	run();
	bool	readScript();
	void	doCommand(vector<string>&);
	void	pumpControl(vector<string>&);
	void	valveControl(vector<string>&);
	void	fluidSupplyControl(vector<string>&);
	void	snapshot(vector<string>&);

	string	scriptPath;

	bool	autoRunFlag;		///< cleared when sampling first enabled
	bool	quitFlag;
	bool	zombieFlag;

	bool	calibrationInProgress;
	string	pumpInCalibration;
	double	calibrationT0;

	void	reply(const string&);

	mutex	mtx;
	thread	myThread;
	thread::id thread_id;
	static void startThread(ConsoleInterp&);
};

} // ends namespace

#endif
