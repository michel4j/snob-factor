#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR/.."

export CIBW_CONTAINER_ENGINE=podman
python3 -m cibuildwheel . --output-dir ./dist/