#!/usr/bin/env python3

import pandas as pd
import snob
snob.LOG_LEVEL = 1
if __name__ == '__main__':
    df = pd.read_csv("./examples/sst.csv")
    dset = snob.SNOBClassifier(
        name='sst',
        attrs={
            'cdist': 'real',
            'ctheta': 'radians',
            'cphi': 'radians',
        },
        cycles=3, steps=40, moves=2, tol=0.01, seed=1234567
    )

    results = dset.fit(df)
    dset.save_model('/tmp/sst.model')
    snob.show_classes(results)
    snob.lib.show_pop_names()
    print(dset.predict())
    print(dset.predict(df))

