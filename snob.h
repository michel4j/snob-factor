/*    This file contains type definitions for generic and global structures,
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
#include <omp.h>

#define MAX_SAMPLES 10 /* Max number of samples */
#define MAX_VSETS 3
#define MAX_POPULATIONS 10 /* Max number of popln models */
#define MAX_CLASSES 150
#define MAX_ZERO 100          /*  Length of the Zero vector  */
#define INPUT_BUFFER_SIZE 450 /*    Length of input line buffer */

/*    -----------------  Control  ------------------------------   */
#define AdjSc 1
#define AdjTr 2
#define AdjPr 4
#define AdjSP 5
#define AdjAll 7

/*    The above define mask bits in 'control' to control
adjustment of weights, scores, structure, parameters
respectively  */

#define Tweak 8
/*    A special control bit to cause params to be adjusted in response
to a change in dad's params, without a full recalculation.  */
#define Noprior 16
/*    Special bit to guess no-prior params, spreads. Tweak, Noprior only
work on leaves without sons (i.e. without subs)  */

#define Partial 1
#define Most_likely 2
#define Random 3
/*    These are values for 'dfix' which normally
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

/*    -----------------  Variable types  ---------------------  */

typedef struct VarTypeStruct {
    int id;
    int data_size;
    int attr_aux_size; /* Size of aux block for vartype in vlist */
    int smpl_aux_size; /* size of aux block for vartype in sample */
    int pop_aux_size;  /* size of aux block for vartype in popln */
    char *name;
    int (*read_aux_attr)();  /* Fun to read aux attribute info */
    int (*read_aux_smpl)();  /* Fun to read aux sample info */
    int (*read_datum)();     /* Fun to read a datum */
    void (*print_datum)();   /* Fun to print datum value */
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

/*    -------------------  Files ----------------------------------  */

typedef struct BufferStruct {
    FILE *cfile;
    int line, nch;
    char cname[80];
    char inl[INPUT_BUFFER_SIZE];
} Buffer;

/*    ------------------  Allocation blocks  --------------------  */
typedef struct BlockStruct Block;
struct BlockStruct {
    Block *next;
    int size;
};

/*    -------------------  Attributes  -------------------------   */

typedef struct VSetVarStruct {
    int id;
    int type;
    int inactive;   /* Inactive attribute flag */
    int basic_size; /*  Sizeof basic block (CVinst) for this var */
    int stats_size; /* Sizeof stats block (EVinst) for this var */
    VarType *vtype;
    char *vaux;
    char name[80];
} VSetVar;

typedef struct VarSetStruct {
    int id;
    Block *blocks;     /* Ptr to chain of blocks allocated */
    char filename[80]; /* file name of vset */
    char name[80];
    int length;   /* Number of variables */
    int num_active; /* Number of active variables */
    VSetVar *variables;
} VarSet;

/*    --------------------  Samples  ---------------------------   */

typedef struct SampleVarStruct {
    int id;
    int nval;
    char *saux;
    int offset; /*  offset of (missing, value) in record  */
} SampleVar;

/*    Sample data is packed into a block of 'records' addressed by
the 'recs' pointer in a Sample structure. There is one record per item in the
sample. Each record has the following format:
    char active_flag. If zero, the item is ignored in building classes.
    int ident    The item identifier, as a positive integer.
  Then follow 'nun_vars' fields for the attribute values of the item. Each field
actually has two parts:
    char missing_flag  If non-zero, shows value is unknown .
    Datum value.  The type Datum depends on the type of attribute. This
        field is present even if the missing flag is on, but contains
        garbage.
    */

typedef struct SampleStruct {
    int id;
    Block *blocks;      /* Ptr to chain of blocks allocated for sample */
    char vset_name[80]; /* Name of variable-set */
    int num_cases;      /* Num of cases */
    int num_active;     /* Num of active cases */
    SampleVar *variables;  /* Ptr to vector of SVinsts, one per variable */
    char *records;      /*  vector of records  */
    int record_length;  /*  Length in chars of a data record  */
    double best_cost;   /*  Cost of best model */
    int best_time;      /*  Popln age when bestcost reached  */
    char name[80];
    char filename[80]; /*  Data file name  */
} Sample;

/*    ----------------------   Classes  ---------------------------     */

typedef struct CVinstStruct { /*Structure for basic info on a var in a class*/
    int id;
    int signif;
    int infac; /* shows if affected by factor  */
} CVinst;

typedef struct EVinstStruct {              /* Stuff for var in class in expln */
    double num_values;                     /*  Num of values */
    double btcost, ntcost, stcost, ftcost; /**/
    double bpcost, npcost, spcost, fpcost; /**/
    double sum_scores_sq;                  /* weighted sum of squared scores */
    int id;
} EVinst;

typedef struct ClassStruct {
    double relab;
    double mlogab;                                     /*  - log relab */
    double best_cost;                                  /* Best total class cost  */
    double dad_cost, nofac_cost, fac_cost;             /* Class costs as dad, sansfac, confac */
    double best_par_cost;                              /**/
    double dad_par_cost, nofac_par_cost, fac_par_cost; /* Parameter costs in above */
    double best_case_cost;                             /**/
    double best_fac_cost;                              /*  Used to track best cfcost to detect improvement*/
    double weights_sum;                                /* sum of weights of members  */
    double sum_score_sq;                               /* Sum of squared scores */
    double score_boost;                                /* Used to inflate score vector early on  */
    double avg_factor_scores;                          /* average factor_scores  */
    char type;                                         /* 0 = ?, 1 = root, 2 = dad, 3 = leaf, 4 = sub */
    char hold_type;                                    /**/
    char use;                                          /* Current use: 1=sansfac, 2=confac */
    char hold_use;                                     /**/
    int boost_count;                                   /*  Monitors need to boost vsq  */
    int score_change_count;                            /*  Counts significant score changes  */
    int age;                                           /*  age in massage counts  */
    int dad_id, sib_id, son_id;                        /* id links in class hierarchy */
    int num_sons;                                      /* Number of son classes */
    int serial;
    /*
        Items above this line must be distributed to
        all remotes before each pass through the data.
        The items in the next group are accumulated and must be
        returned to central.  They are cleared to zero by clear_costs.
    */
    double newcnt;                    /*  Accumulates weights for cnt  */
    double newvsq;                    /*  Accumulates squared scores for vsq  */
    double cfvcost;                   /*  Factor-score cost included in cftcost  */
    double cntcost, cstcost, cftcost; /* Thing costs in above */
    double vav;                       /* sum of log vvsprds */
    double factor_score_sum;          /* sum of factor_scores  */
    int scancnt;                      /*  Number of things considered  */
    /*
        Items below here are generated locally by
        docase for each case, and need not be distributed or returned
    */
    int case_score;          /*  Integer score of current case  */
    double total_case_cost;  /*  tcost of current case  */
    double nofac_case_cost;  /* Cost of current case in no-fac class */
    double fac_case_cost;    /* """"""""""""""""""""""" factor class */
    double coding_case_cost; /* Part of casefcost due to coding score */
    double dad_case_cost;    /* """""""""""""""""""""""  dad   class */
    double case_weight;      /*  weight of current case  */
    double case_fac_score, case_fac_score_sq, cvvsprd, clvsprd;
    /*
        Items below this line are set up when class is made by makeclass()
        and should NOT be copied to a new class structure. IT IS ASSUMED THAT
        'ID' IS THE FIRST ITEM BELOW THE LINE.
        Except for id, which never changes and is set by central, the other
        items are pointers which will be set by remotes, will not thereafter
        change, but may have different values in different machines.
    */
    int id;
    short *factor_scores; /* Factor scores */
                          /* Scores times 4096 held as signed shorts in +-30000 */
    CVinst **basics;      /* ptr to vec of ptrs to variable basics */
    EVinst **stats;       /* ptr to vec of ptrs to variable stats blocks*/
} Class;

/*    --------------------------  Population  ----------------------  */

typedef struct PVinstStruct {
    int id;
    char *paux;
} PVinst;

typedef struct PoplnStruct {
    int id;
    Block *blocks, *model_blocks; /* Ptrs to bocks allocated for popln, and for popln as model of sample */
    char vst_name[80];            /* Name of variable-set */
    char sample_name[80];         /*  Name of sample to which popln is attached if any*/
    int sample_size;              /*  Size of sample attached, or 0 */
    int num_cases;                /*  num of active cases in sample used for training */
    Class **classes;              /* ptr to vec of ptrs to classes  */
    PVinst *variables;                /* Ptr to vector of PVinsts, one per variable */
    char filename[80];            /*  Popln file name  */
    char name[80];
    int next_serial; /*  Next serial number for a new class */
    int num_classes; /* Number of classes  */
    int num_leaves;  /* Number of leaves */
    int root;        /* index of root class  */
    int cls_vec_len; /*  Length of 'classes' vec. */
    int hi_class;    /*  Highest allocated entry in pop->classes */
} Population;

/*    -------------------------------------------------------------   */

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

/*    --------- Functions -------------------------------   */

/*
    In listen.c
*/
int hark(char *lline);

/*
    In inputs.c
*/
int open_buffer();
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

/*
    In poplns.c
*/
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

/*
    in classes.c
 */
int serial_to_id(int ss);
int make_class();
void clear_costs(Class *ccl);
void set_best_costs(Class *ccl);
void score_all_vars(Class *ccl, int item);
void cost_all_vars(Class *ccl, int item);
void deriv_all_vars(Class *ccl, int item);
void parent_cost_all_vars(Class *ccl, int valid);
void adjust_class(Class *ccl, int dod);
void delete_sons(int kk);
void print_class(int kk, int full);
void set_class_with_scores(Class *ccl, int item);
int split_leaf(int kk);
void delete_all_classes();
int next_leaf(Population *cpop, int iss);

/*
    In doall.c
*/
int do_all(int ncy, int all);
void find_all(int typ);
int do_dads(int ncy);
int do_good(int ncy, double target);
void tidy(int hit);
int rand_int();
double rand_float();
void do_case(int cse, int all, int derivs);
void sort_sons(int kk);

/*
    In tune.c
*/
void defaulttune();

/*
    In tactics.c
*/
int flatten(const int show);
double insert_dad(int ser1, int ser2, int *dadid);
int best_insert_dad(int force);
int best_remove_dad();
double splice_dad(int ser);
void rebuild();
void ranclass(int nn);
void binary_hierarchy(int flat);
double move_class(int ser1, int ser2);
int best_move_class(int force);
void try_moves(int ntry);
void trial(int param);

/*
    In badmoves.c
*/

void clr_bad_move();
int chk_bad_move(int code, int w1, int w2);
void set_bad_move(int code, int s1, int s2);

/*
    In block.c
*/
void *alloc_blocks(int gr, int size);
void free_blocks(int gr);
int report_space(int pp);

/*
    In dotypes.c
*/
void do_types();

/*
    In samples.c
*/
void print_var_datum(int i, int n);
int read_vset();
int load_vset(const char *fname);
int load_sample(const char *fname);
int find_sample(char *nam, int expect);
int find_vset(char *nam);
int sort_sample(Sample *samp);
int find_sample_index(int id);
int item_list(char *tlstname);

/*
    In glob.c
*/
void cleanup_population();
void show_population();
char *serial_to_str(Class *cll);
void show_smpl_names();
void show_pop_names();
void print_progress(size_t count, size_t max);
Result classify(const int max_cycles, const int do_steps, const int move_steps, const double tol);
void reset();
void initialize(int lib, int debug, int threads);
void show_pop_names();
void show_smpl_names();
void log_msg(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));
