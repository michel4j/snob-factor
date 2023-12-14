#!/usr/bin/env python3

import ctypes as ct
import sys
import json
import struct
import uuid;
from pathlib import Path
from asciitree import LeftAligned
import pandas as pd;
import numpy as np;

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



# Mapping from Pandas dtypes to struct format characters
dtype_to_struct_fmt = {
    'int64': 'q',  # 64-bit integer
    'float64': 'd',  # double precision float
    'object': 's',  # string/bytes; needs length
    # Add other mappings as needed
}

# Function to create format string from DataFrame
def create_format_string(df):
    format_string = 'q'
    for col in df.columns:
        dtype = str(df[col].dtype)
        if dtype in dtype_to_struct_fmt:
            format_char = dtype_to_struct_fmt[dtype]
            # Special handling for strings to include length
            if dtype == 'object':
                max_length = df[col].str.len().max()
                format_char = f'{max_length}s'
            format_string += format_char
        else:
            raise ValueError(f"Unhandled data type: {dtype}")
    return format_string

# Function to make a tree structure from flat data
def subtree(node: int, info: dict, level=0):
    return {
        v: subtree(v, info)
        for v in [ x['id'] for x in info if x['parent'] == node]
    }


def get_prec(col, err=1e-6):
    prec = 1
    while prec < 6 and (col - col.round(prec)).abs().mean() * 10**prec > err:
        prec += 1
    return prec

def get_type(col):
    return {
        'int64': 'multistate',
        'float64': 'real',
    }.get(col.dtype.name, 1)

class DataSet():
    TYPES = {
        'real': 1,
        'multistate': 2,
        'binary': 3,
        'degree': 4, 
        'radians': 4,
    }

    def __init__(self, data: pd.DataFrame, name: str = 'sample', types: dict | None = None, precision: dict | None = None, auxs: dict | None = None): 
        types = {} if not types else types
        precision = {} if not precision else precision
        auxs = {} if not auxs else auxs

        self.name = name
        self.data = data
        self.columns = df.columns[1:]
        self.format = create_format_string(self.data[self.columns])
        self.attrs = {field: {'prec': get_prec(df[field]), 'type': get_type(df[field])} for field in self.columns}
        for field, prec in precision.items():
            self.attrs[field]['prec'] = prec
        
        for field, type_ in types.items():
            self.attrs[field]['prec'] = self.TYPES[type_]     

        for field, aux in auxs.items():   
            self.attrs[field]['aux'] = aux     

    def fit(self) -> list:
        pass

    def predict(self, data: pd.DataFrame, name: str = 'sample') -> pd.DataFrame:
        pass

    def get_vset(self):
        return (
            f"{self.name}\n"
            f"{len(self.attrs)}\n\n"
        ) + f"\n".join([
            f"{name:<10s} {self._get_type_int(attr):2d} {self._get_aux(attr):2s}"
            for name, attr in self.attrs.items()
        ])
    
    def encode_row(self, row):
        values = (row.name,) + tuple(row[col] for col in self.columns)
        return struct.pack(self.format, *values)
    
    def get_sample(self):
        return self.data[self.columns].apply(lambda row: self.encode_row(row), axis=1)

    def _get_aux(self, attr):
        if attr['type'] in [2, 3]:
            return f'{attr["aux"]:8d}'
        else:
            return ''
    
    def _get_type_int(self, attr):
        return self.TYPES.get(attr['type'], 1)


EXAMPLES = [
    'phi', 'sd1', '5m1c', '5r8c', '6m1c', '6m2r2c',
    '6r1c', '6r1b', 'd2', 'vm',
]
if __name__ == '__main__':
    snob.initialize(0, 0, 0)
    if len(sys.argv) > 1:
        EXAMPLES = sys.argv[1:]

    df = pd.read_csv("./examples/sst.csv")
    dataset = DataSet(
        data = df, 
        types={'theta': 'radians', 'phi': "radians", "ctheta": "radians", "cphi": 'radians'}
    )
    print(dataset.get_vset())
    print(dataset.get_sample())
    sys.exit(1)
        

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
        print(buffer.value.decode('utf-8'))
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
        print(json.dumps(info, indent=4))
        print(df)

        snob.reset()
    
