#!/usr/bin/env python3
import snob
from ucimlrepo import fetch_ucirepo


if __name__ == '__main__':
    iris = fetch_ucirepo(id=105)
    df = iris.data.features
    print(df)

    dset = snob.SNOBClassifier(
        name='wholesale',
        attrs={
            name: 'binary' for name in df.columns
        },
        cycles=100, steps=50, moves=2, tol=-10.0
    )
    results = dset.fit(df)
    snob.show_classes(results)




