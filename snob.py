#!/usr/bin/env python3

import ctypes as ct
import sys
import json
class Classification(ct.Structure):
    """ Result """

    _fields_ = [
        ('classes', ct.c_int),
        ('leaves', ct.c_int),
        ('attrs', ct.c_int),
        ('cases', ct.c_int),       
        ('model', ct.c_double),
        ('data', ct.c_double),
        ('message', ct.c_double)
    ]

snob = ct.cdll.LoadLibrary('./libsnob.so')
snob.initialize.argtypes = [ct.c_int, ct.c_int, ct.c_int]
snob.load_vset.argtypes = [ct.c_char_p]
snob.load_vset.restype = ct.c_int
snob.load_sample.argtypes = [ct.c_char_p]
snob.load_sample.restype = ct.c_int
snob.report_space.argtypes = [ct.c_int]
snob.report_space.restype = ct.c_int
snob.classify.argtypes = [ct.c_int, ct.c_int, ct.c_int, ct.c_double]
snob.classify.restype = Classification
snob.print_class.argtypes = [ct.c_int, ct.c_int]
snob.item_list.argtypes = [ct.c_char_p]
snob.get_assignments.restype = ct.c_int
snob.get_assignments.argtypes = [
    ct.POINTER(ct.c_int),    # for int* ids
    ct.POINTER(ct.c_int),    # for int* prim_cls
    ct.POINTER(ct.c_double), # for double* prim_probs
    ct.POINTER(ct.c_int),    # for int* sec_cls
    ct.POINTER(ct.c_double) # for double* sec_probs
] 
snob.get_class_details.argtypes = [ct.c_char_p, ct.c_size_t]
snob.save_model.argtypes = [ct.c_char_p]
snob.save_model.restype = ct.c_int
snob.load_model.argtypes = [ct.c_char_p]
snob.load_model.restype = ct.c_int

EXAMPLES = [
    'phi',
    'sd1',
    '5m1c',
    '5r8c',
    '6m1c',
    '6m2r2c',
    '6r1c',
    '6r1b',
    'd2',
    'vm',
]
import uuid;
from pathlib import Path
from asciitree import LeftAligned
import pandas as pd;
import numpy as np;

def subtree(node: int, info: dict, level=0):
    return {
        v: subtree(v, info)
        for v in [ x['id'] for x in info if x['parent'] == node]
    }

class DataSet():
    def __init__(self, name: str, attributes: dict): 
        self.name = name
        self.attrs = attributes
        self.model = None

    def fit(self, data: pd.DataFrame, name: str = 'sample') -> list:
        pass

    def predict(self, data: pd.DataFrame, name: str = 'sample') -> pd.DataFrame:
        pass

    def get_vset(self):
        return (
            f"{self.name}\n"
            f"{len(self.attrs)}\n\n"
        ) + f"\n".join([
            f"{name} {attr['type']:8d} {self._get_aux(attr)}"
            for name, attr in self.attrs.items()
        ])

    def get_sample(self):
        pass

    def _get_aux(self, attr):
        if attr['type'] in [2, 3]:
            return f'{attr["aux"]:8d}'
        else:
            return ''

if __name__ == '__main__':
    snob.initialize(0, 0, 0)
    if len(sys.argv) > 1:
        EXAMPLES = sys.argv[1:]

    for name in EXAMPLES:
        vset_file = Path('./examples') / f'{name}.v'
        sample_file = Path('./examples') / f'{name}.s'
        model_file = Path(f'/tmp/{name}.mod')
        report_file = Path(f'/tmp/{name}.rep')
    
        if not (vset_file.exists() and sample_file.exists()):
            continue

        print('#'*80)
        snob.report_space(1)
        print(f"Classifying: {name}")
        
        snob.load_vset(str(vset_file).encode('utf-8'))
        snob.load_sample(str(sample_file).encode('utf-8'))
        result = snob.classify(20, 50, 3, 0.05)
        buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)

        
        # parse JSON classification result
        snob.get_class_details(buffer, buffer_size)
        info = json.loads(buffer.value.decode('utf-8'))

        tree = subtree(-1, info)
        tr = LeftAligned()
        print("-"*79)
        print("Classification Tree")
        print("-"*79)
        print(tr(tree))
        snob.show_population()
        snob.save_model(str(model_file).encode('utf-8'))
        #snob.item_list(str(report_file).encode('utf-8'))

        # Get Class Assignments
        ids = (ct.c_int * result.cases)()
        prim_cls = (ct.c_int * result.cases)()
        prim_probs = (ct.c_double *result.cases)()
        sec_cls = (ct.c_int * result.cases)()
        sec_probs = (ct.c_double *result.cases)()
        snob.get_assignments(ids, prim_cls, prim_probs, sec_cls, sec_probs)

        # Create a Pandas DataFrame
        df = pd.DataFrame({
            'item': np.ctypeslib.as_array(ids),
            'class': np.ctypeslib.as_array(prim_cls),
            'prob': np.ctypeslib.as_array(prim_probs),
            'next_class': np.ctypeslib.as_array(sec_cls),
            'next_prob': np.ctypeslib.as_array(sec_probs)
        })

        print(df)

        snob.reset()
    
