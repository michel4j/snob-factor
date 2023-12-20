#!/usr/bin/env python3
import numpy as np
from scipy.stats.contingency import crosstab
import snob
from ucimlrepo import fetch_ucirepo


def joint_probability_mass_function(x, y):
    """
    Calculate the Joint Probability Mass Function from two discrete random variables (normalized)
    :param x: ndarray of discrete random variable
    :param y: ndarray of discrete random variable
    :return: jpmf: ndarray of probabilities
    """

    xtab = crosstab(x, y)
    return xtab.count / np.sum(xtab.count)


def mutual_information(x, y):
    """
    Calculate the Mutual Information of two discrete random variables
    :param x: ndarray of discrete random variable
    :param y: ndarray of discrete random variable
    :return: mutual information value
    """
    pm_func = joint_probability_mass_function(x, y) + 1e-20

    p_x = np.sum(pm_func, axis=1)
    p_y = np.sum(pm_func, axis=0)

    return np.sum(pm_func * np.log2(pm_func / (p_x[:, None] * p_y[None, :])))


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
    # assignments = dset.predict()
    # target_type = list(iris.data.targets['class'].unique())
    # targets = np.array([
    #     target_type.index(cls) for cls in iris.data.targets['class']
    # ], dtype=int)
    #
    # print(mutual_information(targets, assignments['class'].to_numpy()))
    # #print(mutual_information(targets, targets))



