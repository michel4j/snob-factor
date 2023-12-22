#!/usr/bin/env python3

import pandas as pd
import snob


if __name__ == '__main__':
    df = pd.read_csv("examples/wholesale.csv")

    dset = snob.SNOBClassifier(
        name='wholesale',
        attrs={
            'Region': 'multi-state',
            'Channel': 'multi-state',
            'Fresh': 'real',
            'Milk': 'real',
            'Grocery': 'real',
            'Frozen': 'real',
            'Detergents_Paper': 'real',
            'Delicassen': 'real',

        },
        cycles=50, steps=50, moves=2, tol=0.0005, seed=1234567
    )
    results = dset.fit(df)
    snob.show_classes(results)
    print(dset.predict())
    print(dset.predict(df))

