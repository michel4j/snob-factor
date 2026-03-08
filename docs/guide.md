About Factor Snob
-----------------
This is the documentation for snob-factor, a Python wrapper for the Factor Snob C program
originally written by Chris Wallace. A lot of the documentation has been pieced together
from the original README file as well as notes and guides originally authored by Lloyd 
Allison (see http://www.allisons.org/ll/MML/Notes/SNOB-factor/).

Snob does mixture modelling by Minimum Message Length (MML). Factor
Snob can handle correlated variables by finding single-factor models.
Finding such a factor can reduce the number of classes and therefore shorten 
the message length of the model. Factor Snob is also hierarchical. It explicitly 
constructs a tree, taking the tree structure into the calculation for message length.


Short History of Snob
---------------------
Wallace & Boulton [2] gave a method for estimation of Multivariate Mixture Models over Gaussian 
and Multinomial probability distributions. It was inconsistent because it totally (rather than 
probabilistically)assigned things (data) to components (clusters, classes). Boulton and Wallace 
[3] considered hierarchical classification. Wallace [4] made it consistent by introducing fractional 
assigment. Wallace & Freeman [5] gave the MML theory of a single-factor model, and Wallace & Dowe 
[6] the Poisson and von Mises distributions. The MML book [1] is the best general reference on MML.

According to Lloyd Allison, Chris Wallace had been refining "Factor Snob" for some years before 
his death in 2004, when he found time from his other projects, including his book [1] (2005). The 
code had been used within his department on several problems, but Chris did not want to make it 
publicly available before some more testing and tweaking. He also wanted to brings its file formats 
and command set into closer agreement with the different program "Vanilla Snob", which he wrote in 2002.

I discovered this code circa 2012 and have been working on modernizing the source code and adding 
a Python wrapper for it ever since. In the process, while preserving the original algorithms, I have 
made major performance improvements, and made the code more maintainable and easier to use.

Specifically, I have:

- Added a Python wrapper using ctypes, which provides a more Pythonic scikit-learn-like interface to 
  the C code. The python interface also allows providing data as a Pandas DataFrame.
- A new binary `auto-snob` is now built. This allows calling the program with command line arguments, 
  instead the original command file interface. This is more convenient for running the program with the 
  traditional vset and sample files used by the original program.
- Added OpenMP parallelization to the C code, which provides a significant speedup for large datasets.


References
----------
[1] C. S. Wallace,
    Statistical and Inductive Inference by Minimum Message Length,
    Springer, isbn-13:978-0-387-23795-4, 2005.

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

License                                               
-------                                               
In 2008, Judy Wallace decided that "Factor Snob" be made available under the GNU General Public License, GPL 
(http://www.gnu.org/copyleft/gpl.html). Work using Factor Snob should cite Wallace [1], Boulton & Wallace [3], and 
Wallace & Freeman [5].