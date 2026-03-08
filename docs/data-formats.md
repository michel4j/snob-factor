Data Formats
============

Data for Python API
-------------------

The Python interface to snob-factor is very simple. You only need to provide a pandas DataFrame with the data,
and a list of variable types.

For example:
```python
import snob

# Load data
data = pd.read_csv('data.csv')

# Set up the model
model = snob.SNOBClassifier(
    name='my_sample',
    attrs={                   # these are the features of the data
        'distance': 'real',   # a real-valued attribute
        'theta': 'radians',   # an angle in radians angles are treated specially
        'phi': 'radians',     # another angle in radians
    },
    cycles=3,                 # maximum number of cycles to run
    steps=40,                 # maximum number of estimation/assignment steps to run
    moves=2,                  # maximum number of failed class shuffles before giving up each cycle
    tol=0.01,                 # minimum percentage improvement in cost to indicate convergence
    seed=1234567              # random number seed
)

model.fit(data)
```



Data for auto-snob executable
-----------------------------

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

