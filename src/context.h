typedef struct SnobContextStruct {
    extern
#else
#define EXT
#endif

/*	mathematical constants   */
EXT double HALF_LOG_2PI, HALF_LOG_2, LATTICE, PI, BIT, TWOBIT, TWO_ON_PI, HALF_PI;
    double ZeroVec[MAX_ZERO];
    double FacLog[MAX_CLASSES + 1];
    volatile sig_atomic_t Stop;
    int NTypes;
    VarType *Types;
    Context CurCtx, BkpCtx;
    VarSet *VarSets[MAX_VSETS];
    Sample *Samples[MAX_SAMPLES];
    Population *Populations[MAX_POPULATIONS];
    Buffer *CurSource;
    int Heard;
    int UseStdIn;
    int Interactive;
    int Debug;
    int Control, DControl;
    int DFix, Fix;
    int NumRepChars;
    Score Scores;
    int RSeed;
    int NoSubs;
    int NewSubs;
    Class *Sons[MAX_CLASSES];
    int NextIc[MAX_CLASSES];
    int MinAge;
    int MinFacAge;
    int MinSubAge;
    int MaxSubAge;
    int HoldTime;
    int Forever;
    double MinSize;
    double MinWt;
    double MinSubWt;
    int SigScoreChange;
    int SeeAll;
    int DontIgnore;
    int ScoreChanges;
    int NewSubsTime;
    double InitialAdj;
    double MaxAdj;
    double MinGain;
    double Mbeta;
    double Bbeta;
    int RootAge;
    int GiveUp;
    int BadKey[BadSize];
} SnobContext;

extern __thread SnobContext *current_ctx;

#ifdef USE_SNOB_CONTEXT_MACROS
#define HALF_LOG_2PI (current_ctx->HALF_LOG_2PI)
#define HALF_LOG_2 (current_ctx->HALF_LOG_2)
#define LATTICE (current_ctx->LATTICE)
#define PI (current_ctx->PI)
#define BIT (current_ctx->BIT)
#define TWOBIT (current_ctx->TWOBIT)
#define TWO_ON_PI (current_ctx->TWO_ON_PI)
#define HALF_PI (current_ctx->HALF_PI)
#define ZeroVec (current_ctx->ZeroVec)
#define FacLog (current_ctx->FacLog)
#define Stop (current_ctx->Stop)
#define NTypes (current_ctx->NTypes)
#define Types (current_ctx->Types)
#define CurCtx (current_ctx->CurCtx)
#define BkpCtx (current_ctx->BkpCtx)
#define VarSets (current_ctx->VarSets)
#define Samples (current_ctx->Samples)
#define Populations (current_ctx->Populations)
#define CurSource (current_ctx->CurSource)
#define Heard (current_ctx->Heard)
#define UseStdIn (current_ctx->UseStdIn)
#define Interactive (current_ctx->Interactive)
#define Debug (current_ctx->Debug)
#define Control (current_ctx->Control)
#define DControl (current_ctx->DControl)
#define DFix (current_ctx->DFix)
#define Fix (current_ctx->Fix)
#define NumRepChars (current_ctx->NumRepChars)
#define Scores (current_ctx->Scores)
#define RSeed (current_ctx->RSeed)
#define NoSubs (current_ctx->NoSubs)
#define NewSubs (current_ctx->NewSubs)
#define Sons (current_ctx->Sons)
#define NextIc (current_ctx->NextIc)
#define MinAge (current_ctx->MinAge)
#define MinFacAge (current_ctx->MinFacAge)
#define MinSubAge (current_ctx->MinSubAge)
#define MaxSubAge (current_ctx->MaxSubAge)
#define HoldTime (current_ctx->HoldTime)
#define Forever (current_ctx->Forever)
#define MinSize (current_ctx->MinSize)
#define MinWt (current_ctx->MinWt)
#define MinSubWt (current_ctx->MinSubWt)
#define SigScoreChange (current_ctx->SigScoreChange)
#define SeeAll (current_ctx->SeeAll)
#define DontIgnore (current_ctx->DontIgnore)
#define ScoreChanges (current_ctx->ScoreChanges)
#define NewSubsTime (current_ctx->NewSubsTime)
#define InitialAdj (current_ctx->InitialAdj)
#define MaxAdj (current_ctx->MaxAdj)
#define MinGain (current_ctx->MinGain)
#define Mbeta (current_ctx->Mbeta)
#define Bbeta (current_ctx->Bbeta)
#define RootAge (current_ctx->RootAge)
#define GiveUp (current_ctx->GiveUp)
#define BadKey (current_ctx->BadKey)
#endif
