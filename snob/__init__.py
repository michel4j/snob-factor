from __future__ import annotations

import ctypes as ct
import json
import os
import struct
from enum import IntFlag, auto
from pathlib import Path
from typing import Dict, Literal, Any
import numpy as np
import pandas as pd

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
lib.set_control_flags.argtypes = [ct.c_int]
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

DataType = Literal['real', 'multi-state', 'binary', 'degrees', 'radians']


class SnobContextManager:
    """
    Saves the SNOB Context before performing certain operations
    and restores it after.
    """

    def __enter__(self):
        lib.save_context()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        lib.restore_context()


class SNOBClassifier:
    TypeValue = {
        'real': 1,
        'multi-state': 2,
        'binary': 3,
        'degrees': 4,
        'radians': 4,
    }
    TypeFormat = {
        'real': 'd',
        'multi-state': 'q',
        'binary': 'q',
        'degrees': 'd',
        'radians': 'd'
    }
    summary: Classification | None
    classes_: list

    def __init__(
            self,
            attrs: Dict[str, DataType],
            cycles: int = 20,
            steps: int = 50,
            moves: int = 2,
            tol: float = 5e-2,
            name: str = 'mml'
    ):
        """
        :param attrs: a dictionary mapping attribute  names to attribute types
        :param cycles: Maximum number of cost-assign-adjust-move cycles
        :param steps: number of steps of cost-assign-adjust
        :param moves: maximum number of failed attempts to move classes
        :param tol:  Convergence tolerance. Stops trying if percentage drop in message costs is less than this
        :param name: Internal Name of classifier, default "mml"
        """

        self.attrs = attrs
        self.columns = self.attrs.keys()
        self.format = self.get_format_string()
        self.cycles = cycles
        self.steps = steps
        self.moves = moves
        self.tol = tol
        self.name = name

        self.summary = None

    def get_format_string(self):
        """
        Generate a format string for packing row data into a binary
        representation.
        :return: string
        """
        return ''.join(self.TypeFormat[type_] for field, type_ in self.attrs.items())

    @staticmethod
    def get_precision(col) -> float:
        """
        Given an array of floats, return the estimated precision of the values
        :param col: array
        :return: integer
        """
        err = 1e-6
        prec = 1
        while prec < 6 and (col - col.round(prec)).abs().mean() * 10 ** prec > err:
            prec += 1
        return 10 ** -prec

    def add_vset(self, data: pd.DataFrame):
        """
        Create a new vset for this data
        :param data: Pandas data frame containing the data
        """

        index = lib.create_vset(self.name.encode("utf-8"), len(self.attrs))
        # Add attributes
        for i, (name, _type) in enumerate(self.attrs.items()):
            if _type in ['multi-state', 'binary']:
                aux = data[name].notnull().nunique()
                if aux == 2:
                    # Force use of Binary for 2-valued states
                    _type = 'binary'
                    self.attrs[name] = 'binary'
                elif aux > 20:
                    # Limit auto to 20, remaining values will be marked as missing
                    aux = 20
            else:
                aux = 0
            out = lib.add_attribute(i, name.encode('utf-8'), self.TypeValue[_type], aux)

    def add_data(self, data: pd.DataFrame, name: str = 'sample'):
        """
        Create a new sample and load the data
        :param data: Pandas data frame containing the data
        :param name: name of dataset, default "sample"
        """
        units = np.array([
            1 if type_ == 'degrees' else 0
            for name, type_ in self.attrs.items()
        ], dtype='int64')
        precs = np.array([
            self.get_precision(data[name]) for name, type_ in self.attrs.items()
        ], dtype='float64')

        size = len(data.index)
        index = lib.create_sample(
            name.encode('utf-8'),
            size,
            units.ctypes.data_as(ct.POINTER(ct.c_int)),
            precs.ctypes.data_as(ct.POINTER(ct.c_double))
        )

        # Now add records
        for row in data[self.columns].itertuples():
            lib.add_record(row[0], struct.pack(self.format, *row[1:]))

        # sort the samples
        lib.sort_current_sample()
        lib.show_smpl_names()
        lib.report_space(1)
        lib.peek_data()

    def fit(self, data: pd.DataFrame, y: Any = None) -> list:
        """
        Build and Fit the model
        :param data: Pandas data frame. Columns are features, and rows are samples
        :param y: Not used, present for compatibility with Scikit-Learn interface
        :return: list of dictionaries with each representing details about classes found
        """
        initialize(log_level=1)
        self.add_vset(data)
        self.add_data(data, name='sample')
        result = lib.classify(self.cycles, self.steps, self.moves, self.tol)
        self.summary = result
        buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)

        # parse JSON classification result
        lib.get_class_details(buffer, buffer_size)
        self.classes_ = json.loads(buffer.value.decode('utf-8'))
        return self.classes_

    def save_model(self, filename: str | Path):
        lib.save_model(str(filename).encode('utf-8'))

    def predict(self, data: pd.DataFrame | None) -> pd.DataFrame:
        if data is not None:
            set_control_flags(Adjust.SCORES)
            with SnobContextManager():
                self.add_data(data, 'predict')

        # get assignments
        ids = (ct.c_int * self.summary.cases)()
        prim_cls = (ct.c_int * self.summary.cases)()
        prim_probs = (ct.c_double * self.summary.cases)()
        sec_cls = (ct.c_int * self.summary.cases)()
        sec_probs = (ct.c_double * self.summary.cases)()
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


class Adjust(IntFlag):
    SCORES = auto()
    TREE = auto()
    PARAMS = auto()
    ALL = SCORES | TREE | PARAMS


class ClassType(IntFlag):
    DAD = auto()
    LEAF = auto()
    SUB = auto()
    ALL = DAD | LEAF | SUB


def initialize(log_level: int = 0, seed: int = 0):
    """
    Initialize SNOB system. Do this before loading new
    data for an entirely new classification
    :param log_level: log level
    :param seed: random number seed, 0 will use system time

    """
    lib.initialize(0, log_level, seed)


def set_control_flags(flags: Adjust):
    """
    Set Adjustment flags
    :param flags: Adjust Flags
    """
    lib.set_control_flags(flags)


def classify(
        vset_file: str | Path,
        sample_file: str | Path,
        cycles: int = 3,
        steps: int = 50,
        moves: int = 3,
        tol: float = 1e-2
):
    """
    Run a classification based on vset and sample files
    :param vset_file: VSET File
    :param sample_file: Sample File
    :param cycles:  Number of classification cycles
    :param steps: Number of doall steps
    :param moves: Number of trymoves steps
    :param tol:  percentage drop above which we should continue trying
    :return: list of class dictionaries
    """
    initialize(log_level=1)
    lib.load_vset(str(vset_file).encode('utf-8'))
    lib.load_sample(str(sample_file).encode('utf-8'))

    result = lib.classify(cycles, steps, moves, tol)
    buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
    buffer = ct.create_string_buffer(buffer_size)

    # parse JSON classification result
    lib.get_class_details(buffer, buffer_size)
    return json.loads(buffer.value.decode('utf-8'))
