from __future__ import annotations

import ctypes as ct
import json
import os
import struct
import time
import uuid
from enum import IntFlag, auto
from pathlib import Path
from typing import Dict, Literal, Any, List, Sequence

from numpy.typing import NDArray
import numpy as np
import pandas as pd

# Load the shared library
lib_path = os.path.join(os.path.dirname(__file__), '_snob.so')
lib = ct.CDLL(lib_path)

LOG_LEVEL = 1


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
lib.load_model.restype = Classification
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

# select_sample
lib.select_sample.argtypes = [ct.c_char_p]
lib.select_population.argtypes = [ct.c_char_p]

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


class CategoryEncoder:
    """
    Keep a sorted list of states for converting categories to integers
    """
    states: Sequence[Any]

    def __init__(self, states: Sequence[Any] = ()):
        self.states = sorted(states)

    def set_states(self, states: Sequence[Any]):
        self.states = sorted(states)

    def __call__(self, state: Any) -> int:
        try:
            return self.states.index(state) + 1
        except ValueError:
            return -1


MAX_CATEGORIES = 20
CONVERTERS = {
    'real': float,
    'binary': int,
    'degrees': float,
    'radians': float,
    'multi-state': int,
}


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
        'multi-state': 'i',
        'binary': 'i',
        'degrees': 'd',
        'radians': 'd'
    }

    summary: Classification | None
    classes_: list      # list of class information after fitting
    num_records: int    # Number of records for fitted data
    num_features: int   # Number of features
    has_fit: bool       # Whether the Classifier has been fitted

    def __init__(
            self,
            attrs: Dict[str, DataType],
            cycles: int = 20,
            steps: int = 50,
            moves: int = 2,
            tol: float = 5e-2,
            name: str = 'mml',
            seed: int = 0,
            from_file: str | Path | None = None,
    ):
        """
        :param attrs: a dictionary mapping attribute  names to attribute types
        :param cycles: Maximum number of cost-assign-adjust-move cycles
        :param steps: number of steps of cost-assign-adjust
        :param moves: maximum number of failed attempts to move classes
        :param tol:  Convergence tolerance. Stops trying if percentage drop in message costs is less than this
        :param name: Internal Name of classifier, default "mml"
        :param seed:  Random number seed, 0 implies no seed.
        :param from_file: File name of saved model to load
        """

        self.has_fit = False
        self.file_pending = False
        self.from_file = Path(from_file) if from_file is not None else None
        self.attrs = attrs
        self.columns = list(self.attrs.keys())
        self.cycles = cycles
        self.steps = steps
        self.moves = moves
        self.tol = tol
        self.name = name
        self.classes_ = []
        self.num_records = 0
        self.num_features = len(attrs)
        self.seed = seed
        self.summary = None
        self.encoder = {}
        self.format = ''.join(
            self.TypeFormat[type_] for field, type_ in self.attrs.items()
        )

        # initialize encoders
        for name, type_ in self.attrs.items():
            if type_ in ['binary', 'multi-state']:
                self.encoder[name] = CategoryEncoder()
            else:
                self.encoder[name] = float

        if self.from_file is not None and self.from_file.exists():
            self.has_fit = True
            self.file_pending = True

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
        for i, (name, type_) in enumerate(self.attrs.items()):
            if type_ in ['multi-state', 'binary']:
                unique_values = data[name].dropna().unique()[:MAX_CATEGORIES]
                self.encoder[name].set_states(unique_values)
                aux = len(unique_values)
                if aux == 2:
                    # Force use of Binary for 2-valued states
                    type_ = 'binary'
                    self.attrs[name] = 'binary'
                elif aux > 20:
                    # Limit auto to 20, remaining values will be marked as missing
                    aux = 20
            else:
                aux = 0
            lib.add_attribute(i, str(name).encode('utf-8'), self.TypeValue[type_], aux)

    def add_data(self, data: pd.DataFrame, name: str = 'sample') -> int:
        """
        Create a new sample and load the data
        :param data: Pandas data frame containing the data
        :param name: name of dataset, default "sample"
        :return: the number of cases added
        """
        units = np.array([
            1 if type_ == 'degrees' else 0
            for name, type_ in self.attrs.items()
        ], dtype='int64')
        precs = np.array([
            0.0 if type_ in ['multi-state', 'binary'] else self.get_precision(data[name])
            for name, type_ in self.attrs.items()
        ], dtype='float64')

        size = len(data.index)
        lib.create_sample(
            name.encode('utf-8'),
            size,
            units.ctypes.data_as(ct.POINTER(ct.c_int)),
            precs.ctypes.data_as(ct.POINTER(ct.c_double))
        )

        # Now add records
        for i, row in data[self.columns].iterrows():
            row_values = [
                self.encoder[col](row[col])
                for col in self.columns
            ]
            bytestring = struct.pack('=' + self.format, *row_values)
            lib.add_record(i, bytestring)

        # sort the samples
        lib.sort_current_sample()
        lib.show_smpl_names()
        lib.report_space(1)
        lib.peek_data()
        return size

    def fit(self, data: pd.DataFrame | NDArray, y: None = None) -> SNOBClassifier:
        """
        Build and Fit the model
        :param data: Pandas data frame or Numpy NDArray. Columns are features, and rows are samples
        :param y: Ignored, This parameter exists only for compatibility with Pipeline.
        :return: Returns a reference to itself
        """
        if isinstance(data, np.ndarray):
            data = pd.DataFrame({field: data[:,i] for i, field in enumerate(self.columns)})

        with Timer():
            initialize(log_level=LOG_LEVEL, seed=self.seed)
            self.add_vset(data)
            self.num_records = self.add_data(data, name=self.name)
            result = lib.classify(self.cycles, self.steps, self.moves, self.tol)
            self.summary = result
            buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
            buffer = ct.create_string_buffer(buffer_size)

            # parse JSON classification result
            lib.get_class_details(buffer, buffer_size)
            self.classes_ = json.loads(buffer.value.decode('utf-8'))
            self.has_fit = True
            return self

    def get_classes(self) -> list:
        """
        Get the classification details for all classes
        :return: list of dictionaries representing classification details per class
        """
        return self.classes_

    def score(self) -> float:
        """
        Get the score of the fitted model as the total message length in bits.
        Note that smaller message lengths are better.
        :return: score
        """
        if self.summary is None:
            return 0.0
        return self.summary.message_length

    def save_model(self, filename: str | Path):
        """
        Save the model to a file
        :param filename: path to model file
        """
        if self.has_fit:
            lib.save_model(str(filename).encode('utf-8'))
        else:
            print("No fitted model to save!")

    @staticmethod
    def fetch_classification(summary: Classification) -> list:
        """
        Get the classification details for all classes
        :param summary: Classification summary
        :return: list of dictionaries representing classification details per class
        """

        buffer_size = (summary.classes + summary.leaves) * (summary.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)
        lib.get_class_details(buffer, buffer_size)
        return json.loads(buffer.value.decode('utf-8'))

    @staticmethod
    def fetch_assignments(size: int) -> pd.DataFrame:
        """
        Get the assignments for all records
        :param size: Number of records
        :return: data Frame
        """

        ids = (ct.c_int * size)()
        prim_cls = (ct.c_int * size)()
        prim_probs = (ct.c_double * size)()
        sec_cls = (ct.c_int * size)()
        sec_probs = (ct.c_double * size)()
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

    def predict(self, data: pd.DataFrame | NDArray | None = None, name: str | None = None) -> pd.DataFrame:
        """
        Assign classes to records in the provided dataset.
        :param data: Data frame similar to fitted data frame, if not provided, returns assignments
            for fitted data
        :param name: Name of dataset
        :return: pandas data Frame with columns
            [index, class-id, probability, next_class, next_class_probability] corresponding
            to the top-two class assignments for each record
        """
        if isinstance(data, np.ndarray):
            data = pd.DataFrame({field: data[:,i] for i, field in enumerate(self.columns)})

        sample_name = str(uuid.uuid4())[:8] if name is None else name
        if not self.has_fit:
            print("No fitted model!")
            return pd.DataFrame()
        elif self.file_pending and data is not None:
            self.file_pending = False
            initialize(log_level=LOG_LEVEL, seed=self.seed)
            set_control_flags(Adjust.SCORES)
            self.add_vset(data)
            self.num_records = self.add_data(data, name=sample_name)
            self.summary = lib.load_model(str(self.from_file).encode('utf-8'))
            self.classes_ = self.fetch_classification(self.summary)
            self.has_fit = True
            size = self.num_records
        elif data is not None:
            set_control_flags(Adjust.SCORES)
            with SnobContextManager():
                size = self.add_data(data, name=sample_name)
            select_sample(sample_name)
        else:
            size = self.num_records

        return self.fetch_assignments(size)


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


def select_sample(name: str):
    """
    Select the sample by name
    :param name: Sample Name
    """
    lib.select_sample(name.encode('utf-8'))


def select_population(name: str):
    """
    Select the population by name
    :param name: Population Name
    """
    lib.select_population(name.encode('utf-8'))


def classify(
        vset_file: str | Path,
        sample_file: str | Path,
        cycles: int = 3,
        steps: int = 50,
        moves: int = 3,
        tol: float = 1e-2
):
    """
    Run a classification based on vset and sample files like original SNOB
    :param vset_file: VSET File
    :param sample_file: Sample File
    :param cycles:  Number of classification cycles
    :param steps: Number of do_all steps
    :param moves: Number of try_move steps
    :param tol:  percentage drop above which we should continue trying
    :return: list of class dictionaries
    """
    with Timer():
        initialize(log_level=LOG_LEVEL)
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

        text += f'{prefix}{elbow}── {node}  \n'
        text += ascii_tree(sub_dict, f'{prefix}{stem}   ')
    return text


def show_classes(info):
    from prettytable import PrettyTable

    x = PrettyTable()
    x.field_names = [
        'ID', 'Tree', 'Size', 'Age',
        'Model Cost', 'Data Cost', 'Total Cost', 'Factor'
    ]

    x.align['Tree'] = 'l'
    x.float_format = '0.2'
    x.custom_format['Size'] = lambda f, v: f"{v:,.1f}"
    x.align['Size'] = 'r'
    for col in ['Model Cost', 'Data Cost', 'Total Cost']:
        x.custom_format[col] = lambda f, v: f"{v:,.2f}"
        x.align[col] = 'r'

    # Build tree information
    tree_dict = build_tree(-1, info)
    tree_text = ascii_tree(tree_dict, root=True)
    tree_data = tree_text.split('\n')

    for i, cls in enumerate(info):
        x.add_row([
            cls['id'], tree_data[i], cls['size'], cls['age'],
            cls['costs']['model'], cls['costs']['data'], cls['costs']['total'],
            '*' if cls['factor'] else ' ',
        ])
    print(x)


