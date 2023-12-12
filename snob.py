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
snob.get_class_details.argtypes = [ct.c_char_p, ct.c_size_t]

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
EXAMPLES = ['6r1b']

from pathlib import Path
                
if __name__ == '__main__':
    snob.initialize(0, 0, 16)
    if len(sys.argv) > 1:
        EXAMPLES = sys.argv[1:]

    for name in EXAMPLES:
        vset_file = Path('./examples') / f'{name}.v'
        sample_file = Path('./examples') / f'{name}.s'
    
        if not vset_file.exists() and sample_file.exists():
            continue

        print('#'*80)
        snob.report_space(1)
        print(f"Classifying: {name}")
        
        snob.load_vset(str(vset_file).encode('utf-8'))
        snob.load_sample(str(sample_file).encode('utf-8'))
        result = snob.classify(20, 50, 2, 0.01)
        snob.print_tree()
        buffer_size = (result.classes + result.leaves) * (result.attrs + 1) * 80 * 4
        buffer = ct.create_string_buffer(buffer_size)
        snob.get_class_details(buffer, buffer_size)
        print(buffer.value.decode('utf-8'))
        info = json.loads(buffer.value.decode('utf-8'))
        print(json.dumps(info, indent=4))
        snob.show_population()
        snob.reset()
    
