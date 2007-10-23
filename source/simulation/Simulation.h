#ifndef INCLUDED_SIMULATION
#define INCLUDED_SIMULATION

#include <boost/random.hpp>

class CGame;
class CGameAttributes;
class CWorld;
class CTurnManager;
class CNetMessage;

class CSimulation
{
	CGame *m_pGame;
	CWorld *m_pWorld;
	CTurnManager *m_pTurnManager;
	
	// Current game time; store as double for precision
	double m_Time;
	
	double m_DeltaTime;

	// Random number generator
	boost::mt19937 m_Random;
	
	// Simulate: move the deterministic simulation forward by one interval
	void Simulate();

	// Interpolate: interpolate a data point for rendering between simulation
	// frames.
	void Interpolate(double frameTime, double offset);

public:
	CSimulation(CGame *pGame);
	~CSimulation();
	
	inline void SetTurnManager(CTurnManager *pTurnManager)
	{	m_pTurnManager=pTurnManager; }
	inline CTurnManager *GetTurnManager()
	{	return m_pTurnManager; }

	void RegisterInit(CGameAttributes *pGameAttributes);
	int Initialize(CGameAttributes *pGameAttributes);

	// Perform simulation updates for the specified elapsed time. If it is
	// shorter than the time until the next simulation turn, nothing happens.
	// Returns false if it can't keep up with the desired simulation rate.
	bool Update(double frameTime);

	// If the last Update couldn't keep up with the desired rate, ignore that
	// and don't try to catch up when Update is called again. Will completely break
	// synchronisation of sim time vs real time.
	void DiscardMissedUpdates();

	// Update the graphical representations of the simulation by the given time.
	void Interpolate(double frameTime);

	// Calculate the message mask of a message to be queued
	static uint GetMessageMask(CNetMessage *, uint oldMask, void *userdata);
	
	// Translate the command message into an entity order and queue it
	// Returns oldMask
	static uint TranslateMessage(CNetMessage *, uint oldMask, void *userdata);

	void QueueLocalCommand(CNetMessage *pMsg);

	// Get a random integer between 0 and maxVal-1 from the simulation's random number generator
	int RandInt(int maxVal);

	// Get a random float in [0, 1) from the simulation's random number generator
	float RandFloat();
	
	// Get game time
	inline double GetTime() { return m_Time; }
};

#endif
