# The auto-snob Executable

The `snob-factor` project includes a standalone binary that can automatically perform mixture modelling directly from the command line without dropping into the interactive shell.

The internal C executable is called `auto-snob`.

## Running `auto-snob`

```bash
$ auto-snob -h
Usage: ./build/auto-snob [OPTIONS] <vset.v> <smpl.s> [report.rep]
Options:
  -h          Print this help message
  -v          Verbose Logging (default: off)
  -c CYCLES   Set number of fit cycles (default: 25)
  -s STEPS    Set number of EM steps per cycle (default: 50)
  -m MOVES    Set number of try moves per cycle (default: 4)
```

The `auto-snob` executable expects two target files sequentially:
1. The `.v` (vset list of variables) file
2. The `.s` (sample variables) file

### Example usage

To run the executable against the `vm` problem set inside `examples/`, you would run the following bash command:

```bash
auto-snob examples/vm.v examples/vm.s
```

This will automatically execute the top-down classification algorithm with built-in parameter adjustments, 
and output the log of population structures and model costs identically to standard output. 

You can capture this output in a `.log` file by redirecting standard output (using the Unix `>` or `tee` command):

```bash
auto-snob examples/vm.v examples/vm.s | tee testing_results.log
```

The resulting logs provide granular details on class parameters (like variance and factor loadings) as well as MML 
costs after completing all evaluation cycles.

### Understanding the Output
Some of the output is similar to the original Factor Snob program. Here is a breakdown of the output:

#### Tree

Here is some sample output and explanation.

```
  Popln  1 on sample  1,   5 classes,   800 things  Cost   25077.83
    Assign mode Partial    --- Adjust: Params Scores Tree
      1  Dad       Scan    2400 Size   800.0
         2 Leaf   Fac Scan    2257 Size   394.5
         3  Dad       Scan    2033 Size   405.5
            4 Leaf   Tny Scan    1811 Size   200.3
            5 Leaf       Scan    1822 Size   205.2
  P1     5 classes,    3 leaves,  Pcost   279.3  Tcost   24798.6,  Cost   25077.8
  ```

- "5 classes, 3 leaves" (top and bottom lines) --- All of the data items are in the leaves, but when two leaves are related, there may be a "Dad" which joins them. Factor Snob seems to count both leaves and dads as "classes".
- "Fac" (node 2) --- means there is a factor model at this node. Factor models account for correlations between variables, and can make for much better models of the Dad class --- so much better that you may no longer need subclasses, resulting in a great savings in model cost. For example, using a factor model, we went from 27 classes and 7 leaves down to 7 classes and two leaves.
- "Scan" (nodes 1-5) --- factor models are time-intensive, so Snob occasionally ignores very low-probability "things" when counting membership. That can save a great deal of time, but it means the computed costs won't be quite right. Every now and then Snob peeks just to be sure things haven't moved. "Scan" means it has used this speedup. (Note, it uses the speedup on all models. It just saves the most time on factor models.)

#### Class Details
When class details are printed during the run it will look like the following.  It may say "Use Fac vv 0.99" which means it used a factor model for that class, with strength of 0.99 (near max). Then we see a comparison of the parameter costs for various kinds of models: Leaf (L), Factor (F), Dad (D), and Best (B). Here is an example:

```
  S    2  LEAF Dad    1  Age 260  Sz 394.5  Use Fac Vv  0.70  +-0.736
  Pcosts  S:    66.14  F:   110.74  D:     0.00  B:   110.74
  Tcosts  S: 13206.29  F: 12722.63  D:     0.00  B: 12953.86
  Vcost     ---------  F:   148.94
  totals  S: 13272.44  F: 13064.59  D:     0.00  B: 13064.59
  P1     5 classes,    3 leaves,  Pcost   279.3  Tcost   24798.6,  Cost  25077.8
  ```

- First read the "Pcosts" line. That shows the parameter costs for the serial model (66.14) and the factor model (110.74).
- Then the "Tcosts" line shows, I believe, the data costs. In the original version the "Best" column sometimes did not match any of the others, that bug has been fixed.
- Later on, we see a few lines per variable (character) in the class, headed according to what kind of model was used. For multi-state (multinomial) variables, we see:

> PR --- relative frequency of states (overall)  
> FR --- relative frequency of states if we use the factor (for low-weight factors, this will roughly equal PR)  
> BP --- a measure of the influence of the factor on the states, so we can see the strength of the factor  

For continuous variables, we see:

> S --- the simple model mu and sigma (mean and standard deviation)  
> F --- the factor model mu and sigma, where 'Ld' shows the 'loading', which is how much the factor score affects these values  

