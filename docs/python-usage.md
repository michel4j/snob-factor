# Python Implementation and Usage

Factor Snob provides a robust Python API for programmatic mixture modelling. This is achieved via a C-extension that interfaces with the underlying C library.

## The `snob` Module

To use the Python API, you first need to import the `snob` module:

```python
import snob
```

We provide two distinct ways to run the classification model:
1. An object-oriented scikit-learn compatible `SNOBClassifier`.
2. A functional interface taking traditional `.v` and `.s` file paths directly.

### 1. The `SNOBClassifier` interface

The `SNOBClassifier` provides a Pandas-friendly approach to modeling data, useful when working within Jupyter notebooks or existing modern ML pipelines.

**Example Usage**:
```python
import pandas as pd
import snob

def train_and_predict():
    # Load dataset
    train_data = pd.read_csv("./examples/vmd.csv")
    
    # Initialize the classifer
    sfc = snob.SNOBClassifier(
        name="vmd_test",
        attrs={
            "v1": "real",
            "v2": "real",
            "v3": "real",
            "v4": "real",
            "v5": "real",
        },
        cycles=50,
        steps=50,
        moves=4,
        seed=1234567,
    )

    # Fit the model and optionally save it for later use
    sfc.fit(train_data)
    sfc.save_model("/tmp/vmd_test.mod")
    
    # Predict to extract probabilistic assignments
    pred = sfc.predict()
    
    # assign predicted classes and probabilities to train_data
    for column in ["major_class", "minor_class", "major_prob", "minor_prob"]:
        train_data[column] = pred[column].values
        
    # Print out summary tree architecture
    snob.show_classes(sfc.get_classes())
    print(train_data)
```

### 2. Traditional functional API

If you already have data formatted in Snob `.v` (vset) and `.s` (sample) files, you can use the `snob.classify` function to directly parse the files and run the MML classification.

**Example Usage**:
```python
import snob
from pathlib import Path

def run_legacy_model():
    vset_file = Path("./examples/phi.v")
    sample_file = Path("./examples/phi.s")

    # The classification runs completely inside C extension and returns model details
    classes = snob.classify(
        vset_file, 
        sample_file, 
        cycles=25, 
        steps=50, 
        moves=4, 
        seed=1234567, 
        tol=5e-3
    )
    
    # Print out summary tree architecture
    snob.show_classes(classes)
```

### 3. Saving and Loading Models

The `SNOBClassifier` provides a method to save the model to a file and load it later. This is useful for reusing the same model multiple times without retraining.

**Example Usage**:

To train and save a model:
```python
def train_and_save_model():
    # Load dataset
    train_data = pd.read_csv("./examples/vmd.csv")
    
    # Initialize the classifer
    sfc = snob.SNOBClassifier(
        name="vmd_test",
        attrs={
            "v1": "real",
            "v2": "real",
            "v3": "real",
            "v4": "real",
            "v5": "real",
        },
        cycles=50,
        steps=50,
        moves=4,
        seed=1234567,
    )

    # Fit the model
    sfc.fit(train_data)

    # Print out summary tree architecture
    snob.show_classes(sfc.get_classes())

    # Save the model
    sfc.save_model("vmd_test.mod")
    
```

To reload a trained model for prediction:
```python
def load_and_predict():
    # Load the model
    sfc = snob.SNOBClassifier(
        name="vmd_test",
        attrs={
            "v1": "real",
            "v2": "real",
            "v3": "real",
            "v4": "real",
            "v5": "real",
        },
        cycles=50,
        steps=50,
        moves=4,
        seed=1234567,
        from_file="vmd_test.mod"    # load trained model
    )
    
    # Load dataset
    new_data = pd.read_csv("./test_data.csv")
    
    # Predict to extract probabilistic assignments
    pred = sfc.predict(new_data)
    
    # Print out summary tree architecture
    snob.show_classes(sfc.get_classes())
    print(pred)
```
