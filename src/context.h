typedef struct SnobContextStruct {
  extern
#else
#define EXT
#endif

      /*	mathematical constants   */
      EXT double HALF_LOG_2PI,
      HALF_LOG_2, LATTICE, PI, BIT, TWOBIT, TWO_ON_PI, HALF_PI;
  double ZeroVec[MAX_ZERO];
  double FacLog[MAX_CLASSES + 1];
  volatile sig_atomic_t Stop;
  int NTypes;
  VarType *Types;
  Context state, bkpState;
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

#ifdef USE_SNOB_CONTEXT_MACROS
#define HALF_LOG_2PI (ctx->HALF_LOG_2PI)
#define HALF_LOG_2 (ctx->HALF_LOG_2)
#define LATTICE (ctx->LATTICE)
#define PI (ctx->PI)
#define BIT (ctx->BIT)
#define TWOBIT (ctx->TWOBIT)
#define TWO_ON_PI (ctx->TWO_ON_PI)
#define HALF_PI (ctx->HALF_PI)
#define ZeroVec (ctx->ZeroVec)
#define FacLog (ctx->FacLog)
#define Stop (ctx->Stop)
#define NTypes (ctx->NTypes)
#define Types (ctx->Types)
#define CurCtx (ctx->CurCtx)
#define BkpCtx (ctx->BkpCtx)
#define VarSets (ctx->VarSets)
#define Samples (ctx->Samples)
#define Populations (ctx->Populations)
#define CurSource (ctx->CurSource)
#define Heard (ctx->Heard)
#define UseStdIn (ctx->UseStdIn)
#define Interactive (ctx->Interactive)
#define Debug (ctx->Debug)
#define Control (ctx->Control)
#define DControl (ctx->DControl)
#define DFix (ctx->DFix)
#define Fix (ctx->Fix)
#define NumRepChars (ctx->NumRepChars)
#define Scores (ctx->Scores)
#define RSeed (ctx->RSeed)
#define NoSubs (ctx->NoSubs)
#define NewSubs (ctx->NewSubs)
#define Sons (ctx->Sons)
#define NextIc (ctx->NextIc)
#define MinAge (ctx->MinAge)
#define MinFacAge (ctx->MinFacAge)
#define MinSubAge (ctx->MinSubAge)
#define MaxSubAge (ctx->MaxSubAge)
#define HoldTime (ctx->HoldTime)
#define Forever (ctx->Forever)
#define MinSize (ctx->MinSize)
#define MinWt (ctx->MinWt)
#define MinSubWt (ctx->MinSubWt)
#define SigScoreChange (ctx->SigScoreChange)
#define SeeAll (ctx->SeeAll)
#define DontIgnore (ctx->DontIgnore)
#define ScoreChanges (ctx->ScoreChanges)
#define NewSubsTime (ctx->NewSubsTime)
#define InitialAdj (ctx->InitialAdj)
#define MaxAdj (ctx->MaxAdj)
#define MinGain (ctx->MinGain)
#define Mbeta (ctx->Mbeta)
#define Bbeta (ctx->Bbeta)
#define RootAge (ctx->RootAge)
#define GiveUp (ctx->GiveUp)
#define BadKey (ctx->BadKey)
#endif
