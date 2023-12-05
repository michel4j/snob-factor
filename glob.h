#include "snob.h"

/*	--------------  Global variables declared here  ----------------  */

#ifndef GLOBALS
#define EXT extern
#else
#define EXT
#endif

/*	mathematical constants   */
EXT double hlg2pi, hlg2, lattice, pi, bit, bit2, twoonpi, pion2;
EXT double zerov[MAX_ZERO];
EXT double faclog[MAX_CLASSES + 1];

/*	general:	*/
EXT int Ntypes;     /* The number of different attribute types */
EXT VarType *types; /* a vector of Ntypes type definitions,
         created in do_types */
EXT Context CurCtx;    /* current context */
EXT VarSet *VarSets[MAX_VSETS];
EXT Sample *Samples[MAX_SAMPLES];
EXT Population *Populations[MAX_POPULATIONS];
EXT int nsamples;

/*	re inputs for main  */
EXT Buffer *source; /* Ptr to command source buffer */

/*	re Sample records  */
EXT char *record; /*  Common ptr to a data record  */
EXT int item;     /*  Index of current item  */
EXT char *loc;    /*  Common ptr to a data field  */
EXT int reclen;   /*  reclen of current sample  */
EXT char *recs;   /*  Common ptr to data records block of a sample */

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

/*	re Poplns  */
EXT VarSet *VSet;
EXT Sample *Smpl;
EXT Population *Popln;
EXT AVinst *avi;
EXT PVinst *pvi;
EXT SVinst *svi;
EXT VarType *vtp;
EXT int nc; /* Number of cases */
EXT int nv; /* Number of variables */
EXT int root;
EXT Class *rootcl;
EXT AVinst *vlist;
EXT PVinst *pvars;
EXT SVinst *svars;

/*	re Classes  */
EXT Class *cls, *dad;
EXT short *vv;
EXT double cwt; /*  weight of case in class  */
EXT double cvv, cvvsq, cvvsprd;
EXT double ctv, ctvsq, ctvd1, ctvd1sq, ctvd1cu, ctvsprd, ctd1d2;
EXT int icvv; /*  integer form of cvv*4096 */
EXT double ncasecost, scasecost, fcasecost;
EXT double vvd1, vvd2; /* derivs of case cost wrt score  */
EXT double mvvd2;      /* An over-estimate of vvd2 used in score ajust */
EXT double vvd3;       /*  derivative of vvd2 wrt score  */
EXT int idad;

/*	re Doall   */
EXT int RSeed; /*	Seed for random routines */
EXT int nosubs;
EXT int newsubs;
EXT int deaded; /* Shows some class killed */
EXT int numson;
EXT Class *sons[MAX_CLASSES];
EXT int nextic[MAX_CLASSES];

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
EXT int dontignore;     /* A flag signalling a major change to structure */
EXT int scorechanges;
EXT int NewSubsTime; /* Period for making new subclasses */
EXT double InitialAdj;
EXT double MaxAdj;
EXT double MinGain; /* smallest decrease in cost taken as success */
EXT double Mbeta;   /* A constant compensating for a deficiency in mults.c */
EXT double Bbeta;
EXT int RootAge;
EXT int GiveUp; /* Num of nojoy steps until stopping */

/*	re Badmoves  */
EXT int badkey[BadSize];