/*	---------------- stuff to tune actions -------------------------- */
#define NOTGLOB 1
#define TUNE
#include "glob.c"

/*	----------------  defaulttune -------------------------------- */
void defaulttune ()
{
	Sw i;

	MinAge = 4;
	MinFacAge = 11;
	MinSubAge = 6;
	MaxSubAge = 50;
	HoldTime = 15;
	Forever = 100;
	MinSize = 4.0;
	NewSubsTime = 10;
	InitialAdj = 0.3;
	MaxAdj = 1.3;
	RootAge = 25;
	MinGain = 0.01;
	GiveUp = 6;
	MinWt = 0.005;
	MinSubWt = 0.01;
	SigScoreChange = 5;
	Mbeta = 0.00;

/*	Set table of log factorials  */
	faclog[0] = faclog[1] = 0.0;
	for (i = 2; i <= MaxClasses; i++)
		faclog[i] = faclog[i-1] + log ((double) i);
	return;
}
