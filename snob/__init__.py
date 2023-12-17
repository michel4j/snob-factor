from __future__ import annotations

import ctypes as ct
import json
import struct
import pandas as pd
import numpy as np
import os
from typing import Any
from pathlib import Path

# Load the shared library
lib_path = os.path.join(os.path.dirname(__file__), '_snob.so')
lib = ct.CDLL(lib_path)


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


lib.initialize.argtypes = [ct.c_int, ct.c_int, ct.c_int]
lib.init_population.restype = ct.c_int
lib.load_vset.argtypes = [ct.c_char_p]
lib.load_vset.restype = ct.c_int
lib.load_sample.argtypes = [ct.c_char_p]
lib.load_sample.restype = ct.c_int
lib.report_space.argtypes = [ct.c_int]
lib.report_space.restype = ct.c_int
lib.classify.argtypes = [ct.c_int, ct.c_int, ct.c_int, ct.c_double]
lib.classify.restype = Classification
lib.print_class.argtypes = [ct.c_int, ct.c_int]
lib.item_list.argtypes = [ct.c_char_p]
lib.get_assignments.restype = ct.c_int
lib.get_assignments.argtypes = [
    ct.POINTER(ct.c_int),  # for int* ids
    ct.POINTER(ct.c_int),  # for int* prim_cls
    ct.POINTER(ct.c_double),  # for double* prim_probs
    ct.POINTER(ct.c_int),  # for int* sec_cls
    ct.POINTER(ct.c_double)  # for double* sec_probs
]
lib.create_vset.argtypes = []
lib.get_class_details.argtypes = [ct.c_char_p, ct.c_size_t]
lib.save_model.argtypes = [ct.c_char_p]
lib.save_model.restype = ct.c_int
lib.load_model.argtypes = [ct.c_char_p]
lib.load_model.restype = ct.c_int

# create_vset
lib.create_vset.argtypes = [ct.c_char_p, ct.c_int]
lib.create_vset.restype = ct.c_int

# set_attribute
lib.add_attribute.argtypes = [ct.c_int, ct.c_char_p, ct.c_int, ct.c_int]
lib.add_attribute.restype = ct.c_int

# create_sample
lib.create_sample.argtypes = [ct.c_char_p, ct.c_int, ct.POINTER(ct.c_int), ct.POINTER(ct.c_double)]
lib.create_sample.restype = ct.c_int

# add_record
lib.add_record.argtypes = [ct.c_int, ct.c_char_p]
lib.add_record.restype = ct.c_int

# print_data
lib.print_var_datum.argtypes = [ct.c_int, ct.c_int]

# Mapping from Pandas dtypes to struct format characters
dtype_to_struct_fmt = {
    'int64': 'q',  # 64-bit integer
    'float64': 'd',  # double precision float
}


# Function to create format string from DataFrame
def create_format_string(df):
    """
    Given a Pandas data-frame, generate a format string for packing row data into a binary
    representation.

    :param df: Pandas Data Frame
    :return: string
    """
    format_string = ''.join(
        dtype_to_struct_fmt.get(df[col].dtype.name, '')
        for col in df.columns
    )
    return format_string




def get_prec(col, err=1e-6):
    prec = 1
    while prec < 6 and (col - col.round(prec)).abs().mean() * 10 ** prec > err:
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
    fit_info: Classification
    results: list

    def __init__(
            self,
            data: pd.DataFrame,
            name: str = 'sample',
            types: dict | None = None,
            precision: dict | None = None,
            auxs: dict | None = None
    ):
        """
        :param data: Pandas data frame containing
        :param name: name of sample data
        :param types: data types
        :param precision: data precision
        :param auxs: auxiliary information
        """
        types = {} if not types else types
        precision = {} if not precision else precision
        auxs = {} if not auxs else auxs

        self.name = name
        self.data = data
        self.columns = data.columns[1:]
        self.format = create_format_string(self.data[self.columns])
        self.attrs = {
            field: {'prec': get_prec(data[field]), 'type': get_type(data[field])}
            for field in self.columns
        }

        for field, prec in precision.items():
            self.attrs[field]['prec'] = prec

        for field, type_ in types.items():
            self.attrs[field]['type'] = type_

        for field, aux in auxs.items():
            self.attrs[field]['aux'] = aux

    def setup_from_files(self, vset_file: str | Path, sample_file: str | Path):
        initialize(log_level=1)
        lib.load_vset(str(vset_file).encode('utf-8'))
        lib.load_sample(str(sample_file).encode('utf-8'))

    def setup(self):
        initialize(log_level=1)
        # Create a Variable Set
        vset_index = lib.create_vset(self.name.encode("utf-8"), len(self.attrs))

        # Add attributes
        for i, (name, attr) in enumerate(self.attrs.items()):
            out = lib.add_attribute(
                i, name.encode('utf-8'),
                self.TYPES[attr['type']],
                attr.get('aux', 0)
            )
            print(f'Attribute {out}:{name} added!')
        print(f"VSET:{vset_index} Added with {len(self.attrs)} attributes")
        self.add_data(self.data, 'sample')

    def add_data(self, data: pd.DataFrame, name: str = 'sample'):
        # Create Sample with Auxillary information
        units = np.array([
            1 if attr['type'] == 'degrees' else 0
            for name, attr in self.attrs.items()
        ], dtype='int64')
        precs = np.array([
            10 ** -attr['prec']
            for name, attr in self.attrs.items()
        ], dtype='float64')

        size = len(data.index)
        smpl_index = lib.create_sample(
            self.name.encode('utf-8'),
            size,
            units.ctypes.data_as(ct.POINTER(ct.c_int)),
            precs.ctypes.data_as(ct.POINTER(ct.c_double))
        )

        # Now add records
        for row in data[self.columns].itertuples():
            lib.add_record(row[0], struct.pack(self.format, *row[1:]))

        # sort the samples
        lib.sort_current_sample()
        print(f'SAMPLE:{smpl_index} Added with {size} records')
        lib.show_smpl_names()
        lib.report_space(1)
        lib.peek_data()

    def fit(self) -> list:
        result = lib.classify(15, 50, 2, 0.01)
        self.fit_info = result
        buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)

        # parse JSON classification result
        lib.get_class_details(buffer, buffer_size)
        self.results = json.loads(buffer.value.decode('utf-8'))
        return self.results

    def save_model(self, filename: str):
        lib.save_model(str(filename).encode('utf-8'))

    def predict(self, data: pd.DataFrame | None) -> pd.DataFrame:
        if data is not None:
            self.add_data(data, 'predict')

        result = self.fit_info

        # Get Class Assignments
        ids = (ct.c_int * result.cases)()
        prim_cls = (ct.c_int * result.cases)()
        prim_probs = (ct.c_double * result.cases)()
        sec_cls = (ct.c_int * result.cases)()
        sec_probs = (ct.c_double * result.cases)()
        lib.get_assignments(ids, prim_cls, prim_probs, sec_cls, sec_probs)

        # Create a Pandas DataFrame
        df = pd.DataFrame({
            'item': np.ctypeslib.as_array(ids),
            'class': np.ctypeslib.as_array(prim_cls),
            'prob': np.ctypeslib.as_array(prim_probs),
            'next_class': np.ctypeslib.as_array(sec_cls),
            'next_prob': np.ctypeslib.as_array(sec_probs)
        })
        return df


def initialize(log_level: int = 0, seed: int = 0):
    """
    Initialize SNOB system. Do this before loading new
    data for an entirely new classification
    :param log_level: log level
    :param seed: random number seed, 0 will use system time

    """
    lib.initialize(0, log_level, seed)
