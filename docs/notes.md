
## Chris Wallace's 'Factor Hierarchical Snob'

Factor SNOB, notes on Chris Wallace's hierarchical, factor-analysis unsupervised classification program - commands and hints         

Please cite:

- C. S. Wallace, [_Statistical and Inductive Inference by Minimum Message Length_](../../2005book.html), Springer, 2005,
- C. S. Wallace and P. R. Freeman, [_Single Factor Estimation by MML_](../../Structured/1992-WF-JRSS.html), J. Royal Stat. Soc. B, Vol.54, No.1, pp.195-209, 1992.


### Data

The following works. (It is _possible_ that factor Snob _may_ be more flexible about input format than suggested here.)

`datafile.v` describes the "type" of the data, and `datafile.s` gives the precision and the data values.

`datafile.v`:
```
Variables_for_Hybrids
6
 
height  1
sex     2       2
colour  2       6
yield   1
orientation     4
dry_weight      1
```

description of variables (columns), \# of columns, blank line, then one line per column, 1 ~ continuous, 2 ~ discrete plus arity, 3 ~ Poisson(?), 4 ~ von Mises(?).

`datafile.s`:
```
Name_of_the_dataset
Variables_for_Hybrids
0.3
 
 
.01
1  10
2.0
 
5
101 37.1 1 3   18.2  73  13
115     22      2       6       ==      -50     14
103     =       =       1       30.5    100     30
-555    16.2    1       2       12      -30     8.4
501     40      2       4       12.2    200     ===
```

Name of the data set, description of variables (as in `datafile.v`), then one line per column: precision (of a real variable), blank for a discrete variable, blank line, then number of data (rows), data (NB. 1st col is datum number, -ve ~ don't use). NB. Discrete values are coded from 1 up. (=+ for missing values.)

#### Hints and Tactics:

repeat a few times:
```
doall 50
trymoves 2
```

In interactive mode, experiment to find a sequence of commands that does what you want, then put that sequence into a command file. Then run factor snob with std-input redirected (<) from the command file and std-output redirected (>) to an output file. (It looks like csw had not gotten around to making file versions of all of the reporting commands. Also see 'file' command.) It works for me, e.g.,

cmdfile:
```
datafile.v
datafile.s

doall 50
trymoves 2

doall 50
trymoves 2

doall 50
tree

prclass -2 1
trep thingReportFile

stop
```

then run:

```bash
snob-factor < cmdfile > outputfile
```

### Commands

```
adjust         deldad    insdad     prclass     splitleaf      
assign         doall     killclass  ranclass    sto            
bestdeldad     dodads    killpop    readsamp    stop           
bestinsdad     dogood    killsons   rebuild     thing          
bestmoveclass  doleaves  moveclass  restorepop  tree           
binhier        file      nosubs     samps       trep           
copypop        flatten   pickpop    savepop     trymoves       
crosstab       help      pops       select         
```

**`adjust`**

controls which aspects of a population model will be changed. Follow it with one or more characters from:  
'a'll, 's'cores, 't'ree, 'p'arams, each followed by '+' or '-' to turn adjustment on or off. Thus "adjust t-p+" disables tree structure adjustment and enables class parameter adjustment. 'p' does not include scores.

**`assign <c>`**

controls how things are assigned to classes. The character c should be one of 'p'artial, 'm'ost\_likely or 'r'andom; default is 'p'.  
Partial is usually what you want. I'm _guessing_ that random is in proportion to posterior prob. of membership.

**`bestdeldad`**

guesses the most profitable dad to delete and tries it.

**`bestinsdad`**

makes a guess at the most profitable insdad and tries it.

**`bestmoveclass`**

guesses the most profitable moveclass and tries it.

**`binhier <N>`**

inserts dads to convert tree to a binary hierarchy, then deletes dads to improve. If N>0, first flattens tree.

**`copypop <N, P>`**

copies the "work" model to a new model. N selects the level of detail:  
N=0 copies no thing weights or scores, leaving the new popln unattached to a sample.  
P is the new popln name.

**`crosstab <P>`**

Shows the overlap between the leaves of work and the leaves of model P (which must be attached to the current sample.) The overlap is shown as a permillage of the number of active things, in a cross-tabulation table. A table entry for leaf serial Sw of work and leaf serial Sp of P shows the permillage of all active things which are in both leaves. Crosstab <work> is also meaningful. Here, an entry for leaves S1, S2 shows the permillage of thing which are partially assigned to both classes.

**`deldad <S>`**

Replaces dad class S by its sons

**`doall <N>`**

Does `N` steps of top-down re-estimation and assignment.  
Seems to produce only binary trees; seem to need 'trymoves' to change tree structure.

**`dodads <N>`**

Iterates adjustment of parameter costs and dad parameters till stability or for N cycles

**`dogood <N>`**

Does N cycles of doleaves(3) and dodads(2) or until stable.

**`doleaves <N>`**

Does N steps of re-estimation and assignment to leaf classes subclasses and tree structure usually unaffected. Bottom-up reassignment of weights to dad classes. Doleaves gives faster refinement than Doall, but no tree adjustment. Always followed by one Doall step.

**`file <F>`**

Switches command input to file name F. Command input will return to keyboard at end of F or error, unless F contains a "file" command. F may contain a file command with its own name, returning to the start of F.

**`flatten`**

Makes all twigs (non-root classes with no sons) immediate leaves

**`insdad <S1, S2>`**

Where classes `S1`, `S2` are siblings inserts a new Dad with `S1`, `S2` as childen. The new Dad is son of original Dad of `S1`, `S2`.

**`killclass <S>`**

Kills class serial `S`. Descendants also die.

**`killpop <P>`**

Destroys a population model. `P` must be the popln index or FULL name

**`killsons <S>`**

Kills all sons, grandsons etc. of class serial `S`. `S` becomes leaf

**`moveclass <S1 S2>`**

Moves class `S1` (and any dependent subtree) to be a son of class `S2`, which must be a dad. `S1` may not be an ancestor of `S2`.

**`nosubs <N>`**

Kills and prevents birth of subclasses if `N`>0


**`pickpop <P>`**

Copies population `P` to "work". `P` = population index or FULL name

**`pops`**

Lists the defined models.

**`prclass <S, N>`**

Prints properties of class `S`. If `N`>0, prints parameters If `S`=-1, prints all dads and leaves. If `S`=-2, includes subs.  
'`prclass` -2 1' works (?there may be a bug in `prclass` -1?)

**`ranclass <N>`**

Destroys the current model and inserts `N` random leaves

**`readsamp <F>`**

Reads in a new sample from file `F`. Sample must use a Vset already loaded.

**`rebuild`**

Flattens the tree, then greedily rebuilds it.

**`restorepop <F>`**

Reads a model from file named `F`, as saved by '`savepop`'. If model unattached, or attached to an unknown sample, it is attached to the current sample if any.

**`samps`**

Lists the known samples.

**`savepop <P, N, F>`**

Records model `P` on file named `F`, unattached if `N`=0

**`select <S>`**

First copies the current work model to an unattached model called OldWork, replaces the current sample by sample `<S>` where `S` is either name or index, then picks OldWork to model it, getting thing weights and scores. The 'adjust' state is left as adjusting only scores, not params or tree.

**`splitleaf <S>`**,

If class `S` is a leaf, makes `S` a dad and its subclasses leaves

**`stop`**

Stops both sprompt and snob

**`thing <N>`**

Finds the sample thing with identifier `N`.

**`tree`**

Prints a summary of control settings and the hierarchic tree

**`trep <F>`**

Writes a thing report on file `F`. Assigns each thing to smallest class in which thing has weight >0.5, and to most likely leaf.

**`trymoves <N>`**

Attempts moveclasses until N successive failures; converts a binary-tree into a better structure.
