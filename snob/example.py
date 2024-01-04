#!/usr/bin/env python3

import pandas as pd
import snob
snob.LOG_LEVEL = 1


if __name__ == '__main__':
    train_data = pd.read_csv("./examples/sst.csv")
    sfc = snob.SNOBClassifier(
        name='sst',
        attrs={
            'cdist': 'real',
            'ctheta': 'radians',
            'cphi': 'radians',
        },
        from_file='/tmp/sst.mod',
        cycles=3, steps=40, moves=2, tol=0.01, seed=1234567
    )

    #sfc.fit(train_data)
    #sfc.save_model('/tmp/sst.mod')
    pred = sfc.predict(train_data)
    snob.show_classes(sfc.get_classes())
    print(pred)

