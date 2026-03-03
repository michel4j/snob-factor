#!/usr/bin/env python3

import re
import sys

#P1     9 classes,    5 leaves,  Pcost   312.7  Tcost   25524.9,  Cost   25837.6
#P3     8 classes,    6 leaves,  Pcost   272.0  Tcost   25608.6,  Cost   25880.6
pattern = re.compile(
    r"^P\d\s+(?P<classes>\d+)\s+classes,\s+"
    r"(?P<leaves>\d+)\s+leaves,\s+Pcost\s+(?P<p_cost>\d+\.\d+)\s+"
    r"Tcost\s+(?P<t_cost>\d+\.\d+),\s+Cost\s+(?P<cost>\d+\.\d+)", re.MULTILINE
)

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) != 3:
        print("Usage: compare.py <expected> <actual>")
        sys.exit(1)
    
    expected = sys.argv[1]
    actual = sys.argv[2]
    
    with open(expected, "rt", encoding="utf-8") as f:
        expected_output = f.read()
    
    with open(actual, "rt", encoding="utf-8") as f:
        actual_output = f.read()

    expected_results = [m.groupdict() for m in pattern.finditer(expected_output)]
    actual_results = [m.groupdict() for m in pattern.finditer(actual_output)]

    final_expected = expected_results[-1]
    final_actual = actual_results[-1]

    if final_expected != final_actual:

        print(f"Classes: expected {final_expected['classes']}, actual {final_actual['classes']}")
        print(f"Leaves: expected {final_expected['leaves']}, actual {final_actual['leaves']}")
        print(f"Pcost: expected {final_expected['p_cost']}, actual {final_actual['p_cost']}")
        print(f"Tcost: expected {final_expected['t_cost']}, actual {final_actual['t_cost']}")
        print(f"Cost: expected {final_expected['cost']}, actual {final_actual['cost']}")

        success = True
        if abs(int(final_expected['classes']) - int(final_actual['classes'])) > 1:
            print("Classes do not match!")
            success = False
        if abs(int(final_expected['leaves']) - int(final_actual['leaves'])) > 0:
            print("Leaves do not match!")
            success = False
        if abs(float(final_actual['p_cost']) - float(final_expected['p_cost'])) / float(final_expected['p_cost']) > 0.05:
            print("Pcost does not match! (>5%)")
            success = False
        if abs(float(final_actual['t_cost']) - float(final_expected['t_cost'])) / float(final_expected['t_cost']) > 0.05:
            print("Tcost does not match! (>5%)")
            success = False
        if not success:
            print("FAILED: Outputs do not match!")
            sys.exit(1)
    
    print("SUCCESS: Outputs match!")
    

    