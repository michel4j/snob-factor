#!/usr/bin/env python3
import sys
import random
sys.path.append('/home/michel/Projects/snob-factor')
import numpy as np
import pandas as pd
import snob

if __name__ == '__main__':
    df = pd.read_csv("examples/zfish-bioxas-ds.csv", na_values=[''])

    dset = snob.SNOBClassifier(
        name='xrf',
        attrs={
            v: 'real' for v in df.columns[1:]
        },
        cycles=20, steps=50, moves=2, tol=0.005, seed=1234567
    )
    print(df)
    training = np.random.choice(df.index, size=1000, replace=False)
    print(training)
    dfs = df.iloc[training].copy(deep=True)
    print(dfs)
    results = dset.fit(dfs)
    snob.show_classes(results)
    pdf = dset.predict(df)
    img_data = pdf['class'].to_numpy().reshape((108, 120, -1))

    import matplotlib as mpl
    import matplotlib.pyplot as plt

    mpl.use('TkAgg')
    plt.imshow(img_data)
    plt.show()
