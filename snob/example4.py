#!/usr/bin/env python3

import pandas as pd
import snob

if __name__ == '__main__':
    df = pd.read_csv("examples/zfish-down.csv", na_values=[''])

    dset = snob.SNOBClassifier(
        name='xrf',
        attrs={
            v: 'real' for v in df.columns[1:]
        },
        cycles=20, steps=50, moves=2, tol=0.005, seed=1234567
    )

    results = dset.fit(df)
    snob.show_classes(results)
