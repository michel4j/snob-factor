
About this Project
------------------
This repository is an updated version of the open-source Factor Snob program for Mixture modelling by Minimum Message
Length (MML) written in C.  The original source code was obtained from Lloyd Allison's website at:
http://www.allisons.org/ll/MML/Notes/SNOB-factor/. The goal of this project is to make the program more accessible by 
providing a Python interface to the program. The original program was written in C and compiled to a binary executable.

I have made many changes to the code mainly to improve readability, and to fix some minor bugs I discovered. The current
version produces the same output as the original, except for a couple of display bugs present in the
original version (see below). Here is a summary of the changes:  

* Refactored function declarations into a central "snob.h" include file.
* Previous version included "glob.c" within other files. "glob.c" now contains some generic and exported functions.
* Replaced Old-style C-function prototypes with moder C function prototypes.
* Removed as many global variables as possible and eliminated many side effects. The previous version relied too much on  
  side effects and global variables. The current version has made variable passing more explicit. In some cases where 
  global variables are still used, they have been grouped into global structures instead. This should make it easier to
  eliminate them entirely in the future. There is still plenty of room for improvement.
* Refactored many functions to replace `goto` statements with more modern control structures.
* Replaced many print statements with a logging facility that can be dialed down using a debug level.
* Added functions for printing a summary of the existing sample data
* Added functions for generating a JSON representation of the classification result equivalent to the data produced by 
  the `prclass` command.
* Added functions for generating class assignments to the sample items. Also, useful for classification through Python.
* Fixed a bug in which during printing of Class information, the wrong class pointer was used to calculate one of the
  Factor costs. This is why the total costs for the Factor model did not match the sum of the parameter, data and 
  variable costs. It also affected the display of the boost character on the class header information.
* Added functions to allow loading `vset` and Sample data through function calls rather than from file. This allows 
  `vset` and sample information to be supplied through Python ctypes calls. 
* Added functions to allow saving and loading of the population model to and from file. This allows the population model
  to be saved and loaded through Python ctypes calls.
* Added a Scikit-learn style interface for fitting and predicting using the Factor Snob model. This allows the model to
  be used in a similar way to other Scikit-learn classification models. The features include the ability to save fitted
  models and to predict on new data based on previously saved models.
* Added a python module that can provide the data, run the Factor Snob routines and extract the results within Python, 
  using `ctypes`.

The original documentation file is included in the `docs` folder. It contains a lot of useful information about the 
original SNOB programs and may be useful for understanding some of the input and output parameters.

Any published work using this software should cite the original author's work as well as this repository. See the

The MML Book
------------
[1] C. S. Wallace,
    Statistical and Inductive Inference by Minimum Message Length,
    Springer, isbn-13:978-0-387-23795-4, 2005.

Using the Python Module
-----------------------

The Python module can be installed using `pip` as follows:

    pip install snob

The module can be used in a similar way to other Scikit-learn classification models. The following example shows how to
use the module to fit a Factor Snob model to some sample data and then use the model to predict the class of some new
data.

```python
import pandas as pd
import snob

# Load the sample data
train = pd.read_csv('train.csv')  
sfc = snob.SNOBClassifier(
    name='sst',
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

# Fit the model to the sample data
sfc.fit(train)
sfc.save_model('sst.mod')     # save the model to a file for use later

classes = sfc.get_classes()   # get the class parameters for the fitted model
snob.show_classes(classes)    # display the classification summary

# get class assignments for training data
train_pred = sfc.predict()    # assignments the training data, note that predict is called without arguments
print(train_pred)             # train_pred is a pandas DataFrame with the class assignments

# Load some new data to predict
test = pd.read_csv('test.csv')

# Predict the class of the new data
test_pred = sfc.predict(test)
print(test_pred)

```

The `SNOBClassifier` class has a number of parameters that can be used to control the behaviour of the model. The
attrs parameter is a dictionary that defines the features of the data. The keys are the names of the features and the
values are the types of the features. The types can be one of the following:

* 'real' - a real-valued feature
* 'radians' - an angle in radians
* 'degrees' - an angle in degrees
* 'multi-state' - a categorical feature with more than two but preferably fewer than 20 states
* 'binary' - a boolean or two-valued feature

Since `SNOBClassifier` is an unsupervised learning model, the `fit()` method does not require a target variable. The
`fit()` method takes a pandas DataFrame as input. The DataFrame must contain columns for each of the features defined in
the `attrs` parameter. The `fit()` method will run the Factor Snob algorithm on the data and produce a classification 
model for the data.  After fitting, the classifier will be fully parameterized and can be used to predict the classes of
new data. 

The `get_classes()` method of a fully parameterized classifier can be used to get the class parameters for the fitted
model. This returns a list of dictionaries with each dictionary representing one class. The `show_classes()` function from
the `snob` package can be used to display a summary of the class parameters. The `show_classes()` function takes the list
of class dictionaries as returned by the `get_classes()` method.

The model can be saved to a file using the `save_model()`. This method takes a single argument which is the name of the
file to save the model to. A previously saved model, can be used by specifying a `from_file` parameter during
initialization of the classifier. The `from_file` parameter should be set to the name of the file containing the saved
model. The `attrs` parameter is always required even when a `from_file` parameter is provided. The `attrs` parameter
should be the same as the one used to create the saved model.

The `predict()` method can then be used to predict the class of new data. The first time a restored model is used to 
for prediction, the model will be loaded into memory and used to fully parameterize the classifer before prections
are performed. Details of class parameters will only be available after the classifer is fully parameterized. 

The `predict()` method takes a pandas DataFrame as input. The DataFrame must contain columns for each of the features.

For example, the model above can be loaded from the saved model file and used as follows:

```python

sfc = snob.SNOBClassifier(
    name='sst',
    attrs={                   # these are the features of the data
        'distance': 'real',   # a real-valued attribute
        'theta': 'radians',   # an angle in radians angles are treated specially
        'phi': 'radians',     # another angle in radians 
    },
    from_file='sst.mod'       # load the model from the file
)
new_data = pd.read_csv('new_data.csv')
new_pred = sfc.predict(new_data)    # No need to run fit again, the model will be loaded from the file

class_info = sfc.get_classes()      # must run predict first to fully parameterize the model
snob.show_classes(class_info)
print(new_pred)

```

