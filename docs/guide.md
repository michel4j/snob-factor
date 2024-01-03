Factor Snob
-----------
The documentation below has been pieced together from the original README file as well as notes and guides originally 
authored by Lloyd Allison. The original notes and guides can be read at http://www.allisons.org/ll/MML/Notes/SNOB-factor/. 
Information from Vanilla Snob, the previous version of Snob is also included in the docs folder as it provides additional 
information and details relevant for understanding Factor Snob.

Lloyd Allison: Chris Wallace had been refining "Factor Snob" for some years
before his death in 2004, when he found time from his other
projects, including his book [1] (2005). Some of us at his department had run it on several problems,
but Chris did not want to make it publicly available before
some more testing and tweaking. (He also wanted to brings its
file formats and command set into closer agreement with the
different program "Vanilla Snob", which he wrote in 2002.)
It is our belief that the code is essentially correct.

License                                               
-------                                               
2008: Judy Wallace decided that "Factor Snob" be made 
available under the GNU General Public License, GPL 
(http://www.gnu.org/copyleft/gpl.html).               
                                                      
    Work using Factor Snob should cite Wallace [1],
    Boulton & Wallace [3], and Wallace & Freeman [5].


About Factor Snob
-----------------
Snob does mixture modelling by Minimum Message Length (MML). Factor
Snob can handle correlated variables by finding single-factor models.
Finding such a factor can reduce the number of classes and
therefore shorten the message length of the model.

Factor Snob is also hierarchical. It explicitly constructs a tree,
taking the tree structure into the calculation for message length.


Short History of Snob
---------------------
Wallace & Boulton [2] gave a method for estimation of Multivariate
Mixture Models over Gaussian and Multinomial probability distributions.
It was inconsistent because it totally (rather than probabilistically)
assigned things (data) to components (clusters, classes).
Boulton and Wallace [3] considered hierarchical classification.
Wallace [4] made it consistent by introducing fractional assigment.
Wallace & Freeman [5] gave the MML theory of a single-factor model,
and Wallace & Dowe [6] the Poisson and von Mises distributions.

The MML book [1] is the best general reference on MML.


The MML Book
------------
[1] C. S. Wallace,
    Statistical and Inductive Inference by Minimum Message Length,
    Springer, isbn-13:978-0-387-23795-4, 2005.


Other References
----------------
[2] C. S. Wallace & D. M. Boulton,
    An Information Measure for Classification,
    Computer J., Vol.11, No.2, pp.185-194, 1968.

[3] D. M. Boulton and C. S. Wallace,
    An information measure for hierarchic classification,
    Computer J., Vol.16, No.3, pp.254-261, 1973.

[4] C. S. Wallace,
    An improved program for classification.
    Proc. ACSC-9, pp.357-366, 1986.

[5] C. S. Wallace and P. R. Freeman,
    Single Factor Estimation by MML,
    J. Royal Stat. Soc. B, Vol.54, No.1, pp.195-209, 1992.

[6] C. S. Wallace & D.Dowe,
    MML Mixture Modelling of multi-state, Poisson, von Mises circular and
    Gaussian distributions, 28th Symp. on the Interface, pp.608-613, 1997.

---

Data Formats
------------
The program needs 2 data files.  One of these is called a "Vset" file which lists the number and types of the variables in the data.

Its first line is a character-string "name" for the Vset.  
The next line shows the number of variables.  
The next line can be blank, or can be omitted.  
Then follows a line for each variable. The general format for such a line is
```
<var-name> <type-code> <auxiliary info>
The var-name is a character string.
The type-code is one of:
    
    1  (real-valued varibale, class distn modeled by Gaussian)
    2  (multi-state)
    3  (binary)
    4  (Von-Mises angular variable, modeled by VonMises-Fisher distribution)

The auxiliary info depends on the type code.
Codes 1 and 4 need no auxilliary info.
Code 2 needs the number of states in the multi-state variable.
    This should not be too big. 20 is probably the limit of good sense and the software implements this as the limit.
```

An example Vset file, which might be called "hybrids.vset"
```
Variables_for_Hybrids
6

height	1
sex	2	2
colour	2	6
yield	1
orientation	4
dry_weight	1
```

The second file, called	a Samp file, holds the actual data.
It must have a name with suffix .samp
Its format is:

```
First line:  Name of the sample as a character string.
Next line:  Name of the Vset describing the variables in the sample.
Then a line for each variable in the Vset, giving auxiliary info about the
sample data. This depends on the variable type code.

Code 1:	  Precision (least count) of measurement.
Code 2:   Blank like (no info needed)
Code 4:   Angular unit code, 0 for radians, 1 for degrees, then precision
		of measurement (in that unit)
```

After the lines for the variables, leave a blank line and
then have a line containing (only) the number of cases in the sample.

Then comes a data record for each case. Each record must begin on a new line
but may extend over several lines. Lines should preferably not exceed 80 chars.

A case record consists of an integer case number followed by numeric values
for each variable for that case. Values are space- or newline- or tab-
separated. A case with a negative case number is ignored in building the
classification.
The absolute values of case numbers must be unique, but need not be
compact or in order.

Type 1 and 4 variables are (possibly signed) decimal values.
Type 2 values are integers in the range 1 to number-of-states.
Type 3 values are integers 1 or 2.
Missing values of any type are shown by a string of 1 or more consecutive
`=` (equals) characters. Each string represents one missing value.

An example sample file, for the example Vset above:

```
Hill-18
Variables_for_Hybrids
0.3


.01
1  10
2.0

235
  101   37.1    1   3   18.2     73      13
  115     22	2	6     ==    -50	     14
  103      =	=   1   30.5    100	     30
 -555   16.2	1	2     12    -30	    8.4
  501     40	2   4   12.2    200      ==
...

```

Usage
-----
The program can be run interactively but the best way would be to place all the commands into a command file and then run factor snob with std-input redirected (<) from the command file and std-output redirected (>) to an output file if needed. For example:

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

The fist line is the `vset` data file and the second line is the `sample` data file. The rest of the lines contain commands and their parameters one after the other. Snob can then be run as follows:

```bash
snob-factor < cmdfile > outputfile
```

Commands
--------

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

Understanding the Output
------------------------

#### Tree

The tree command gives a lot of output, some difficult to understand at first. Here is some sample output and explanation.

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

#### PrClass
If you print a class, you get a lot of detail, especially if Snob used factor models. It may say "Use Fac vv 0.99" which means it used a factor model for that class, with strength of 0.99 (near max). Then we see a comparison of the parameter costs for various kinds of models: Leaf (L), Factor (F), Dad (D), and Best (B). Here is an example from "`prclass 2 0`" in one run of vm.s:

```
  S    2  LEAF Dad    1  Age 260  Sz 394.5  Use Fac Vv  0.70  +-0.736
  Pcosts  S:    66.14  F:   110.74  D:     0.00  B:   110.74
  Tcosts  S: 13206.29  F: 12722.63  D:     0.00  B: 12953.86
  Vcost     ---------  F:   148.94
  totals  S: 13272.44  F: 13064.59  D:     0.00  B: 13064.59
  P1     5 classes,    3 leaves,  Pcost   279.3  Tcost   24798.6,  Cost  25077.8
  ```

- First read the "Pcosts" line. That shows the parameter costs for the serial model (66.14) and the factor model (110.74).
- Then the "Tcosts" line shows, I believe, the data costs. In the original version the "Best" column sometimes did not match any of the others, that bug has been fixed in recent versions.
- Later on, we see a few lines per variable (character) in the class, headed according to what kind of model was used. For multi-state (multinomial) variables, we see:

> PR --- relative frequency of states (overall)  
> FR --- relative frequency of states if we use the factor (for low-weight factors, this will roughly equal PR)  
> BP --- a measure of the influence of the factor on the states, so we can see the strength of the factor  

For continuous variables, we see:

> S --- the simple model mu and sigma (mean and standard deviation)  
> F --- the factor model mu and sigma, where 'Ld' shows the 'loading', which is how much the factor score affects these values  
