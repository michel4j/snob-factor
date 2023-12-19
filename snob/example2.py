#!/usr/bin/env python3

import sys
from pathlib import Path

import snob

EXAMPLES = [
    'phi', 'sd1', '5m1c', '5r8c', '6m1c', '6m2r2c',
    '6r1c', '6r1b', 'd2', 'vm',
]

if __name__ == '__main__':
    if len(sys.argv) > 1:
        EXAMPLES = sys.argv[1:]

    # Loop through examples and do a more traditional classification with files
    for name in EXAMPLES:
        vset_file = Path('./examples') / f'{name}.v'
        sample_file = Path('./examples') / f'{name}.s'

        if not (vset_file.exists() and sample_file.exists()):
            continue

        print('#' * 80)
        print(f"Classifying: {name}")

        classes = snob.classify(vset_file, sample_file, cycles=20, steps=50, moves=2, tol=0.01)
        snob.show_classes(classes)
