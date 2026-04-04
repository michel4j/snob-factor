from __future__ import annotations

from os import PathLike
import ctypes as ct
import json
import os
import struct
import atexit
import time
import uuid
from enum import IntFlag, auto
from pathlib import Path
from typing import Dict, Literal, Any, Sequence
from importlib.metadata import version, PackageNotFoundError

from numpy.typing import NDArray
import numpy as np
import pandas as pd

__version__ = "0.0.0"

# Load the shared library
lib_path = os.path.join(os.path.dirname(__file__), "_snob.so")
lib = ct.CDLL(lib_path)


class SnobContext(ct.Structure):
    pass


SnobContextPtr = ct.POINTER(SnobContext)


class Classification(ct.Structure):
    """Result"""

    _fields_ = [
        ("classes", ct.c_int),
        ("leaves", ct.c_int),
        ("attrs", ct.c_int),
        ("cases", ct.c_int),
        ("model", ct.c_double),
        ("data", ct.c_double),
        ("message", ct.c_double),
    ]

class Attr(ct.Structure):
    """Attribute"""
    _fields_ = [
        ("name", ct.c_char * 80),
        ("index", ct.c_int),
        ("type", ct.c_int),
        ("aux", ct.c_int),
    ]

    def to_dict(self):
        return {
            "name": self.name.decode("utf-8").strip(),
            "type": self.type,
            "aux": self.aux,
        }


lib.initialize.argtypes = [ct.c_int, ct.c_int, ct.c_int]
lib.initialize.restype = SnobContextPtr
lib.destroy_context.argtypes = [SnobContextPtr]
lib.destroy_context.restype = None
lib.init_population.restype = ct.c_int
lib.load_vset.argtypes = [SnobContextPtr, ct.c_char_p]
lib.load_vset.restype = ct.c_int
lib.load_sample.argtypes = [SnobContextPtr, ct.c_char_p]
lib.load_sample.restype = ct.c_int
lib.classify.argtypes = [SnobContextPtr, ct.c_int, ct.c_int, ct.c_int, ct.c_double]
lib.classify.restype = Classification
lib.get_classification.argtypes = [SnobContextPtr]
lib.get_classification.restype = Classification
lib.print_class.argtypes = [SnobContextPtr, ct.c_int, ct.c_int]
lib.item_list.argtypes = [SnobContextPtr, ct.c_char_p]
lib.get_assignments.restype = ct.c_int
lib.get_assignments.argtypes = [
    SnobContextPtr,
    ct.POINTER(ct.c_int),
    ct.POINTER(ct.c_int),
    ct.POINTER(ct.c_double),
    ct.POINTER(ct.c_int),
    ct.POINTER(ct.c_double),
]
lib.get_model_details.argtypes = [SnobContextPtr, ct.c_char_p, ct.c_size_t]
lib.get_class_details.argtypes = [SnobContextPtr, ct.c_char_p, ct.c_size_t]
lib.get_num_vars.argtypes = [SnobContextPtr]
lib.get_num_vars.restype = ct.c_int
lib.save_model.argtypes = [SnobContextPtr, ct.c_char_p]
lib.save_model.restype = ct.c_int
lib.load_model.argtypes = [SnobContextPtr, ct.c_char_p]
lib.load_model.restype = ct.c_int
lib.set_control_flags.argtypes = [SnobContextPtr, ct.c_int]
lib.set_control_flags.restype = ct.c_int
# create_vset
lib.create_vset.argtypes = [SnobContextPtr, ct.c_char_p, ct.c_int]
lib.create_vset.restype = ct.c_int

# set_attribute
lib.add_attribute.argtypes = [SnobContextPtr, ct.c_int, ct.c_char_p, ct.c_int, ct.c_int]
lib.add_attribute.restype = ct.c_int

# create_sample
lib.create_sample.argtypes = [
    SnobContextPtr,
    ct.c_char_p,
    ct.c_int,
    ct.POINTER(ct.c_int),
    ct.POINTER(ct.c_double),
]
lib.create_sample.restype = ct.c_int

# add_record
lib.add_record.argtypes = [SnobContextPtr, ct.c_int, ct.c_char_p]
lib.add_record.restype = ct.c_int

# select_sample
lib.select_sample.argtypes = [SnobContextPtr, ct.c_char_p]
lib.select_population.argtypes = [SnobContextPtr, ct.c_char_p]

# print_data
lib.print_var_datum.argtypes = [SnobContextPtr, ct.c_int, ct.c_int]

DataType = Literal["real", "multi-state", "binary", "degrees", "radians"]


class SnobContextManager:
    def __init__(self, ctx):
        self.ctx = ctx

    def __enter__(self):
        lib.save_context(self.ctx)
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        lib.restore_context(self.ctx)


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


class Encoder:
    """
    Base class for encoders
    """

    def __call__(self, value: Any) -> Any:
        raise NotImplementedError

    def set_states(self, states: Sequence[Any]):
        pass


class SimpleEncoder(Encoder):
    """
    A simple encoder that returns the value as a given type
    """

    type_: type

    def __init__(self, type_):
        self.type_ = type_

    def __call__(self, value: Any) -> Any:
        return self.type_(value)


class CategoryEncoder(Encoder):
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
    "real": float,
    "binary": int,
    "degrees": float,
    "radians": float,
    "multi-state": int,
    "gaussian": float,
    "von-mises-fisher": float,
}

AttrType = {
    1: "gaussian",
    2: "multi-state",
    3: "binary",
    4: "von-mises-fisher",
}

class SNOBClassifier:
    TypeValue = {
        "real": 1,
        "gaussian": 1,
        "multi-state": 2,
        "binary": 3,
        "von-mises-fisher": 4,
        "degrees": 4,
        "radians": 4,
    }
    TypeFormat = {
        "real": "d",
        "gaussian": "d",
        "von-mises-fisher": "d",
        "multi-state": "i",
        "binary": "i",
        "degrees": "d",
        "radians": "d",
    }
    attrs: Dict[str, DataType]  # a dictionary mapping attribute  names to attribute types
    columns: list[str]  # list of attribute names
    summary: Classification | None
    format: str  # packed format string for attribute value types
    classes_: list  # list of class information after fitting
    num_records: int  # Number of records for fitted data
    num_features: int  # Number of features
    has_fit: bool  # Whether the Classifier has been fitted

    def __init__(
        self,
        attrs: dict[str, DataType] = None,
        models: list[dict] = None,
        cycles: int = 25,
        steps: int = 50,
        moves: int = 4,
        tol: float = 5e-3,
        name: str = "mml",
        seed: int = 0,
        verbose: bool = False,
        from_file: PathLike = None,
    ):
        """
        :param attrs: a dictionary mapping attribute  names to attribute types optional if models is provided
        :param models: a list of dictionaries, each containing the keys "name", "attrs", "epsilon", etc.
        :param cycles: Maximum number of cost-assign-adjust-move cycles
        :param steps: number of steps of cost-assign-adjust
        :param moves: maximum number of failed attempts to move classes
        :param tol:  Convergence tolerance. Stops trying if percentage drop in message costs is less than this
        :param name: Internal Name of classifier, default "mml"
        :param seed:  Random number seed, 0 implies no seed.
        :param verbose: Whether to print log messages, default False
        :param from_file: File name of saved model to load
        """

        self.ctx: SnobContextPtr = 0
        self.has_fit = False
        self.from_file: PathLike | None = Path(from_file) if from_file else None
        self.fit_pending = False

        self.cycles = cycles
        self.steps = steps
        self.moves = moves
        self.tol = tol
        self.name = name
        self.classes_ = []
        self.num_records = 0

        self.seed = seed
        self.verbose = verbose
        self.summary = None
        self.encoder: Dict[str, Encoder] = {}
        self.data: pd.DataFrame | None = None
        self.epsilons = {}
        self.units = {}

        if models:
            attrs = {
                attr: model['name'] for model in models for attr in model['attrs']
            }
            self.epsilons = {
                attr: model['epsilon'] for model in models for attr in model['attrs'] if 'epsilon' in model
            }
            self.units = {
                attr: model['units'] for model in models for attr in model['attrs'] if 'units' in model
            }

        if attrs is not None:
            self._update_attrs(attrs)
        elif from_file:
            self.fit_pending = True
            name, attrs = read_model_attributes(from_file)
            self.name = name
            self._update_attrs(attrs)


    def __str__(self):
        return f"SNOBClassifier({self.name!r}, attrs={self.attrs!r})"

    def __repr__(self):
        return str(self)

    def _update_attrs(self, attrs):
        self.attrs = attrs
        self.columns = list(self.attrs.keys())
        self.num_features = len(attrs)
        self.format = "".join(
            self.TypeFormat[type_] for field, type_ in self.attrs.items()
        )   
        # initialize encoders
        for name, type_ in self.attrs.items():
            if type_ in ["binary", "multi-state"]:
                self.encoder[name] = CategoryEncoder()
            else:
                self.encoder[name] = SimpleEncoder(float)
            if type_ in ['degrees', 'radians']:
                self.units[name] = type_
                self.attrs[name] = 'von-mises-fisher'
        
    def get_precision(col) -> float:
        """
        Given an array of floats, return the estimated precision of the values
        :param col: array
        :return: integer
        """
        if col in self.epsilons:
            return self.epsilons[col]

        err = 1e-6
        prec = 1
        while prec < 6 and (col - col.round(prec)).abs().mean() * 10**prec > err:
            prec += 1
        return 10**-prec

    def add_vset(self, data: pd.DataFrame):
        """
        Create a new vset for this data
        :param data: Pandas data frame containing the data
        """

        if self.ctx == 0:
            self.ctx = initialize(0 if self.verbose else 1, self.seed)

        lib.create_vset(self.ctx, self.name.encode("utf-8"), len(self.attrs))
        # Add attributes
        for i, (name, type_) in enumerate(self.attrs.items()):
            if type_ in ["multi-state", "binary"]:
                unique_values = data[name].dropna().unique()[:MAX_CATEGORIES]
                self.encoder[name].set_states(unique_values)
                aux = len(unique_values)
                if aux == 2:
                    # Force use of Binary for 2-valued states
                    type_ = "binary"
                    self.attrs[name] = "binary"
                elif aux > 20:
                    # Limit auto to 20, remaining values will be marked as missing
                    aux = 20
            else:
                aux = 0
            lib.add_attribute(
                self.ctx, i, str(name).encode("utf-8"), self.TypeValue[type_], aux
            )

    def add_data(self, data: pd.DataFrame, name: str = "sample") -> int:
        """
        Create a new sample and load the data
        :param data: Pandas data frame containing the data
        :param name: name of dataset, default "sample"
        :return: the number of cases added
        """
        units = np.array(
            [1 if self.units.get(name, "radians") == "degrees" else 0 for name in self.columns],
            dtype="int32",
        )
        precs = np.array(
            [
                0.0
                if type_ in ["multi-state", "binary"]
                else self.get_precision(data[name])
                for name, type_ in self.attrs.items()
            ],
            dtype="float64",
        )
        size = len(data.index)
        lib.create_sample(
            self.ctx,
            name.encode("utf-8"),
            size,
            units.ctypes.data_as(ct.POINTER(ct.c_int32)),
            precs.ctypes.data_as(ct.POINTER(ct.c_double)),
        )

        # Now add records
        for i, row in data[self.columns].iterrows():
            row_values = [self.encoder[col](row[col]) for col in self.columns]
            bytestring = struct.pack("=" + self.format, *row_values)
            lib.add_record(self.ctx, i, bytestring)

        # sort the samples
        lib.sort_current_sample(self.ctx)
        lib.show_smpl_names(self.ctx)
        lib.peek_data(self.ctx)
        return size

    def fit(self, data: pd.DataFrame | NDArray, y: None = None) -> SNOBClassifier:
        """
        Build and Fit the model
        :param data: Pandas data frame or Numpy NDArray. Columns are features, and rows are samples
        :param y: Ignored, This parameter exists only for compatibility with Pipeline.
        :return: Returns a reference to itself
        """
        if isinstance(data, np.ndarray):
            data = pd.DataFrame(
                {field: data[:, i] for i, field in enumerate(self.columns)}
            )

        # check that data has same columns as model
        if not set(self.columns).issubset(data.columns):
            missing_cols = set(self.columns) - set(data.columns)
            raise ValueError(f"Model attributes {missing_cols} are not in the data.")
        
        with Timer():
            self.ctx = initialize(0 if self.verbose else 1, self.seed)
            self.data = data
            self.add_vset(self.data)
            self.num_records = self.add_data(self.data, name=self.name)
            result = lib.classify(
                self.ctx, self.cycles, self.steps, self.moves, self.tol
            )
            self.summary = result
            buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
            buffer = ct.create_string_buffer(buffer_size)

            # parse JSON classification result
            lib.get_class_details(self.ctx, buffer, buffer_size)
            self.classes_ = json.loads(buffer.value.decode("utf-8"))
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
            lib.save_model(self.ctx, str(filename).encode("utf-8"))
        else:
            print("No fitted model to save!")

    def fetch_classification(self, summary: Classification) -> list:
        """
        Get the classification details for all classes
        :param summary: Classification summary
        :return: list of dictionaries representing classification details per class
        """

        buffer_size = (summary.classes + summary.leaves) * (summary.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)
        lib.get_class_details(self.ctx, buffer, buffer_size)
        return json.loads(buffer.value.decode("utf-8"))

    def fetch_assignments(self, size: int) -> pd.DataFrame:
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
        lib.get_assignments(self.ctx, ids, prim_cls, prim_probs, sec_cls, sec_probs)

        # Create a Pandas DataFrame
        df = pd.DataFrame(
            {
                "item": np.ctypeslib.as_array(ids),
                "major_class": np.ctypeslib.as_array(prim_cls),
                "major_prob": np.ctypeslib.as_array(prim_probs),
                "minor_class": np.ctypeslib.as_array(sec_cls),
                "minor_prob": np.ctypeslib.as_array(sec_probs),
            }
        )
        return df

    def predict(
        self, data: pd.DataFrame | NDArray | None = None, name: str | None = None
    ) -> pd.DataFrame:
        """
        Assign classes to records in the provided dataset.
        :param data: Data frame similar to fitted data frame, if not provided, returns assignments
            for fitted data
        :param name: Name of dataset
        :return: pandas data Frame with columns
            [index, major_class, major_prob, minor_class, minor_prob] corresponding
            to the top-two class assignments for each record
        """
        if isinstance(data, np.ndarray):
            data = pd.DataFrame(
                {field: data[:, i] for i, field in enumerate(self.columns)}
            )
        
        if not set(self.columns).issubset(data.columns):
            missing_cols = set(self.columns) - set(data.columns)
            raise ValueError(f"Model attributes {missing_cols} are not in the data.")

        sample_name = str(uuid.uuid4())[:8] if name is None else name
        if not (self.has_fit or self.fit_pending):
            print("No fitted model!")
            return pd.DataFrame()
        elif self.fit_pending and data is not None:
            self.fit_pending = False
            self.ctx = lib.initialize(0, 0 if self.verbose else 1, self.seed)
            set_control_flags(self.ctx, Adjust.SCORES)
            self.add_vset(data)
            self.num_records = self.add_data(data, name=sample_name)
            file_name = str(self.from_file).encode("utf-8")
            lib.load_model(self.ctx, file_name)
            self.summary = lib.get_classification(self.ctx)
            self.classes_ = self.fetch_classification(self.summary)
            self.has_fit = True
            size = self.num_records
        elif data is not None:
            set_control_flags(self.ctx, Adjust.SCORES)
            with SnobContextManager(self.ctx):
                size = self.add_data(data, name=sample_name)
            select_sample(self.ctx, sample_name)
        else:
            size = self.num_records

        assignments = self.fetch_assignments(size)
        data = data if data is not None else self.data
        new_data = data.copy()
        for column in ["major_class", "major_prob", "minor_class", "minor_prob"]:
            new_data.loc[assignments['item'].values, column] = assignments[column].values
            if column in ["major_class", "minor_class"]:
                new_data[column] = new_data[column].astype(int)
        return new_data
    
    def load_model(self, path: PathLike) -> bool:
        """
        Load the model from a file
        :param path: path to model file
        :return: True if successful, False otherwise
        """

        if not Path(path).exists():
            print("Model file does not exist!")
            return False
        
        # Initialize SNOB and read the model form file
        self.ctx = initialize(0 if self.verbose else 1, self.seed)
        set_control_flags(self.ctx, Adjust.SCORES)
        file_name = str(path).encode("utf-8")
        lib.load_model(self.ctx, file_name)

        # Get model name and attributes
        inverse_typevar = {code: label for label, code in self.TypeValue.items()}
        num_attrs = lib.get_num_vars(self.ctx)
        print(num_attrs)
        buffer_size = num_attrs * 1024
        buffer = ct.create_string_buffer(buffer_size)
        model_info = lib.get_model_details(self.ctx, buffer, buffer_size)
        self.name = model_info['name']
        attrs = {
            var_name: inverse_typevar.get(var_type, 0)
            for var_name, var_type in json.loads(buffer.value.decode("utf-8")).items()
        }
        self._update_attrs(attrs)
        self.fit_pending = True
        return True


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
    ctx = lib.initialize(0, log_level, seed)
    atexit.register(destroy_context, ctx)
    return ctx


def destroy_context(ctx):
    """
    Destroy SNOB system
    :param ctx: SNOB context
    """
    lib.destroy_context(ctx)


def set_control_flags(ctx, flags: Adjust):
    """
    Set Adjustment flags
    :param flags: Adjust Flags
    """
    lib.set_control_flags(ctx, flags)


def select_sample(ctx, name: str):
    """
    Select the sample by name
    :param name: Sample Name
    """
    lib.select_sample(ctx, name.encode("utf-8"))


def select_population(ctx, name: str):
    """
    Select the population by name
    :param name: Population Name
    """
    lib.select_population(ctx, name.encode("utf-8"))


def classify(
    vset_file: str | Path,
    sample_file: str | Path,
    cycles: int = 3,
    steps: int = 50,
    moves: int = 3,
    seed: int = 0,
    tol: float = 1e-2,
    verbose: bool = False,
):
    """
    Run a classification based on vset and sample files like original SNOB
    :param vset_file: VSET File
    :param sample_file: Sample File
    :param cycles:  Number of classification cycles
    :param steps: Number of do_all steps
    :param moves: Number of try_move steps
    :param seed: random number seed, 0 will use system time
    :param tol:  percentage drop above which we should continue trying
    :return: list of class dictionaries
    """
    with Timer():
        ctx = initialize(log_level=0 if verbose else 1, seed=seed)
        lib.load_vset(ctx, str(vset_file).encode("utf-8"))
        lib.load_sample(ctx, str(sample_file).encode("utf-8"))
        lib.peek_data(ctx)
        result = lib.classify(ctx, cycles, steps, moves, tol)
        buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)

        # parse JSON classification result
        lib.get_class_details(ctx, buffer, buffer_size)
        return json.loads(buffer.value.decode("utf-8"))


def build_tree(node: int, info: list) -> dict:
    """
    Given a list of class dictionaries, build a recursive tree structure where the key is the class-id
    and the value is a dictionary containing child classes

    :param node: class-id of starting node
    :param info: list of class dictionaries
    :return: Nested recursive dictionary mapping class-ids to subtree dictionaries
    """
    return {
        v: build_tree(v, info) for v in [x["id"] for x in info if x["parent"] == node]
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
            elbow, stem = (" ", " ")
        else:
            elbow, stem = ("├", "│") if i < size - 1 else ("└", " ")

        text += f"{prefix}{elbow}── {node}  \n"
        text += ascii_tree(sub_dict, f"{prefix}{stem}   ")
    return text


def show_classes(info):
    from prettytable import PrettyTable

    x = PrettyTable()
    x.field_names = [
        "ID",
        "Tree",
        "Size",
        "Age",
        "Model Cost",
        "Data Cost",
        "Total Cost",
        "Factor",
    ]

    x.align["Tree"] = "l"
    x.float_format = "0.2"
    x.custom_format["Size"] = lambda f, v: f"{v:,.1f}"
    x.align["Size"] = "r"
    for col in ["Model Cost", "Data Cost", "Total Cost"]:
        x.custom_format[col] = lambda f, v: f"{v:,.2f}"
        x.align[col] = "r"

    # Build tree information
    tree_dict = build_tree(-1, info)
    tree_text = ascii_tree(tree_dict, root=True)
    tree_data = tree_text.split("\n")

    for i, cls in enumerate(info):
        x.add_row(
            [
                cls["id"],
                tree_data[i],
                cls["size"],
                cls["age"],
                cls["costs"]["model"],
                cls["costs"]["data"],
                cls["costs"]["total"],
                "*" if cls["factor"] else " ",
            ]
        )
    print(x)


def get_attr_type(attr: Attr) -> str:
    """
    Convert attribute type to string
    :param attr: attribute
    :return: string representation of attribute type
    """
    
    type_ = AttrType.get(attr.type, 'real')
    if type_ == 'vonmises' and attr.aux == 0:
        return "radians"
    elif type_ == 'vonmises' and attr.aux == 1:
        return "degrees"
    else:
        return type_

def read_model_attributes(model_file: PathLike) -> tuple[str, dict[str, str]]:
    """
    Read attributes from the model file
    :param model_file: path to the model file
    :return: tuple of (model_name, attributes)
    """

    with open(model_file, "rb") as f:
        header = f.readline()
        if header.decode("utf-8").strip() != "Snob-Model-V2":
            raise ValueError("Invalid model file")
        
        # read model name
        int_size = struct.calcsize("=i")
        name_length = struct.unpack("=i", f.read(int_size))[0]
        name = f.read(name_length).decode("utf-8").strip()
        print("Model Name", name)

        num_attrs = struct.unpack("=i", f.read(int_size))[0]
        print("Number of Attributes", num_attrs)

        raw_attrs = [
            Attr.from_buffer_copy(f.read(ct.sizeof(Attr)))
            for _ in range(num_attrs)
        ]
        attrs = {
            attr.name.decode("utf-8"): get_attr_type(attr) for attr in raw_attrs
        }
        return name, attrs

        
    
