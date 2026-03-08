# OpenMP Parallelization

Successfully refactored the C codebase to use OpenMP for parallel processing during the E-step (Estimation step).

## Modifications

The parallelization strategy required ensuring isolated environments to avoid data races during execution of the complex [do_case](file:///home/michel/Projects/snob-factor/src/doall.c#766-1006) logic which previously mutated global or shared structs significantly. 

1. **Context Cloning**: Inside the OpenMP parallel loops, a deep thread-local copy of the main `SnobContext`, `Sample`, `Population`, and active `Class` statistics was generated. 
2. **Eliminated Data Races**: The primary source of data races were `saux` fields inside `Sample` and various temporary counters in `Class` and `Stats`.
3. **Reduction Strategy**: Implemented sequential reduction steps after the parallel execution of the [do_case](file:///home/michel/Projects/snob-factor/src/doall.c#766-1006) iterations to map and aggregate individual thread accumulations correctly back into the main Context `Class` and `Stats` structs.
4. **Variable Handlers Update**: Modified `VarType` structures across [reals.c](file:///home/michel/Projects/snob-factor/src/reals.c), [expbin.c](file:///home/michel/Projects/snob-factor/src/expbin.c), [expmults.c](file:///home/michel/Projects/snob-factor/src/expmults.c), and [vonm.c](file:///home/michel/Projects/snob-factor/src/vonm.c) to accept [reduce_stats](file:///home/michel/Projects/snob-factor/src/vonm.c#326-342) callbacks tailored to deeply clone and reduce the various `Stats` variants securely.

## Verification

Compiled the source extension and validated the OpenMP builds by comparing the output of the single and multi-threaded runs with original output from the non-parallelized `auto-snob` executable. The results matched identically. The execution was run with varying thread pools to verify consistency: The multi-threaded variant performed completely correctly and safely and demonstrated measurable performance improvements.

## Performance Scaling

We executed [snob/example3.py](file:///home/michel/Projects/snob-factor/snob/example3.py) on the `sst.csv` benchmark, testing from 1 to 16 OpenMP threads dynamically to plot execution speedup compared to single-threaded performance. The performance speedups were close to the limit of the hardware acheiving almost 80% of the theoretical maximum.

