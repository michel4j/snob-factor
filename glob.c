/*	This file contains type definitions for generic and global structures,
and declarations of cetain global variables. The file is intended to be
included in many other files, as well as being compiled and loaded itself.
	For variables, arrays etc defined herein, the instantiation is
suppressed when included in other files by #define NOTGLOB 1 in those other
files. The declarations herein then become converted to "extern" declarations.
	*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define Sf double
#define Sw int
#define Sh short
#define Sc char

#define Maxsamples 10  /* Max number of samples */
#define Maxvsets 3
#define Maxpoplns 10	/* Max number of popln models */
#define MaxClasses 150
#define MaxZ 100	/*  Length of the Zero vector  */
#define LL 450	/*	Length of input line buffer */

/*	-----------------  Control  ------------------------------   */

#define AdjSc 1
#define AdjTr 2
#define AdjPr 4
#define AdjSP 5
#define AdjAll 7
/*	The above define mask bits in 'control' to control
adjustment of weights, scores, structure, parameters
respectively  */
#define Tweak 8
/*	A special control bit to cause params to be adjusted in response
to a change in dad's params, without a full recalculation.  */
#define Noprior 16
/*	Special bit to guess no-prior params, spreads. Tweak, Noprior only
work on leaves without sons (i.e. without subs)  */


#define Partial 1
#define Most_likely 2
#define Random 3
/*	These are values for 'dfix' which normally
controls the mode of weight assignment  */



/*	-----------------  Variable types  ---------------------  */

typedef struct Vtypest {
	Sw id;
	Sw datsize;
	Sw vauxsize;	/* Size of aux block for vartype in vlist */
	Sw sauxsize;	/* size of aux block for vartype in sample */
	Sw pauxsize;	/* size of aux block for vartype in popln */
	Sc *name;
	Sw (*readvaux)();	/* Fun to read aux attribute info */
	Sw (*readsaux)();	/* Fun to read aux sample info */
	Sw (*readdat) ();	/* Fun to read a datum */
	void (*printdat) ();	/* Fun to print datum value */
	void (*setsizes) ();	/* To set basicsize, statssize */
	void (*setbestparam) ();	/* To set current best use params */
	void (*clearstats) ();	/* To clear stats prior to re-estimation */
	void (*scorevar) ();
	void (*derivvar) ();
	void (*costvar) ();
	void (*ncostvar) ();
	void (*adjust) ();
	void (*vprint) ();
	void (*setvar) ();
	} Vtype;


/*	-------------------  Files ----------------------------------  */

typedef struct Bufst	{
	FILE *cfile;
	Sw line, nch;
	Sc cname [80];
	Sc inl [LL];
	}	Buf;

/*	------------------  Allocation blocks  --------------------  */
typedef struct Blockst {
	struct Blockst *nblock;  Sw size;
	}  Block;


/*	-------------------  Attributes  -------------------------   */

typedef struct AVinstst {
	Sw id;
	Sw itype;
	Sw idle;	/* Inactive attribute flag */
	Sw basicsize;   /*  Sizeof basic block (CVinst) for this var */
	Sw statssize;   /* Sizeof stats block (EVinst) for this var */
	Vtype *vtp;
	char *vaux;
	Sc name [80];
	} AVinst;

typedef struct Vsetst {
	Sw id;
	Block *vblks;		/* Ptr to chain of blocks allocated */
	char dfname [80];  /* file name of vset */
	char name [80];
	Sw nv;	/* Number of variables */
	Sw nactv;	/* Number of active variables */
	AVinst *vlist;
	}	Vset;


/*	--------------------  Samples  ---------------------------   */

typedef struct SVinstst {
	Sw id;
	Sw nval;
	char *saux;
	Sw offset;	/*  offset of (missing, value) in record  */
	}  SVinst;


/*	Sample data is packed into a block of 'records' addressed by
the 'recs' pointer in a Sample structure. There is one record per thing in the
sample. Each record has the following format:
	char active_flag. If zero, the thing is ignored in building classes.
	Sw ident	The thing identifier, as a positive integer.
  Then follow 'nv' fields for the attribute values of the thing. Each field
actually has two parts:
	char missing_flag  If non-zero, shows value is unknown .
	Datum value.  The type Datum depends on the type of attribute. This
		field is present even if the missing flag is on, but contains
		garbage.
	*/

typedef struct Samplest	{
	Sw id;
	Block *sblks;	/* Ptr to chain of blocks allocated for sample */
	char vstname [80];	/* Name of variable-set */
	Sw nc;		/* Num of cases */
	Sw nact;	/* Num of active cases */
	SVinst *svars;	/* Ptr to vector of SVinsts, one per variable */
	char *recs;	/*  vector of records  */
	Sw reclen;	/*  Length in chars of a data record  */
	Sf bestcost;	/*  Cost of best model */
	Sw goodtime;	/*  Popln age when bestcost reached  */
	Sc name[80];
	Sc dfname[80];	/*  Data file name  */
	}  Sample;






/*	----------------------   Classes  ---------------------------     */


typedef struct CVinstst{ /*Structure for basic info on a var in a class*/
	Sw id;
	Sw signif;
	Sw infac;	/* shows if affected by factor  */
	}	CVinst;


typedef struct EVinstst	{/* Stuff for var in class in expln */
	Sf cnt;		/*  Num of values */
	Sf btcost, ntcost, stcost, ftcost;
	Sf bpcost, npcost, spcost, fpcost;
	Sf vsq;		/* weighted sum of squared scores */
	Sw id;
	}	EVinst;



typedef struct Classst	{
	Sf relab;
	Sf mlogab;	/*  - log relab  */
	Sf cbcost;	/* Best total class cost  */
	Sf cncost, cscost, cfcost;  /* Class costs as dad, sansfac, confac */
	Sf cbpcost;
	Sf cnpcost, cspcost, cfpcost;   /* Parameter costs in above */
	Sf cbtcost;
	Sf cbfcost;	/*  Used to track best cfcost to detect improvement*/
	Sf cnt;		/* sum of weights of members  */
	Sf vsq;		/* Sum of squared scores */
	Sf vboost;	/* Used to inflate score vector early on  */
	Sf avvv;	/* average vv  */
	Sc type;	/* 0 = ?, 1 = root, 2 = dad, 3 = leaf, 4 = sub */
	Sc holdtype;
	Sc use;		/* Current use: 1=sansfac, 2=confac */
	Sc holduse;
	Sw boostcnt;	/*  Monitors need to boost vsq  */
	Sw scorechange;	/*  Counts significant score changes  */
	Sw age;		/*  age in massage counts  */
	Sw idad, isib, ison;	/* id links in class hierarchy */
	Sw nson;	/* Number of son classes */
	Sw serial;
/*	******************* Items above this line must be distributed to
			all remotes before each pass through the data.
		The items in the next group are accumulated and must be
		returned to central.  They are cleared to zero by cleartcosts.
		*/
	Sf newcnt;	/*  Accumulates weights for cnt  */
	Sf newvsq;	/*  Accumulates squared scores for vsq  */
	Sf cfvcost;	/*  Factor-score cost included in cftcost  */
	Sf cntcost, cstcost, cftcost;	/* Thing costs in above */
	Sf vav;		/* sum of log vvsprds */
	Sf totvv;	/* sum of vvs  */
	Sw scancnt;	/*  Number of things considered  */
/*	********************  Items below here are generated locally by
		docase for each case, and need not be distributed or
		returned  */
	Sw caseiv;	/*  Integer score of current case  */
	Sf casecost;	/*  tcost of current case  */
	Sf casescost;	/* Cost of current case in no-fac class */
	Sf casefcost;	/* """"""""""""""""""""""" factor class */
	Sf casevcost;	/* Part of casefcost due to coding score */
	Sf casencost;	/* """""""""""""""""""""""  dad   class */
	Sf casewt;	/*  weight of current case  */
	Sf cvv, cvvsq, cvvsprd, clvsprd;
/*	*******************
	Items below this line are set up when class is made by makeclass()
and should NOT be copied to a new class structure. IT IS ASSUMED THAT
'ID' IS THE FIRST ITEM BELOW THE LINE.
	Except for id, which never changes and is set by central, the other
	items are pointers which will be set by remotes, will not thereafter
	change, but may have different values in different machines.
	********************* */
	Sw id;
	Sh *vv;		/* Factor scores */
			/* Scores times 4096 held as signed shorts in +-30000 */
	CVinst **basics;	/* ptr to vec of ptrs to variable basics */
	EVinst **stats;		/* ptr to vec of ptrs to variable stats blocks*/
	}	Class;

#define Maxv ((Sf) 5.0)
#define ScoreScale ((Sf) 4096.0)
#define HScoreScale ((Sf) 2048.0)
#define ScoreRscale ((Sf) (1.0 / ScoreScale))

#define Dad 1
#define Leaf 2
#define Sub 4
#define Vacant 64
#define Tiny 0
#define Plain 1
#define Fac 2


/*	--------------------------  Population  ----------------------  */

typedef struct PVinstst	{
	Sw id;
	char *paux;
	}	PVinst;


typedef struct Poplnst	{
	Sw id;
	Block *pblks, *jblks;	/* Ptrs to bocks allocated for popln,
		and for popln as model of sample */
	char vstname [80];	/* Name of variable-set */
	Sc samplename[80];  /*  Name of sample to which popln is attached if any*/
	Sw nc;		/*  Size of sample attached, or 0 */
	Sw samplesize;	/*  num of active cases in sample used for training */
	Class **classes;	/* ptr to vec of ptrs to classes  */
	PVinst *pvars;	/* Ptr to vector of PVinsts, one per variable */
	Sc dfname[80];	/*  Popln file name  */
	Sc name[80];
	Sw nextserial;	/*  Next serial number for a new class */
	Sw ncl;		/* Number of classes  */
	Sw nleaf;	/* Number of leaves */
	Sw root;	/* index of root class  */
	Sw mncl;	/*  Length of 'classes' vec. */
	Sw hicl;	/*  Highest allocated entry in pop->classes */
	}  Popln;

/*	-------------------------------------------------------------   */

typedef struct Contextst	{
	Vset *vset;
	Sample *sample;
	Popln *popln;
	Buf *buffer;
	}  Context;


/*	--------- Functions -------------------------------   */

/*	In LISTEN.c	*/
	Sw hark (Sc *lline);
/*		end listen.c		*/

/*	In INPUTS.c  */
#ifndef INPUTS
	extern Sw terminator;
	extern Buf cfilebuf, commsbuf;
#endif
	Sw bufopen();
	Sw newline();
	Sw readint (Sw *x, Sw cnl);
	Sw readdf (Sf *x, Sw cnl);
	Sw readalf (Sc *str, Sw cnl);
	Sw readch (Sw cnl);
	void swallow ();
	void bufclose ();
	void revert (Sw flag);
	void rep (Sw ch);
	void flp ();
/*		end inputs.c		*/

/*	In POPLNS.c   */
	void nextclass (Class **ptr);
	Sw makepop(Sw fill);
	Sw firstpop();
	void makesubs (Sw kk);
	void setpop();
	void killpop (Sw px);
	Sw copypop (Sw p1, Sw fill, Sc *newname);
	Sw savepop (Sw p1, Sw fill, Sc *newname);
	Sw restorepop (Sc *nam);
	Sw loadpop (Sw pp);
	void printtree();
	Sw bestpopid ();
	void trackbest(Sw verify);
	Sw pname2id (Sc *nam);
	void correlpops (Sw xid);
/*		end poplns.c		*/


/*	In CLASSES.c	*/
	Sw s2id (Sw ss);
	Sw makeclass ();
	void cleartcosts (Class *ccl);
	void setbestparall (Class *ccl);
	void scorevarall (Class *ccl);
	void costvarall (Class *ccl);
	void derivvarall (Class *ccl);
	void ncostvarall (Class *ccl, Sw valid);
	void adjustclass (Class *ccl, Sw dod);
	void killsons (Sw kk);
	void printclass (Sw kk, Sw full);
	void setclass1 (Class *ccl);
	void setclass2 (Class *ccl);
	Sw splitleaf (Sw kk);
	void deleteallclasses();
	Sw nextleaf (Popln *cpop, Sw iss);
/*		end classes.c		*/


/*	In DOALL.c	*/
	Sw doall (Sw ncy, Sw all);
	void findall (Sw typ);
	Sw dodads (Sw ncy);
	Sw dogood (Sw ncy, Sf target);
	void tidy (Sw hit);
	Sw uran();
	Sw sran ();
	Sf fran();
	void docase (Sw cse, Sw all, Sw derivs);

#define Dead -10
#define Deadsmall -11
#define Deadsing  -12
#define Deadorphan -13
#define Deadkilled -14
/*		end doall.c		*/


/*	In TUNE.c	*/
	void defaulttune();
/*		end tune.c	*/


/*	In TACTICS.c	*/
	void flatten();
	Sf insdad(Sw ser1, Sw ser2, Sw *dadid);
	Sw bestinsdad (Sw force);
	Sf deldad (Sw ser);
	Sw bestdeldad ();
	void rebuild ();
	void ranclass (Sw nn);
	void binhier (Sw flat);
	Sf moveclass (Sw ser1, Sw ser2);
	Sw bestmoveclass (Sw force);
	void trymoves (Sw ntry);
	void trial (Sw param);
/*		end tactics.c		*/


/*	In BADMOVES.c	*/
#define BadSize 1013
	 void clearbadm ();
	 Sw testbadm (Sw code, Sw w1, Sw w2);
	 void setbadm (Sw code, Sw s1, Sw s2);
/*		end badmoves.c		*/


/*	In BLOCK.c	*/
	void *gtsp (Sw gr, Sw size);
	void freesp (Sw gr);
	Sw repspace (Sw pp);
/*		end block.c		*/


/*	In DOTYPES.c	*/
	void dotypes ();
/*		end dotypes.c		*/


/*	In SAMPLES.c	*/
	void printdatum (Sw i, Sw n);
	Sw readvset ();
	Sw readsample (Sc *fname);
	Sw sname2id (Sc *nam, Sw expect);
	Sw vname2id (Sc *nam);
	Sw qssamp (Sample *samp);
	Sw id2ind (Sw id);
	Sw thinglist (Sc *tlstname);
/*		end samples.c		*/


/*	In GLOB.c	*/
	void cmcpy (void *pt, void *pf, Sw ntm);
 	Sc* sers (Class *cll);
/*		end glob.c		*/


/*	-----------------  routines defined here  -------------------  */
#ifndef NOTGLOB

/*	A memory-to-memory copier replacing memcpy, which I can't use
properly on all systems.   */
void cmcpy (pt, pf, ntm)
	void *pt, *pf;  Sw ntm;
{
	char *cpt, *cpf;

	cpt = (char*) pt;  cpf = (char*) pf;
/*	Copy ntm chars from pf to pt  */
next:
	*cpt = *cpf;  cpt++;  cpf++;
	ntm--;  if (ntm) goto next;
	return;
}


/*	To assist in printing class serials  */
char decsers [8];

char* sers (cll)
	Class *cll;
{
	Sw i, j, k;
	for (i = 0; i < 6; i++) decsers[i] = ' ';  decsers[6] = 0;
	if (! cll) goto done;
	if (cll->type == Vacant) goto done;
	j = cll->serial;
	k = j & 3;
	if (k == 1) decsers[5] = 'a';
	else if (k == 2) decsers[5] = 'b';
	j = j >> 2;
	decsers[4] = '0';
	for (i = 4; i > 0; i--)	{
		if (j <= 0) goto done;
		decsers[i] = j % 10 + '0';  j = j / 10;
		}
	if (j) { strcpy (decsers+1, "2BIG"); }
done:	return (decsers);
}
#endif		/* end of function definitions in glob.c */


/*	--------------  Global variables declared here  ----------------  */
#ifdef NOTGLOB
#define Ex extern
#else
#define Ex
#endif

/*	mathematical constants   */
	Ex Sf hlg2pi, hlg2, lattice, pi, bit, bit2, twoonpi, pion2;
	Ex Sf zerov [MaxZ];
	Ex Sf faclog [MaxClasses + 1];

/*	general:	*/
	Ex Sw Ntypes;		/* The number of different attribute types */
	Ex Vtype *types;	/* a vector of Ntypes type definitions,
				created in dotypes */
	Ex Context ctx;	/* current context */
	Ex Vset *vsets[Maxvsets];
	Ex Sample *samples[Maxsamples];
	Ex Popln *poplns [Maxpoplns];
	Ex Sw nsamples;

/*	re inputs for main  */
	Ex Buf *source;		/* Ptr to command source buffer */

/*	re Sample records  */
	Ex char *rec;	/*  Common ptr to a data record  */
	Ex Sw thing;	/*  Index of current thing  */
	Ex char *loc;		/*  Common ptr to a data field  */
	Ex Sw reclen;	/*  reclen of current sample  */
	Ex char *recs;		/*  Common ptr to data records block of a sample */

/*	re hark  */
	Ex Sw heard;	/* Flag showing a new command line has been detected */
	Ex Sw usestdin;	/* Flag showing input from sdtdin */

	Ex Sw trapkk, trapage, trapcnt;

/*	To control what is adjusted  */
	Ex Sw control, dcontrol;

/*	To determine how weights are distributed  */
	Ex Sw dfix, fix;

/*	re Poplns  */
	Ex Vset *vst;
	Ex Sample *samp;
	Ex Popln *pop;
	Ex AVinst *avi;
	Ex PVinst *pvi;
	Ex SVinst *svi;
	Ex Vtype *vtp;
	Ex Sw nc;  /* Number of cases */
	Ex Sw nv;	/* Number of variables */
	Ex Sw root;
	Ex Class *rootcl;
	Ex AVinst *vlist;
	Ex PVinst *pvars;
	Ex SVinst *svars;

/*	re Classes  */
	Ex Class *cls, *dad;
	Ex Sh *vv;
	Ex Sf cwt;	/*  weight of case in class  */
	Ex Sf cvv, cvvsq, cvvsprd;
	Ex Sf ctv, ctvsq, ctvd1, ctvd1sq, ctvd1cu, ctvsprd, ctd1d2;
	Ex Sw icvv;	/*  integer form of cvv*4096 */
	Ex Sf ncasecost, scasecost, fcasecost;
	Ex Sf vvd1, vvd2;  /* derivs of case cost wrt score  */
	Ex Sf mvvd2;	/* An over-estimate of vvd2 used in score ajust */
	Ex Sf vvd3;	/*  derivative of vvd2 wrt score  */
	Ex Sw idad;

/*	re Doall   */
	Ex Sw rseed;	/*	Seed for random routines */
	Ex Sw nosubs;
	Ex Sw newsubs;
	Ex Sw deaded;	/* Shows some class killed */
	Ex Sw numson;
	Ex Class *sons[MaxClasses];
	Ex Sw nextic[MaxClasses];

/*	re Tuning  */
	Ex Sw MinAge; 	/* Min class age for creation of subs */
	Ex Sw MinFacAge;		/* Min age for contemplating a factor model */
	Ex Sw MinSubAge;    /* Until this age, subclass relabs not allowed to
			decay.  */
	Ex Sw MaxSubAge; 	/* Subclasses older than this are replaced  */
	Ex Sw HoldTime;	/* Massage cycles for which class type is fixed
		following forcible change  */
	Ex Sw Forever;		/* If holdtype set to this, it isnt counted down */
	Ex Sf MinSize ;	/* Min size for a class */
	Ex Sf MinWt;	/* Min weight for a case to be treated in a class */
	Ex Sf MinSubWt;	/* Min fractional wt for a case to be treated in a Sub */
	Ex Sw SigScoreChange;	/*  Min significant change in factor score */
	Ex Sw seeall; /* A flag to make all cases be treated in all classes */
	Ex Sw dontignore;	/* A flag signalling a major change to structure */
	Ex Sw scorechanges;
	Ex Sw NewSubsTime;	/* Period for making new subclasses */
	Ex Sf InitialAdj; 
	Ex Sf MaxAdj;     
	Ex Sf MinGain;	/* smallest decrease in cost taken as success */
	Ex Sf Mbeta;	/* A constant compensating for a deficiency in mults.c */
Ex Sf Bbeta;
	Ex Sw RootAge;
	Ex Sw GiveUp;		/* Num of nojoy steps until stopping */

/*	re Badmoves  */
	Ex Sw badkey  [BadSize];

/*	re functions defined in glob.c   */




/*	zzzzzzzzzzzzzzzzzzzzzzzz  */
