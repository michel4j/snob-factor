/*	This file contains type definitions for generic and global structures,
and declarations of cetain global variables. The file is intended to be
included in many other files, as well as being compiled and loaded itself.
    For variables, arrays etc defined herein, the instantiation is
suppressed when included in other files by #define NOTGLOB 1 in those other
files. The declarations herein then become converted to "EXT" declarations.
    */

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct SnobContextStruct SnobContext;

#define MAX_SAMPLES 5 // Max number of samples
#define MAX_VSETS 3
#define MAX_POPULATIONS 5 // Max number of popln models
#define MAX_CLASSES 999
#define BUFFER_SIZE 64000
#define MAX_ZERO 100 //  Length of the Zero vector

// Mathematical Constants
#define SNOB_PI 3.14159265358979323846       // 4.0 * atan(1.0)
#define SNOB_HALF_PI 1.5707963267948966      // 0.5 * SNOB_PI
#define SNOB_TWO_ON_PI 0.6366197723675814    // 2.0 / SNOB_PI
#define SNOB_HALF_LOG_2PI 0.9189385332046727 // 0.5 * log(2.0 * SNOB_PI)
#define SNOB_HALF_LOG_2 0.34657359027997264  // 0.5 * log(2.0)
#define SNOB_LATTICE -1.2424533248940002     // -0.5 * log(12.0)
#define SNOB_BIT 0.6931471805599453          // log(2.0)
#define SNOB_TWOBIT 1.3862943611198906       // 2.0 * SNOB_BIT
#define INPUT_BUFFER_SIZE 450                // Length of input line buffer

// ctx->control
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
#define ScoreRScale ((double)(1.0 / ScoreScale))

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

/**
 */

typedef struct ClassVarStruct { // Structure for basic info on a var in a class
    int id;
    int signif;
    int infac; // shows if affected by factor
} ClassVar;

typedef struct ExplnVarStruct { // Stuff for var in class in expln
    double num_values;          //  Num of values
    double btcost, ntcost, stcost, ftcost;
    double bpcost, npcost, spcost, fpcost;
    double sum_scores_sq; // weighted sum of squared scores
    int id;
} ExplnVar;

typedef struct ClassStruct {
    double relab;
    double mlogab;                         //  - log relab
    double best_cost;                      // Best total class cost
    double dad_cost, nofac_cost, fac_cost; // Class costs as dad, sansfac, confac
    double best_par_cost;
    double dad_par_cost, nofac_par_cost, fac_par_cost; // Parameter costs in above
    double best_case_cost;
    double best_fac_cost;     //  Used to track best cfcost to detect improvement
    double weights_sum;       // sum of weights of members
    double sum_score_sq;      // Sum of squared scores
    double score_boost;       // Used to inflate score vector early on
    double avg_factor_scores; // average vv
    char type;                // 0 = ?, 1 = root, 2 = dad, 3 = leaf, 4 = sub
    char hold_type;
    char use; // Current use: 1=sansfac, 2=confac
    char hold_use;
    int boost_count;            //  Monitors need to boost vsq
    int score_change_count;     //  Counts significant score changes
    int age;                    //  age in massage counts
    int dad_id, sib_id, son_id; // id links in class hierarchy
    int num_sons;               // Number of son classes
    int serial;
    /*	******************* Items above this line must be distributed to
                all remotes before each pass through the data.
            The items in the next group are accumulated and must be
            returned to central.  They are cleared to zero by cleartcosts.
            */
    double newcnt;                    //  Accumulates weights for cnt
    double newvsq;                    //  Accumulates squared scores for vsq
    double cfvcost;                   //  Factor-score cost included in cftcost
    double cntcost, cstcost, cftcost; // Thing costs in above
    double vav;                       // sum of log vvsprds
    double totvv;                     // sum of vvs
    int scancnt;                      //  Number of things considered
    /*	********************  Items below here are generated locally by
            docase for each case, and need not be distributed or
            returned  */
    int case_score;          //  Integer score of current case
    double total_case_cost;  //  tcost of current case
    double nofac_case_cost;  // Cost of current case in no-fac class
    double fac_case_cost;    // """"""""""""""""""""""" factor class
    double coding_case_cost; // Part of casefcost due to coding score
    double dad_case_cost;    // """""""""""""""""""""""  dad   class
    double case_weight;      //  weight of current case
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
    short *factor_scores; // Factor scores
    // ctx->scores times 4096 held as signed shorts in +-30000
    ClassVar **basics; // ptr to vec of ptrs to variable basics
    ExplnVar **stats;  // ptr to vec of ptrs to variable stats blocks
} Class;

// Variable types

typedef struct MemBufferStruct {
    char *buffer;
    int size;
    int offset;
} MemBuffer;

typedef struct PSauxst {
    int missing;
    double dummy;
    double xn;
} PSaux;

typedef struct VarTypeStruct {
    int id;
    int data_size;
    int attr_aux_size; // Size of aux block for vartype in vlist
    int smpl_aux_size; // size of aux block for vartype in sample
    int pop_aux_size;  // size of aux block for vartype in popln
    char *name;
    int (*read_aux_attr)(SnobContext *ctx, void *vax);           // Func to read aux attribute info
    int (*read_aux_smpl)(SnobContext *ctx, void *sax);           // Func to read aux sample info
    int (*read_datum)(SnobContext *ctx, char *loc, int iv);      // Func to read a datum
    void (*print_datum)(SnobContext *ctx, char *loc);            // Func to print datum value
    void (*set_sizes)(SnobContext *ctx, int iv);                 // Func to set basicsize, statssize
    void (*set_best_pars)(SnobContext *ctx, int iv, Class *cls); // Func to set current best use params
    void (*clear_stats)(SnobContext *ctx, int iv, Class *cls);
    void (*reduce_stats)(SnobContext *ctx, int iv, Class *dest, Class *src);
    void (*score_var)(SnobContext *ctx, int iv, Class *cls);
    void (*deriv_var)(SnobContext *ctx, int iv, int fac, Class *cls);
    void (*cost_var)(SnobContext *ctx, int iv, int fac, Class *cls);
    void (*cost_var_nonleaf)(SnobContext *ctx, int iv, int vald, Class *cls);
    void (*adjust)(SnobContext *ctx, int iv, int fac, Class *cls);
    void (*show)(SnobContext *ctx, Class *cls, int iv);
    void (*set_var)(SnobContext *ctx, int iv, Class *cls);
    void (*details)(SnobContext *ctx, Class *cls, int iv, MemBuffer *buffer);
    int (*set_aux_attr)(SnobContext *ctx, void *vax, int aux);               // Func to add attribute aux info directly
    int (*set_aux_smpl)(SnobContext *ctx, void *sax, int unit, double prec); // Func to add sample aux info directly
    int (*set_datum)(SnobContext *ctx, char *loc, int iv, void *value);      // Func to add a datum
} VarType;

/**
 */

typedef struct BufferStruct {
    FILE *cfile;
    int line, nch;
    char cname[80];
    char inl[INPUT_BUFFER_SIZE];
} Buffer;

// Allocation blocks
typedef struct BlockStruct Block;
struct BlockStruct {
    Block *next;
    int size;
};

/**
 */

typedef struct VSetVarStruct {
    int id;
    int type;
    int inactive;   // Inactive attribute flag
    int basic_size; //  Sizeof basic block (ClassVar) for this var
    int stats_size; // Sizeof stats block (ExplnVar) for this var
    VarType *vtype;
    char *vaux;
    char name[80];
} VSetVar;

typedef struct VSetStruct {
    int id;
    Block *blocks;     // Ptr to chain of blocks allocated
    char filename[80]; // file name of vset
    char name[80];
    int length;     // Number of variables
    int num_active; // Number of active variables
    VSetVar *variables;
} VarSet;

// ctx->samples

typedef struct SampleVarStruct {
    int id;
    int nval;
    char *saux;
    int offset; //  offset of (missing, value) in record
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
    Block *blocks;        // Ptr to chain of blocks allocated for sample
    char vset_name[80];   // Name of variable-set
    int num_cases;        // Num of cases
    int num_active;       // Num of active cases
    int num_added;        /* Num of cases added, should match num_cases after loading is
                             complete */
    SampleVar *variables; // Ptr to vector of SVinsts, one per variable
    char *records;        //  vector of records
    int record_length;    //  Length in chars of a data record
    double best_cost;     //  Cost of best model
    int best_time;        //  Popln age when bestcost reached
    char name[80];
    char filename[80]; //  Data file name
} Sample;

/**
 */

typedef struct PopVarStruct {
    int id;
    char *paux;
} PopVar;

typedef struct PoplnStruct {
    int id;
    Block *blocks, *model_blocks; /* Ptrs to bocks allocated for popln,
      and for popln as model of sample */
    char vst_name[80];            // Name of variable-set
    char sample_name[80];         //  Name of sample to which popln is attached if any
    int sample_size;              //  Size of sample attached, or 0
    int num_cases;                //  num of active cases in sample used for training
    Class **classes;              // ptr to vec of ptrs to classes
    PopVar *variables;            // Ptr to vector of PVinsts, one per variable
    char filename[80];            //  Popln file name
    char name[80];
    int next_serial; //  Next serial number for a new class
    int num_classes; // Number of classes
    int num_leaves;  // Number of leaves
    int root;        // index of root class
    int cls_vec_len; //  Length of 'classes' vec.
    int hi_class;    //  Highest allocated entry in pop->classes
} Population;

//

typedef struct StateStruct {
    VarSet *vset;
    Sample *sample;
    Population *popln;
    Buffer *buffer;
} State;

// Classification Result
typedef struct ResultStruct {
    int num_classes;       // Number of classes found, includes Dads, Leaves and Subs
    int num_leaves;        // Number of leaves, these are the relevant categories
    int num_attrs;         // Number of variables in vset;
    int num_cases;         // Number of cases;
    double model_length;   // Cost of Transmitting Model
    double data_length;    // Cost of Transmitting Data
    double message_length; // Total Cost
} Result;

// Structur for calculating factor scores
typedef struct ScoreStruct {
    double CaseFacScore, CaseFacScoreSq, cvvsprd;
    int CaseFacInt; //  integer form of case_fac_score*4096
    double CaseCost, CaseNoFacCost, CaseFacCost;
    double CaseFacScoreD1, CaseFacScoreD2; // derivs of case cost wrt score
    double EstFacScoreD2;                  /* An over-estimate of CaseFacScoreD2 used in score
                                              ajust */
    double CaseFacScoreD3;
} Score;

/**
 * @param ctx Pointer to the Snob context.
 * @param lline
 */

//	In LISTEN.c
int hark(SnobContext *ctx, char *lline);
//		end listen.c

//	In inputs.c
#ifndef INPUTS
extern int Terminator;
extern Buffer CFileBuffer, CommsBuffer;
#endif

int open_buffser(SnobContext *ctx);
int new_line(SnobContext *ctx);
int read_int(SnobContext *ctx, int *x, int cnl);
int read_double(SnobContext *ctx, double *x, int cnl);
int read_str(SnobContext *ctx, char *str, int cnl);
int read_char(SnobContext *ctx, int cnl);
void swallow(SnobContext *ctx);
void close_buffer(SnobContext *ctx);
void revert(SnobContext *ctx, int flag);
void rep(SnobContext *ctx, int ch);
void flp(SnobContext *ctx);
//	end inputs.c

//	In POPLNS.c
void next_class(SnobContext *ctx, Class **ptr);
int make_population(SnobContext *ctx, int fill);
int init_population(SnobContext *ctx);
void make_subclasses(SnobContext *ctx, int kk);
void destroy_population(SnobContext *ctx, int px);
int copy_population(SnobContext *ctx, int p1, int fill, char *newname);
int save_population(SnobContext *ctx, int p1, int fill, char *newname);
int load_population(SnobContext *ctx, char *nam);
int set_work_population(SnobContext *ctx, int pp);
void print_tree(SnobContext *ctx);
int get_best_pop(SnobContext *ctx);
void track_best(SnobContext *ctx, int verify);
int find_population(SnobContext *ctx, char *nam);
void correlpops(SnobContext *ctx, int xid);
//		end poplns.c

//	In CLASSES.c
int serial_to_id(SnobContext *ctx, int ss);
int make_class(SnobContext *ctx);
void clear_costs(SnobContext *ctx, Class *cls);
void set_best_costs(SnobContext *ctx, Class *cls);
void score_all_vars(SnobContext *ctx, Class *cls, int item);
void cost_all_vars(SnobContext *ctx, Class *cls, int item);
void deriv_all_vars(SnobContext *ctx, Class *cls, int item);
void parent_cost_all_vars(SnobContext *ctx, Class *cls, int valid);
void adjust_class(SnobContext *ctx, Class *cls, int dod);
void delete_sons(SnobContext *ctx, int kk);
void print_class(SnobContext *ctx, int kk, int full);
void set_class_score(SnobContext *ctx, Class *cls, int item);
int split_leaf(SnobContext *ctx, int kk);
void delete_all_classes(SnobContext *ctx);
int next_leaf(SnobContext *ctx, Population *cpop, int iss);
//		end classes.c

//	In DOALL.c
int do_all(SnobContext *ctx, int ncy, int all);
int find_all(SnobContext *ctx, int typ);
int do_dads(SnobContext *ctx, int ncy);
int do_good(SnobContext *ctx, int ncy, double target);
void tidy(SnobContext *ctx, int hit, int no_subs);
int rand_uint(SnobContext *ctx);
int rand_int(SnobContext *ctx);
double rand_float(SnobContext *ctx);
void do_case(SnobContext *ctx, int cse, int all, int derivs, int num_son);
//		end doall.c

//	In TUNE.c
void default_tune(SnobContext *ctx);
//		end tune.c

//	In TACTICS.c
void flatten(SnobContext *ctx);
double insert_dad(SnobContext *ctx, int ser1, int ser2, int *dadid);
int best_insert_dad(SnobContext *ctx, int force);
double splice_dad(SnobContext *ctx, int ser);
int best_remove_dad(SnobContext *ctx);
void rebuild(SnobContext *ctx);
void ranclass(SnobContext *ctx, int nn);
void binary_hierarchy(SnobContext *ctx, int flat);
double move_class(SnobContext *ctx, int ser1, int ser2);
int best_move_class(SnobContext *ctx, int force);
void try_moves(SnobContext *ctx, int ntry);
void trial(SnobContext *ctx, int param);
//		end tactics.c

//	In BADMOVES.c
void clr_bad_move(SnobContext *ctx);
int chk_bad_move(SnobContext *ctx, int code, int cls_a, int cls_b);
void set_bad_move(SnobContext *ctx, int code, int cls_a, int cls_b);
//		end badmoves.c

//	In BLOCK.c
void *alloc_blocks(SnobContext *ctx, int chain, int size);
void free_blocks(SnobContext *ctx, int chain);
//		end block.c

//	In DOTYPES.c
void do_types(SnobContext *ctx);
//		end dotypes.c

//	In SAMPLES.c
void print_var_datum(SnobContext *ctx, int i, int n);
int read_vset(SnobContext *ctx);
int load_vset(SnobContext *ctx, const char *fname);
int load_sample(SnobContext *ctx, const char *fname);
int find_sample(SnobContext *ctx, char *nam, int expect);
int find_vset(SnobContext *ctx, char *nam);
int sort_sample(SnobContext *ctx, Sample *samp);
int find_sample_index(SnobContext *ctx, int id);
int item_list(SnobContext *ctx, char *tlstname);
void destroy_sample(SnobContext *ctx, int sx);
void destroy_vset(SnobContext *ctx, int vx);

int create_vset(SnobContext *ctx, const char *name, int num_vars);
int add_attribute(SnobContext *ctx, int index, const char *name, int itype, int aux);
int create_sample(SnobContext *ctx, char *name, int size, int *units, double *precision);
int add_record(SnobContext *ctx, int index, char *bytes);

//		end samples.c

//	In glob.c
char *serial_to_str(SnobContext *ctx, Class *cls);
void log_msg(SnobContext *ctx, int level, const char *format, ...) __attribute__((format(printf, 3, 4)));
int error_value(SnobContext *ctx, const char *message, const int value);
void print_progress(SnobContext *ctx, size_t count, size_t max);
void save_context(SnobContext *ctx);
void restore_context(SnobContext *ctx);
void destroy_context(SnobContext *ctx);
void set_control_flags(SnobContext *ctx, int flags);

void cleanup_population(SnobContext *ctx);
void show_population(SnobContext *ctx);
void show_smpl_names(SnobContext *ctx);
void select_sample(SnobContext *ctx, char *name);
void select_population(SnobContext *ctx, char *name);
void show_pop_names(SnobContext *ctx);
SnobContext *initialize(int interact, int debug, int seed);
void get_class_details(SnobContext *ctx, char *buffer, size_t buffer_size);
void print_buffer(SnobContext *ctx, MemBuffer *buffer, const char *format, ...) __attribute__((format(printf, 3, 4)));
Result classify(SnobContext *ctx, const int max_cycles, const int do_steps, const int move_steps, const double tol);
int save_model(SnobContext *ctx, char *filename);
int load_model(SnobContext *ctx, char *filename);
void peek_data(SnobContext *ctx);
int get_assignments(SnobContext *ctx, int *ids, int *prim_cls, double *prim_probs, int *sec_cls, double *sec_probs);
int sort_current_sample(SnobContext *ctx);
