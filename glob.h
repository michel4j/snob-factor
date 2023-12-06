#include "snob.h"

/*	--------------  Global variables declared here  ----------------  */

#ifndef GLOBALS
#define EXT extern
#else
#define EXT
#endif

/*	mathematical constants   */
EXT double HALF_LOG_2PI, HALF_LOG_2, LATTICE, PI, BIT, TWOBIT, TWO_ON_PI, HALF_PI;
EXT double ZeroVec[MAX_ZERO];
EXT double FacLog[MAX_CLASSES + 1];

/*	general:	*/
EXT int NTypes;     /* The number of different attribute types */
EXT VarType *Types; /* a vector of Ntypes type definitions,    created in dotypes */
EXT Context CurCtx; /* current context */
EXT VarSet *VarSets[MAX_VSETS];
EXT Sample *Samples[MAX_SAMPLES];
EXT Population *Populations[MAX_POPULATIONS];
EXT int NSamples;

/*	re inputs for main  */
EXT Buffer *CurSource; /* Ptr to command source buffer */

/*	re Sample records  */
EXT char *CurRecord;  /*  Common ptr to a data record  */
EXT int CurItem;      /*  Index of current item  */
EXT char *CurField;   /*  Common ptr to a data field  */
EXT int CurRecLen;    /*  reclen of current sample  */
EXT char *CurRecords; /*  Common ptr to data records block of a sample */

/*	re hark  */
EXT int Heard;    /* Flag showing a new command line has been detected */
EXT int UseStdIn; /* Flag showing input from sdtdin */
EXT int UseLib;   // Flag showing if snob is being called as a library
EXT int Debug;    // Flag to turn toggle debug messages
EXT int UseBin;

EXT int trapkk, trapage, trapcnt;

/*	To control what is adjusted  */
EXT int Control, DControl;

/*	To determine how weights are distributed  */
EXT int DFix, Fix;
EXT Score Scores;

/*	re Poplns  */
EXT VarSet *CurVSet;
EXT Sample *CurSample;
EXT Population *CurPopln;
EXT VSetVar *CurAttr;
EXT PVinst *CurPopVar;
EXT SampleVar *CurVar;
EXT VarType *CurVType;
EXT int NumCases; /* Number of cases */
EXT int NumVars;  /* Number of variables */
EXT int CurRoot;
EXT Class *CurRootClass;
EXT VSetVar *CurAttrList;
EXT PVinst *CurPopVarList;
EXT SampleVar *CurVarList;

/*	re Classes  */
EXT Class *CurClass, *CurDad;
EXT double CurCaseWeight; /*  weight of case in class  */
EXT double CurCaseFacScore, CurCaseFacScoreSq, cvvsprd;
EXT int CaseFacInt; /*  integer form of cvv*4096 */
EXT double NCaseCost, CaseNoFacCost, CaseFacCost;
EXT double CaseFacScoreD1, CaseFacScoreD2; /* derivs of case cost wrt score  */
EXT double EstFacScoreD2;                  /* An over-estimate of vvd2 used in score ajust */
EXT double CaseFacScoreD3;                 /*  derivative of vvd2 wrt score  */

/*	re Doall   */
EXT int RSeed; /*	Seed for random routines */
EXT int NoSubs;
EXT int NewSubs;
EXT int Deaded; /* Shows some class killed */
EXT Class *Sons[MAX_CLASSES];
EXT int NextIc[MAX_CLASSES];

/*	re Tuning  */
EXT int MinAge;         /* Min class age for creation of subs */
EXT int MinFacAge;      /* Min age for contemplating a factor model */
EXT int MinSubAge;      /* Until this age, subclass relabs not allowed to
          decay.  */
EXT int MaxSubAge;      /* Subclasses older than this are replaced  */
EXT int HoldTime;       /* Massage cycles for which class type is fixed
       following forcible change  */
EXT int Forever;        /* If holdtype set to this, it isnt counted down */
EXT double MinSize;     /* Min size for a class */
EXT double MinWt;       /* Min weight for a case to be treated in a class */
EXT double MinSubWt;    /* Min fractional wt for a case to be treated in a Sub */
EXT int SigScoreChange; /*  Min significant change in factor score */
EXT int SeeAll;         /* A flag to make all cases be treated in all classes */
EXT int DontIgnore;     /* A flag signalling a major change to structure */
EXT int ScoreChanges;
EXT int NewSubsTime; /* Period for making new subclasses */
EXT double InitialAdj;
EXT double MaxAdj;
EXT double MinGain; /* smallest decrease in cost taken as success */
EXT double Mbeta;   /* A constant compensating for a deficiency in mults.c */
EXT double Bbeta;
EXT int RootAge;
EXT int GiveUp; /* Num of nojoy steps until stopping */

/*	re Badmoves  */
EXT int BadKey[BadSize];