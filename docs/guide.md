## Factor Snob User's Guide

### File Formats

The file formats are similar to Vanilla Snob, but not yet identical, though csw plans to make them so when he finishes his book. That means Factor Snob expects separate files for the variables and the samples (unlike Standard Snob).

Factor Snob can also handle angular variables, by assuming they have a von Mises distribution. However, unlike Standard Snob, it does not yet handle Poisson distributions. Here are more details from the file 'dataformats' written by csw.

#### Data Formats for C-Snob. 26 Jan 2004

The program needs 2 data files. One of these is called a "Vset" file, must have suffix .vset and lists the number and types of the variables in the data.

Its first line is a character-string "name" for the Vset. The next line shows the number of variables. The next line can be blank, or can be omitted. Then follows a line for each variable. The general format for such a line is:

        <var-name> <type-code> <auxiliary info>
        The var-name is a character string.
        The type-code is one of:
                1  (real-valued varibale, class distn modeled by Gaussian)
                2  (multi-state)
                4  (Von-Mises angular variable, modeled by VonMises-Fisher
                        distribution)
        The auxiliary info depends on the type code.
        Codes 1 and 4 need no auxilliary info.
        Code 2 needs the number of states in the multi-state variable.
                This should not be too big. 20 is probably the limit of good
                sense.
An example Vset file, which might be called "hybrids.vset"

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

The second file, called a Samp file, holds the actual data. It must have a name with suffix .samp Its format is:
First line: Name of the sample as a character string.
Next line: Name of the Vset describing the variables in the sample.
Then a line for each variable in the Vset, giving auxiliary info about the sample data. This depends on the variable type code.

- Code 1: Precision (least count) of measurement.
- Code 2: Blank like (no info needed)
- Code 4: Angular unit code, 0 for radians, 1 for degrees, then precision of measurement (in that unit)

After the lines for the variables, leave a blank line and then have a line containing (only) the number of cases in the sample.

Then comes a data record for each case. Each record must begin on a new line but may extend over several lines. Lines should preferably not exceed 80 chars.

A case record consists of an integer case number followed by numeric values for each variable for that case. Values are space- or newline- or tab- separated. A case with a negative case number is ignored in building the classification. The absolute values of case numbers must be unique, but need not be compact or in order.

Type 1 and 4 variables are (possibly signed) decimal values. Type 2 values are integers in the range 1 to number-of-states. Missing values of any type are shown by a string of 1 or more consecutive '=' (equals) characters. Each string represents one missing value.

An example sample file, for the example Vset above:
```
Hill-18
Variables_for_Hybrids
0.3
.01
1  10
2.0
235
101 37.1 1 3   18.2  73  13
115     22      2       6       ==      -50     14
103     =       =       1       30.5    100     30
-555    16.2    1       2       12      -30     8.4
501     40      2       4       12.2    200     ===
....
```

### Walkthrough: commands and tactics
Because this version of Snob is explicitly hierarchical, there are new commands. Let us walk through a sample session to learn what they do. This walkthrough was written from a demo session with csw.

First, we read in the dataset 5r8c which was generated as 8 classes (two main classes, each with 4 leaves). There are 5 real (Gaussian) variables.

- doall --- works like before, only it can only make binary trees, so it fails to recover the proper structure
- tree --- shows the structure. As noted, it's not right.
- moveclass --- manually rearrange the hierarchy. That would be cheating, so we don't.
- trymoves --- automatically rearrange the hierarchy. That recovers the proper structure! (Use the tree command to see it.)
- flatten --- if we want to see what it would look like under standard Snob. So that has destroyed our nice tree model.
- nosubs --- a toggle which turns off subclasses, making it behave more like Standard Snob.
- doall --- tidy things up after the flatten (gets about 16 flat classes). OK, that's nice, but let's make a tree again. Better toggle subs back on.
- nosubs --- Better turn this on before doing the next command! (Else Snob gets flummoxed and throws things away.)
- binhier --- Builds a binary tree from a flat structure.

### Understanding the Output
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
- Then the "Tcosts" line shows, I believe, the data costs. I'm not sure why the "Best" column doesn't match any of the others.
- Later on, we see a few lines per variable (character) in the class, headed according to what kind of model was used. For multi-state (multinomial) variables, we see:

> PR --- relative frequency of states (overall)  
> FR --- relative frequency of states if we use the factor (for low-weight factors, this will roughly equal PR)  
> BP --- a measure of the influence of the factor on the states, so we can see the strength of the factor  

For continuous variables, we see:

> S --- the simple model mu and sigma (mean and standard deviation)  
> F --- the factor model mu and sigma, where 'Ld' shows the 'loading', which is how much the factor score affects these values  
