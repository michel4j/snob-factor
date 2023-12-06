/*	This file contains type definitions for generic and global structures,
and declarations of cetain global variables. The file is intended to be
included in many other files, as well as being compiled and loaded itself.
    For variables, arrays etc defined herein, the instantiation is
suppressed when included in other files by #define NOTGLOB 1 in those other
files. The declarations herein then become converted to "EXT" declarations.
    */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_SAMPLES 10 /* Max number of samples */
#define MAX_VSETS 3
#define MAX_POPULATIONS 10 /* Max number of popln models */
#define MAX_CLASSES 150
#define MAX_ZERO 100          /*  Length of the Zero vector  */
#define INPUT_BUFFER_SIZE 450 /*    Length of input line buffer */

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

#define Maxv ((double)5.0)
#define ScoreScale ((double)4096.0)
#define HScoreScale ((double)2048.0)
#define ScoreRscale ((double)(1.0 / ScoreScale))

#define Dad 1
#define Leaf 2
#define Sub 4
#define Vacant 64
#define Tiny 0
#define Plain 1
#define Fac 2
#define Dead -10
#define Deadsmall -11
#define Deadsing -12
#define Deadorphan -13
#define Deadkilled -14
#define BadSize 1013

/*	-----------------  Variable types  ---------------------  */

typedef struct VarTypeStruct {
    int id;
    int data_size;
    int attr_aux_size; /* Size of aux block for vartype in vlist */
    int smpl_aux_size; /* size of aux block for vartype in sample */
    int pop_aux_size; /* size of aux block for vartype in popln */
    char *name;
    int (*read_aux_attr)();      /* Fun to read aux attribute info */
    int (*read_aux_smpl)();      /* Fun to read aux sample info */
    int (*read_datum)();       /* Fun to read a datum */
    void (*print_datum)();     /* Fun to print datum value */
    void (*set_sizes)();     /* To set basicsize, statssize */
    void (*set_best_pars)(); /* To set current best use params */
    void (*clear_stats)();   /* To clear stats prior to re-estimation */
    void (*score_var)();
    void (*deriv_var)();
    void (*cost_var)();
    void (*cost_var_nonleaf)();
    void (*adjust)();
    void (*show)();
    void (*set_var)();
} VarType;

/*	-------------------  Files ----------------------------------  */

typedef struct BufferStruct {
    FILE *cfile;
    int line, nch;
    char cname[80];
    char inl[INPUT_BUFFER_SIZE];
} Buffer;

/*	------------------  Allocation blocks  --------------------  */
typedef struct BlockStruct Block;
struct BlockStruct {
    Block *next;
    int size;
};

/*	-------------------  Attributes  -------------------------   */

typedef struct VSetVarStruct {
    int id;
    int type;
    int inactive;      /* Inactive attribute flag */
    int basic_size; /*  Sizeof basic block (CVinst) for this var */
    int stats_size; /* Sizeof stats block (EVinst) for this var */
    VarType *vtype;
    char *vaux;
    char name[80];
} VSetVar;

typedef struct VSetStruct {
    int id;
    Block *blocks;    /* Ptr to chain of blocks allocated */
    char filename[80]; /* file name of vset */
    char name[80];
    int length;    /* Number of variables */
    int num_active; /* Number of active variables */
    VSetVar *variables;
} VarSet;

/*	--------------------  Samples  ---------------------------   */

typedef struct SampleVarStruct {
    int id;
    int nval;
    char *saux;
    int offset; /*  offset of (missing, value) in record  */
} SampleVar;

/*	Sample data is packed into a block of 'records' addressed by
the 'recs' pointer in a Sample structure. There is one record per item in the
sample. Each record has the following format:
    char active_flag. If zero, the item is ignored in building classes.
    int ident	The item identifier, as a positive integer.
  Then follow 'nv' fields for the attribute values of the item. Each field
actually has two parts:
    char missing_flag  If non-zero, shows value is unknown .
    Datum value.  The type Datum depends on the type of attribute. This
        field is present even if the missing flag is on, but contains
        garbage.
    */

typedef struct SampleStruct {
    int id;
    Block *blocks;     /* Ptr to chain of blocks allocated for sample */
    char vset_name[80]; /* Name of variable-set */
    int num_cases;           /* Num of cases */
    int num_active;         /* Num of active cases */
    SampleVar *variables;    /* Ptr to vector of SVinsts, one per variable */
    char *records;       /*  vector of records  */
    int record_length;       /*  Length in chars of a data record  */
    double best_cost;  /*  Cost of best model */
    int best_time;     /*  Popln age when bestcost reached  */
    char name[80];
    char filename[80]; /*  Data file name  */
} Sample;

/*	----------------------   Classes  ---------------------------     */

typedef struct CVinstStruct { /*Structure for basic info on a var in a class*/
    int id;
    int signif;
    int infac; /* shows if affected by factor  */
} CVinst;

typedef struct EVinstStruct { /* Stuff for var in class in expln */
    double num_values;               /*  Num of values */
    double btcost, ntcost, stcost, ftcost;
    double bpcost, npcost, spcost, fpcost;
    double sum_scores_sq; /* weighted sum of squared scores */
    int id;
} EVinst;

typedef struct ClassStruct {
    double relab;
    double mlogab;                 /*  - log relab  */
    double best_cost;                 /* Best total class cost  */
    double dad_cost, nofac_cost, fac_cost; /* Class costs as dad, sansfac, confac */
    double best_par_cost;
    double dad_par_cost, nofac_par_cost, fac_par_cost; /* Parameter costs in above */
    double best_case_cost;
    double best_fac_cost; /*  Used to track best cfcost to detect improvement*/
    double weights_sum;     /* sum of weights of members  */
    double sum_score_sq;     /* Sum of squared scores */
    double score_boost;  /* Used to inflate score vector early on  */
    double avg_factor_scores;    /* average vv  */
    char type;      /* 0 = ?, 1 = root, 2 = dad, 3 = leaf, 4 = sub */
    char hold_type;
    char use; /* Current use: 1=sansfac, 2=confac */
    char hold_use;
    int boost_count;         /*  Monitors need to boost vsq  */
    int score_change_count;      /*  Counts significant score changes  */
    int age;              /*  age in massage counts  */
    int dad_id, sib_id, son_id; /* id links in class hierarchy */
    int num_sons;             /* Number of son classes */
    int serial;
    /*	******************* Items above this line must be distributed to
                all remotes before each pass through the data.
            The items in the next group are accumulated and must be
            returned to central.  They are cleared to zero by cleartcosts.
            */
    double newcnt;                    /*  Accumulates weights for cnt  */
    double newvsq;                    /*  Accumulates squared scores for vsq  */
    double cfvcost;                   /*  Factor-score cost included in cftcost  */
    double cntcost, cstcost, cftcost; /* Thing costs in above */
    double vav;                       /* sum of log vvsprds */
    double totvv;                     /* sum of vvs  */
    int scancnt;                      /*  Number of things considered  */
    /*	********************  Items below here are generated locally by
            docase for each case, and need not be distributed or
            returned  */
    int case_score;       /*  Integer score of current case  */
    double total_case_cost;  /*  tcost of current case  */
    double nofac_case_cost; /* Cost of current case in no-fac class */
    double fac_case_cost; /* """"""""""""""""""""""" factor class */
    double coding_case_cost; /* Part of casefcost due to coding score */
    double dad_case_cost; /* """""""""""""""""""""""  dad   class */
    double case_weight;    /*  weight of current case  */
    double case_fac_score, case_fac_score_sq, cvvsprd, clvsprd;
    /*	*******************
        Items below this line are set up when class is made by makeclass()
    and should NOT be copied to a new class structure. IT IS ASSUMED THAT
    'ID' IS THE FIRST ITEM BELOW THE LINE.
        Except for id, which never changes and is set by central, the other
        items are pointers which will be set by remotes, will not thereafter
        change, but may have different values in different machines.
        ********************* */
    int id;
    short *factor_scores;       /* Factor scores */
                     /* Scores times 4096 held as signed shorts in +-30000 */
    CVinst **basics; /* ptr to vec of ptrs to variable basics */
    EVinst **stats;  /* ptr to vec of ptrs to variable stats blocks*/
} Class;

/*	--------------------------  Population  ----------------------  */

typedef struct PVinstStruct {
    int id;
    char *paux;
} PVinst;

typedef struct PoplnStruct {
    int id;
    Block *blocks, *model_blocks; /* Ptrs to bocks allocated for popln,
      and for popln as model of sample */
    char vst_name[80];     /* Name of variable-set */
    char sample_name[80];  /*  Name of sample to which popln is attached if any*/
    int sample_size;               /*  Size of sample attached, or 0 */
    int num_cases;       /*  num of active cases in sample used for training */
    Class **classes;      /* ptr to vec of ptrs to classes  */
    PVinst *variables;        /* Ptr to vector of PVinsts, one per variable */
    char filename[80];      /*  Popln file name  */
    char name[80];
    int next_serial; /*  Next serial number for a new class */
    int num_classes;        /* Number of classes  */
    int num_leaves;      /* Number of leaves */
    int root;       /* index of root class  */
    int cls_vec_len;       /*  Length of 'classes' vec. */
    int hi_class;       /*  Highest allocated entry in pop->classes */
} Population;

/*	-------------------------------------------------------------   */

typedef struct ContextStruct {
    VarSet *vset;
    Sample *sample;
    Population *popln;
    Buffer *buffer;
} Context;

/* Classification Result */
typedef struct ResultStruct {
    int num_classes;    // Number of classes found, includes Dads, Leaves and Subs
    int num_leaves;     // Number of leaves, these are the relevant categories
    double model_length;   // Cost of Transmitting Model 
    double data_length;   // Cost of Transmitting Data
    double message_length; // Total Cost
} Result;


/* Structur for calculating factor scores */
typedef struct ScoreStruct {
    double CurCaseFacScore, CurCaseFacScoreSq, cvvsprd;
    int CaseFacInt; /*  integer form of case_fac_score*4096 */
    double ncasecost, scasecost, fcasecost;
    double CaseFacScoreD1, CaseFacScoreD2; /* derivs of case cost wrt score  */
    double EstFacScoreD2;                     /* An over-estimate of CaseFacScoreD2 used in score ajust */
    double CaseFacScoreD3;
} Score;

/*	--------- Functions -------------------------------   */

/*	In LISTEN.c	*/
int hark(char *lline);
/*		end listen.c		*/

/*	In inputs.c  */
#ifndef INPUTS
extern int terminator;
extern Buffer cfilebuf, commsbuf;
#endif

int open_buffser();
int new_line();
int read_int(int *x, int cnl);
int read_double(double *x, int cnl);
int read_str(char *str, int cnl);
int read_char(int cnl);
void swallow();
void close_buffer();
void revert(int flag);
void rep(int ch);
void flp();
/*	end inputs.c  */

/*	In POPLNS.c   */
void next_class(Class **ptr);
int make_population(int fill);
int init_population();
void make_subclasses(int kk);
void set_population();
void destroy_population(int px);
int copy_population(int p1, int fill, char *newname);
int save_population(int p1, int fill, char *newname);
int load_population(char *nam);
int set_work_population(int pp);
void print_tree();
int get_best_pop();
void track_best(int verify);
int find_population(char *nam);
void correlpops(int xid);
/*		end poplns.c		*/

/*	In CLASSES.c	*/
int serial_to_id(int ss);
int make_class();
void clear_costs(Class *ccl);
void set_best_costs(Class *ccl);
void score_all_vars(Class *ccl);
void cost_all_vars(Class *ccl);
void deriv_all_vars(Class *ccl);
void parent_cost_all_vars(Class *ccl, int valid);
void adjust_class(Class *ccl, int dod);
void delete_sons(int kk);
void print_class(int kk, int full);
void set_class(Class *ccl);
void set_class_with_scores(Class *ccl);
int split_leaf(int kk);
void deleteallclasses();
int next_leaf(Population *cpop, int iss);
/*		end classes.c		*/

/*	In DOALL.c	*/
int doall(int ncy, int all);
void findall(int typ);
int dodads(int ncy);
int dogood(int ncy, double target);
void tidy(int hit);
int uran();
int sran();
double fran();
void docase(int cse, int all, int derivs);
/*		end doall.c		*/

/*	In TUNE.c	*/
void defaulttune();
/*		end tune.c	*/

/*	In TACTICS.c	*/
void flatten();
double insdad(int ser1, int ser2, int *dadid);
int bestinsdad(int force);
double deldad(int ser);
int bestdeldad();
void rebuild();
void ranclass(int nn);
void binhier(int flat);
double moveclass(int ser1, int ser2);
int bestmoveclass(int force);
void trymoves(int ntry);
void trial(int param);
/*		end tactics.c		*/

/*	In BADMOVES.c	*/
void clearbadm();
int testbadm(int code, int w1, int w2);
void setbadm(int code, int s1, int s2);
/*		end badmoves.c		*/

/*	In BLOCK.c	*/
void *gtsp(int gr, int size);
void freesp(int gr);
int repspace(int pp);
/*		end block.c		*/

/*	In DOTYPES.c	*/
void dotypes();
/*		end dotypes.c		*/

/*	In SAMPLES.c	*/
void printdatum(int i, int n);
int readvset();
int readsample(char *fname);
int sname2id(char *nam, int expect);
int vname2id(char *nam);
int qssamp(Sample *samp);
int id2ind(int id);
int thinglist(char *tlstname);
/*		end samples.c		*/

/*	In GLOB.c	*/
char *sers(Class *cll);

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
EXT int Ntypes;   /* The number of different attribute types */
EXT VarType *types; /* a vector of Ntypes type definitions,
         created in dotypes */
EXT Context ctx;  /* current context */
EXT VarSet *vsets[MAX_VSETS];
EXT Sample *samples[MAX_SAMPLES];
EXT Population *poplns[MAX_POPULATIONS];
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
EXT int heard;    /* Flag showing a new command line has been detected */
EXT int usestdin; /* Flag showing input from sdtdin */

EXT int trapkk, trapage, trapcnt;

/*	To control what is adjusted  */
EXT int control, dcontrol;

/*	To determine how weights are distributed  */
EXT int dfix, fix;

/*	re Poplns  */
EXT VarSet *vst;
EXT Sample *samp;
EXT Population *population;
EXT VSetVar *avi;
EXT PVinst *pvi;
EXT SampleVar *svi;
EXT VarType *vtp;
EXT int nc; /* Number of cases */
EXT int nv; /* Number of variables */
EXT int root;
EXT Class *rootcl;
EXT VSetVar *vlist;
EXT PVinst *pvars;
EXT SampleVar *svars;

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
EXT int rseed; /*	Seed for random routines */
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
EXT int seeall;         /* A flag to make all cases be treated in all classes */
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