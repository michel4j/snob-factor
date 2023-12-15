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
snob.init_population.restype = ct.c_int
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
snob.create_vset.argtypes =[]
snob.get_class_details.argtypes = [ct.c_char_p, ct.c_size_t]
snob.save_model.argtypes = [ct.c_char_p]
snob.save_model.restype = ct.c_int
snob.load_model.argtypes = [ct.c_char_p]
snob.load_model.restype = ct.c_int

# create_vset
snob.create_vset.argtypes = [ct.c_char_p, ct.c_int]
snob.create_vset.restype = ct.c_int

# set_attribute
snob.add_attribute.argtypes = [ct.c_int, ct.c_char_p, ct.c_int, ct.c_int]
snob.add_attribute.restype = ct.c_int

# create_sample
snob.create_sample.argtypes = [ct.c_char_p, ct.c_int, ct.POINTER(ct.c_int), ct.POINTER(ct.c_double)]
snob.create_sample.restype = ct.c_int

# add_record
snob.add_record.argtypes = [ct.c_int, ct.c_char_p]
snob.add_record.restype = ct.c_int

# print_data
snob.print_var_datum.argtypes = [ct.c_int, ct.c_int]


# Mapping from Pandas dtypes to struct format characters
dtype_to_struct_fmt = {
    'int64': 'q',  # 64-bit integer
    'float64': 'd',  # double precision float
    # Add other mappings as needed
}

# Function to create format string from DataFrame
def create_format_string(df):
    format_string = ''.join(
        dtype_to_struct_fmt.get(df[col].dtype.name, '')
        for col in df.columns
    )
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
        'degrees': 4, 
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
        self.attrs = {
            field: {'prec': get_prec(df[field]), 'type': get_type(df[field])}
            for field in self.columns
        }
        
        for field, prec in precision.items():
            self.attrs[field]['prec'] = prec
        
        for field, type_ in types.items():
            self.attrs[field]['type'] = type_     

        for field, aux in auxs.items():   
            self.attrs[field]['aux'] = aux     

    def setup(self):
        # Create a Variable Set 
        vset_index = snob.create_vset(self.name.encode("utf-8"), len(self.attrs))

        # Add attributes
        for i, (name, attr) in enumerate(self.attrs.items()):
            out = snob.add_attribute(
                i, name.encode('utf-8'), 
                self.TYPES[attr['type']],
                attr.get('aux', 0)
            )
            print(f'Attribute {out}:{name} added!')
        print(f"VSET:{vset_index} Added with {len(self.attrs)} attributes")

        # Create Sample with Auxillary information
        units = np.array([
            1 if attr['type'] == 'degrees' else 0 
            for name, attr in self.attrs.items()
        ], dtype='int64')
        precs = np.array([
            10**-attr['prec'] 
            for name, attr in self.attrs.items()
        ], dtype='float64')

        size = len(self.data.index)
        smpl_index = snob.create_sample(
            self.name.encode('utf-8'), 
            size, 
            units.ctypes.data_as(ct.POINTER(ct.c_int)), 
            precs.ctypes.data_as(ct.POINTER(ct.c_double))
        )

        # Now add records
        for row in self.data[self.columns].itertuples():
            snob.add_record(row[0], struct.pack(self.format, *row[1:]))
        
        # sort the samples
        snob.sort_current_sample()

        print(f'SAMPLE:{smpl_index} Added with {size} records')
        snob.show_smpl_names()
        snob.report_space(1)
        snob.peek_data()

    def fit(self) -> list:
        result = snob.classify(15, 50, 2, 0.01)
        buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)

        # parse JSON classification result
        snob.get_class_details(buffer, buffer_size)
        self.results = json.loads(buffer.value.decode('utf-8'))

    def predict(self, data: pd.DataFrame, name: str = 'sample') -> pd.DataFrame:
        pass



EXAMPLES = [
    'phi', 'sd1', '5m1c', '5r8c', '6m1c', '6m2r2c',
    '6r1c', '6r1b', 'd2', 'vm',
]
if __name__ == '__main__':
    snob.initialize(0, 0, 0)
    if len(sys.argv) > 1:
        EXAMPLES = sys.argv[1:]

    df = pd.read_csv("./examples/sst.csv")
    dset = DataSet(
        data = df, 
        name = 'sst',
        types={
            'theta': 'radians', 
            'phi': "radians", 
            "ctheta": "radians", 
            "cphi": 'radians'
        }
    )
    print(dset.attrs)
    dset.setup()
    dset.fit()
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
        #print(buffer.value.decode('utf-8'))
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

        snob.peek_data()
            

        snob.reset()
    
