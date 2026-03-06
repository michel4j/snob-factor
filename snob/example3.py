#!/usr/bin/env python3
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

import pandas as pd
import snob


if __name__ == '__main__':
    train_data = pd.read_csv("./examples/sst.csv")
    sfc = snob.SNOBClassifier(
        name='sst_test',
        attrs={
            'stretch': 'real',
            'curve': 'real',
            'twist': 'real',
        },
        cycles=50, steps=50, moves=4, seed=1234567
    )

    sfc.fit(train_data)
    sfc.save_model('/tmp/vmd_test.mod')
    pred = sfc.predict()
    snob.show_classes(sfc.get_classes())
    print(pred)

