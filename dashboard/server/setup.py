from setuptools import setup, Extension, find_packages
from Cython.Build import cythonize
import os
import numpy as np

# Get relative path to the C++ implementation
script_dir = os.path.dirname(os.path.abspath(__file__))
repo_root = os.path.abspath(os.path.join(script_dir, '..', '..'))
cpp_dir = os.path.join(repo_root, 'components', 'common', 'lib')
proto_dir = os.path.join(repo_root, 'protocol', 'generated', 'nanopb')

# Define the extension module
extensions = [
    Extension(
        "lib.can._can_interface",
        sources=[
            "lib/can/_can_interface.pyx",
            os.path.join(cpp_dir, "ProtobufCANInterface/src/ProtobufCANInterface.cpp"),
        ],
        include_dirs=[
            os.path.join(cpp_dir, "ProtobufCANInterface/src"),
            os.path.join(proto_dir),
            np.get_include(),  # For numpy support if needed
        ],
        language="c++",
        extra_compile_args=["-std=c++11", "-O3"],
    )
]

setup(
    name="go_kart_platform",
    version="0.1.0",
    description="Go Kart Platform Server",
    author="Go Kart Team",
    packages=find_packages(),
    ext_modules=cythonize(extensions),
    zip_safe=False,
    install_requires=[
        'flask',
        'flask-cors',
        'flask-socketio',
        'python-can',
        'numpy',
        'cython',
        'protobuf',
    ],
) 