""" Generic routines, useful for all data """

import sys

# Modules to be imported independent of version
for_all_versions = [\
                    'calculus',\
                    'closest_line',\
                    'datafile',\
                    # 'efit_analyzer',\ # bunch pkg required
                    'fft_deriv',\
                    'fft_integrate',\
                    'file_import',\
                    'getmpirun',\
                    'int_func',\
                    'launch',\
                    'linear_regression',\
                    'mode_structure',\
                    # 'moment_xyzt',\   # bunch pkg requried
                    'ncpus',\
                    'shell',\
                    'showdata',\
                    # 'surface_average',\
                    # 'volume_integral',\ #bunch pkg required
                    ]

# Check the current python version
if sys.version_info[0]>=3:
    do_import = for_all_versions
    __all__ = do_import
else:
    do_import = for_all_versions
    do_import.append('anim')
    do_import.append('plotpolslice')
    do_import.append('View3D')
    __all__ = do_import
