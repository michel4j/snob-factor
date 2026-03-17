#!/usr/bin/env python3

from numpy.testing import verbose
import os
import sys
from pathlib import Path

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

import snob

EXAMPLES = [
    "phi",
]

if __name__ == "__main__":
    if len(sys.argv) > 1:
        EXAMPLES = sys.argv[1:]

    # Loop through examples and do a more traditional classification with files
    for name in EXAMPLES:
        vset_file = Path("./examples") / f"{name}.v"
        sample_file = Path("./examples") / f"{name}.s"

        if not (vset_file.exists() and sample_file.exists()):
            continue

        print("#" * 80)
        print(f"Classifying: {name}")

        classes = snob.classify(
            vset_file, sample_file, cycles=10, steps=50, moves=2, seed=1234567, verbose=True
        )
        snob.show_classes(classes)
