#!/usr/bin/env python3

import snob
import sys
import pandas as pd
import pprint
from pathlib import Path
from asciitree import LeftAligned

EXAMPLES = [
    'phi', 'sd1', '5m1c', '5r8c', '6m1c', '6m2r2c',
    '6r1c', '6r1b', 'd2', 'vm',
]


# Function to make a tree structure from flat data
def subtree(node: int, info: list, level=0):
    return {
        v: subtree(v, info)
        for v in [x['id'] for x in info if x['parent'] == node]
    }


def show_tree(classes):
    tree = subtree(-1, classes)
    tr = LeftAligned()
    print("-" * 79)
    print("Classification Tree")
    print("-" * 79)
    print(tr(tree))


if __name__ == '__main__':
    if len(sys.argv) > 1:
        EXAMPLES = sys.argv[1:]

    df = pd.read_csv("./examples/sst.csv")
    dset = snob.SNOBClassifier(
        name='sst',
        attrs={
            'cdist': 'real',
            'ctheta': "radians",
            "cphi": "radians",
        },
        cycles=20, steps=50, moves=2, tol=0.05
    )
    results = dset.fit(df)
    pprint.pprint(results)
    show_tree(results)

    # Loop through examples and do a more traditional classification with files
    for name in EXAMPLES:
        vset_file = Path('./examples') / f'{name}.v'
        sample_file = Path('./examples') / f'{name}.s'

        if not (vset_file.exists() and sample_file.exists()):
            continue

        print('#' * 80)
        print(f"Classifying: {name}")

        classes = snob.classify(vset_file, sample_file, cycles=3, steps=50, moves=2, tol=0.01)
        class_tree = subtree(-1, classes)
        tr = LeftAligned()
        print("-" * 80)
        print("Classification Tree")
        print("-" * 80)
        print(tr(class_tree))

