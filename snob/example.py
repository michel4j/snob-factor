#!/usr/bin/env python3
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

import pandas as pd
import snob


if __name__ == '__main__':
    train_data = pd.read_csv("./examples/vmd.csv")
    sfc = snob.SNOBClassifier(
        name='vmd_test',
        attrs={
            'v1': 'real',
            'v2': 'real',
            'v3': 'real',
            'v4': 'real',
            'v5': 'real',
        },
        cycles=50, steps=50, moves=4, seed=1234567
    )

    sfc.fit(train_data)
    sfc.save_model('/tmp/vmd_test.mod')
    pred = sfc.predict()
    train_data['major_class'] = pred['class'].values
    train_data['minor_class'] = pred['next_class'].values
    train_data['major_prob'] = pred['prob'].values
    train_data['minor_prob'] = pred['next_prob'].values
    snob.show_classes(sfc.get_classes())
    print(train_data)

