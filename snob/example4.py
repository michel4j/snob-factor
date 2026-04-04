#!/usr/bin/env python3
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

import pandas as pd
import snob


if __name__ == "__main__":
    train_data = pd.read_csv("./examples/vmd.csv")
    models = [
        {'name': 'von-mises-fisher', 'attrs': ['v1', 'v3', 'v4', 'v5'], 'units': 'radians', 'epsilon': 0.001},
        {'name': 'von-mises-fisher', 'attrs': ['v2'], 'units': 'degrees', 'epsilon': 0.1},
    ]
    sfc = snob.SNOBClassifier(
        name="vmd_test",
        models=models,
        cycles=3,
        steps=50,
        moves=2,
        seed=1234567,
        verbose=True
    )

    sfc.fit(train_data)
    sfc.save_model("/tmp/vmd_test.mod")
    pred = sfc.predict(train_data)
    snob.show_classes(sfc.get_classes())
    print(pred)

    # create a new classifier from the model
    sfc2 = snob.SNOBClassifier.from_file("/tmp/vmd_test.mod")
    pred2 = sfc2.predict(train_data)
    snob.show_classes(sfc2.get_classes())
    print(pred2)
    
