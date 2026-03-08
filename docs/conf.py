import os
import sys
sys.path.insert(0, os.path.abspath('..'))

project = 'Factor Snob'
copyright = '2008, Chris Wallace, Michel Fodje'
author = 'Chris Wallace, Michel Fodje'

extensions = [
    'myst_parser',
    'sphinx.ext.autodoc',
    'sphinx.ext.napoleon',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

myst_enable_extensions = [
    "colon_fence",
]
