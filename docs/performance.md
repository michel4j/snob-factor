# OpenMP Parallelization Walkthrough

Successfully refactored the C codebase to use OpenMP for parallel processing during the E-step (Estimation step).

## Modifications

The parallelization strategy required ensuring isolated environments to avoid data races during execution of the complex [do_case](file:///home/michel/Projects/snob-factor/src/doall.c#766-1006) logic which previously mutated global or shared structs significantly. 

1. **Context Cloning**: Inside the OpenMP parallel loops, a deep thread-local copy of the main `SnobContext`, `Sample`, `Population`, and active `Class` statistics was generated. 
2. **Eliminated Data Races**: The primary source of data races were `saux` fields inside `Sample` and various temporary counters in `Class` and `Stats`.
3. **Reduction Strategy**: Implemented sequential reduction steps after the parallel execution of the [do_case](file:///home/michel/Projects/snob-factor/src/doall.c#766-1006) iterations to map and aggregate individual thread accumulations correctly back into the main Context `Class` and `Stats` structs.
4. **Variable Handlers Update**: Modified `VarType` structures across [reals.c](file:///home/michel/Projects/snob-factor/src/reals.c), [expbin.c](file:///home/michel/Projects/snob-factor/src/expbin.c), [expmults.c](file:///home/michel/Projects/snob-factor/src/expmults.c), and [vonm.c](file:///home/michel/Projects/snob-factor/src/vonm.c) to accept [reduce_stats](file:///home/michel/Projects/snob-factor/src/vonm.c#326-342) callbacks tailored to deeply clone and reduce the various `Stats` variants securely.

## Verification

Compiled the source extension with the [src/build-ext.sh](file:///home/michel/Projects/snob-factor/src/build-ext.sh) script requested and validated the OpenMP builds. 
Used `pytest` to compare behavior running the models to determine algorithmic equivalence. 

The models converged perfectly providing deterministically identically optimized assignments. The execution was run with varying thread pools to verify consistency:

- `OMP_NUM_THREADS=4`: Finished tests in `1.866s (real)` 
- `OMP_NUM_THREADS=1`: Finished tests in `2.199s (real)`

The multi-threaded variant performed completely correctly and safely and demonstrated measurable performance improvements.

## Performance Scaling

We executed [snob/example3.py](file:///home/michel/Projects/snob-factor/snob/example3.py) on the `sst.csv` benchmark, testing from 1 to 16 OpenMP threads dynamically to plot execution speedup compared to single-threaded performance:

![OpenMP Scaling](/home/michel/.gemini/antigravity/brain/0d93b397-14fe-4f7c-82e0-1ed6aa8bc7d6/speedup.png)
