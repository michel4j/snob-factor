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
snob.initialize.argtypes = [ct.c_int]
snob.load_vset.argtypes = [ct.c_char_p]
snob.load_vset.restype = ct.c_int
snob.load_sample.argtypes = [ct.c_char_p]
snob.load_sample.restype = ct.c_int
snob.classify.argtypes = [ct.c_int, ct.c_int, ct.c_int, ct.c_double]
snob.classify.restype = Classification
snob.print_class.argtypes = [ct.c_int, ct.c_int]
snob.item_list.argtypes = [ct.c_char_p]



                
if __name__ == '__main__':
    snob.initialize(1, 1)

    snob.load_vset(b'./examples/phi.v')
    snob.load_sample(b'./examples/phi.s')

    result = snob.classify(20, 50, 2, 0.01)
       
    snob.print_tree()
    #snob.print_class(-2, 1)
    #snob.show_population()
    

