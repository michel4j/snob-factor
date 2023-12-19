from __future__ import annotations

import ctypes as ct
import json
import os
import struct
import time
import re
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


class Timer:
    """
    A context manager for measuring the process times and reporting them at the end
    """
    proc: float
    wall: float

    def __init__(self):
        self.proc = 0.0
        self.wall = 0.0

    def __enter__(self):
        self.proc = time.process_time()
        self.wall = time.time()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        proc_time = time.process_time() - self.proc
        wall_time = time.time() - self.wall
        print(
            "\n"
            f"CPU Time:     {proc_time:10.3f} s\n"
            f"Elapsed Time: {wall_time:10.3f} s\n"
            "\n"
        )


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
            name: str = 'mml',
            seed: int = 0
    ):
        """
        :param attrs: a dictionary mapping attribute  names to attribute types
        :param cycles: Maximum number of cost-assign-adjust-move cycles
        :param steps: number of steps of cost-assign-adjust
        :param moves: maximum number of failed attempts to move classes
        :param tol:  Convergence tolerance. Stops trying if percentage drop in message costs is less than this
        :param name: Internal Name of classifier, default "mml"
        :param seed:  Random number seed, 0 implies no seed.
        """

        self.attrs = attrs
        self.columns = self.attrs.keys()
        self.format = self.get_format_string()
        self.cycles = cycles
        self.steps = steps
        self.moves = moves
        self.tol = tol
        self.name = name
        self.seed = seed

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
        with Timer():
            initialize(log_level=1, seed=self.seed)
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
    :param steps: Number of do_all steps
    :param moves: Number of try_move steps
    :param tol:  percentage drop above which we should continue trying
    :return: list of class dictionaries
    """
    with Timer():
        initialize(log_level=1)
        lib.load_vset(str(vset_file).encode('utf-8'))
        lib.load_sample(str(sample_file).encode('utf-8'))
        lib.peek_data()
        result = lib.classify(cycles, steps, moves, tol)
        buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)

        # parse JSON classification result
        lib.get_class_details(buffer, buffer_size)
        return json.loads(buffer.value.decode('utf-8'))


def build_tree(node: int, info: list) -> dict:
    """
    Given a list of class dictionaries, build a recursive tree structure where the key is the class-id
    and the value is a dictionary containing child classes

    :param node: class-id of starting node
    :param info: list of class dictionaries
    :return: Nested recursive dictionary mapping class-ids to subtree dictionaries
    """
    return {
        v: build_tree(v, info)
        for v in [x['id'] for x in info if x['parent'] == node]
    }


def ascii_tree(tree_dict: dict, prefix: str = "", root: bool = False) -> str:
    """
    Generate the text representation of an item in a tree
    :param tree_dict: nested dictionary of nodes
    :param prefix: string line prefix
    :param root: bool, if true, treat first level as root
    """
    text = ""
    size = len(tree_dict)
    for i, (node, sub_dict) in enumerate(tree_dict.items()):
        if root:
            elbow, stem = (' ', ' ')
        else:
            elbow, stem = ('├', '│') if i < size - 1 else ('└', ' ')

        text += f'{prefix}{elbow}── {node}\n'
        text += ascii_tree(sub_dict, f'{prefix}{stem}   ')
    return text


def show_tree(info):
    """
    Build and display an ascii tree from the classification results
    :param info: list of class dictionaries
    """
    tree_dict = build_tree(-1, info)
    tree_text = ascii_tree(tree_dict, root=True)
    print("-" * 80)
    print("Classification Tree")
    print("-" * 80)
    print(tree_text)
    print("-" * 80)


def show_classes(info):
    import pprint
    pprint.pprint(info)
    from prettytable import PrettyTable
    x = PrettyTable()
    x.align = 'r'
    x.float_format = '0.2'
    for col in ['Model Cost', 'Data Cost', 'Total Cost']:
        x.custom_format[col] = lambda f, v: f"{v:,.2f}"
    x.custom_format['Size'] = lambda f, v: f"{v:,.1f}"
    x.field_names = [
        'ID', 'Parent', 'Type', 'Size', 'Age',
        'Model Cost', 'Data Cost', 'Total Cost', 'Factor', 'Tiny'
    ]
    for cls in info:
        x.add_row([
            cls['id'], cls['parent'], cls['type'], cls['size'], cls['age'],
            cls['costs']['model'], cls['costs']['data'], cls['costs']['total'], cls['factor'], cls['tiny']
        ])
    print(x)


