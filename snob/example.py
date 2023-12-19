#!/usr/bin/env python3

import pprint
import sys
from pathlib import Path

import pandas as pd
from asciitree import LeftAligned

import snob

# Function to make and display a tree
def show_tree(info):
    tree_dict = snob.build_tree(-1, info)
    tree = LeftAligned()
    print("-" * 80)
    print("Classification Tree")
    print("-" * 80)
    print(tree(tree_dict))


if __name__ == '__main__':
    df = pd.read_csv("./examples/sst.csv")
    dset = snob.SNOBClassifier(
        name='sst',
        attrs={
            'cdist': 'real',
            'ctheta': "radians",
            "cphi": "radians",
        },
        cycles=20, steps=50, moves=2, tol=0.01, seed=1234567
    )
    results = dset.fit(df)
    snob.show_classes(results)
