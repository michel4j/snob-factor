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
        from_file='/tmp/sst.model',
        cycles=3, steps=40, moves=2, tol=0.01, seed=1234567
    )

    #results = dset.fit(df)
    #dset.save_model('sst.model')
    pred = dset.predict(df)
    snob.show_classes(dset.classes_)
    snob.lib.show_pop_names()
    print(pred)

