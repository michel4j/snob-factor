#!/usr/bin/env python3
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

import pandas as pd
import snob


if __name__ == "__main__":
    train_data = pd.read_csv("./examples/vmd.csv")
    attrs = {
        "v1": "radians",
        "v2": "degrees",
        "v3": "radians",
        "v4": "radians",
        "v5": "radians",
    }
    sfc = snob.SNOBClassifier(
        name="vmd_test",
        attrs=attrs,
        cycles=50,
        steps=50,
        moves=4,
        seed=1234567,
    )

    sfc.fit(train_data)
    sfc.save_model("/tmp/vmd_test.mod")
    pred = sfc.predict(train_data)
    snob.show_classes(sfc.get_classes())
    print(pred)


    # create a new classifier from the model
    sfc2 = snob.SNOBClassifier(from_file="/tmp/vmd_test.mod")
    pred2 = sfc2.predict(train_data)
    print(sfc2.get_classes())
    print(pred2)
    
