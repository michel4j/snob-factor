#!/usr/bin/env python3

import ctypes as ct

class Classification(ct.Structure):
    """ Result """

    _fields_ = [
        ('num_classes', ct.c_int),
        ('num_leaves', ct.c_int),
        ('model_length', ct.c_double),
        ('data_length', ct.c_double),
        ('message_length', ct.c_double)
    ] 
    
snob = ct.cdll.LoadLibrary('./libsnob.so')
snob.initialize.argtypes = [ct.c_int, ct.c_int, ct.c_int]
snob.load_vset.argtypes = [ct.c_char_p]
snob.load_vset.restype = ct.c_int
snob.load_sample.argtypes = [ct.c_char_p]
snob.load_sample.restype = ct.c_int
snob.classify.argtypes = [ct.c_int, ct.c_int, ct.c_int, ct.c_double]
snob.classify.restype = Classification
snob.print_class.argtypes = [ct.c_int, ct.c_int]
snob.item_list.argtypes = [ct.c_char_p]


EXAMPLES = [
    '5m1c',
    '5r8c',
    '6m1c',
    '6m2r2c',
    '6r1c',
    '6r1b',
    'd2',
    'phi',
    'sd1',
    'vm',
]
EXAMPLES = ['6r1b']

from pathlib import Path
                
if __name__ == '__main__':
    for name in EXAMPLES:
        vset_file = str(Path('./examples') / f'{name}.v')
        sample_file = str(Path('./examples') / f'{name}.s')
    
        print(f"Classifying: {name}")
        
        snob.initialize(1, 0, 16)
        snob.load_vset(vset_file.encode('utf-8'))
        snob.load_sample(sample_file.encode('utf-8'))

        result = snob.classify(20, 50, 2, 0)
        
        snob.print_tree()
        snob.print_class(-2, 1)
        snob.show_population()
    

