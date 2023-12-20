#!/usr/bin/env python3
import snob
from ucimlrepo import fetch_ucirepo


if __name__ == '__main__':
    dataset = fetch_ucirepo(id=105)
    df = dataset.data.features

    dset = snob.SNOBClassifier(
        name='wholesale',
        attrs={
            name: 'multi-state' for name in df.columns
        },
        cycles=100, steps=50, moves=3, tol=0.005
    )
    results = dset.fit(df)
    snob.show_classes(results)




