#!/usr/bin/env python3
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

import pandas as pd
import snob


if __name__ == '__main__':
    train_data = pd.read_csv("./examples/aa.csv")
    sfc = snob.SNOBClassifier(
        name='amino_acids',
        attrs={
            'weight': 'real',
            'pct_buried': 'real',
            'volume': 'real',
            'vw_volume': 'real',
            'solvent_area': 'real',
            'hydrophobicity': 'real',
            'flexibility': 'real',
            'acid_strength_cooh': 'real',
            'acid_strength_nh3': 'real',
            'isoelectric_point': 'real',
        },
        cycles=50, steps=50, moves=4, seed=1234567
    )

    sfc.fit(train_data)
    sfc.save_model('/tmp/amino_acids.mod')
    pred = sfc.predict(train_data)
    snob.show_classes(sfc.get_classes())
    print(pred)

