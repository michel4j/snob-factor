#!/usr/bin/env python3

import pandas as pd
import snob


if __name__ == '__main__':
    df = pd.read_csv("examples/countries.csv", na_values=[''])

    dset = snob.SNOBClassifier(
        name='countries',
        attrs={
            #'Region': 'multi-state',
            'Population': 'real',
            'Pop_Density': 'real',
            'Net_Migration': 'real',
            'GDP': 'real',
            'Literacy': 'real',
            'Phones': 'real',
            'Birthrate': 'real',
            'Deathrate': 'real',
            'Infant_Mortality': 'real'
        },
        cycles=50, steps=50, moves=2, tol=0.0005, seed=1234567
    )
    print(df)
    results = dset.fit(df)
    snob.show_classes(results)
    print(dset.predict())
    print(dset.predict(df))

