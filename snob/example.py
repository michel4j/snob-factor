#!/usr/bin/env python3

import pandas as pd
import snob

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
