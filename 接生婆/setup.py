from setuptools import setup, Extension
import pybind11
import sys

args = ['-O3', '-std=c++17']
if sys.platform == 'darwin':
    args += ['-stdlib=libc++', '-mmacosx-version-min=10.14']

ext = Extension(
    'taiji_core',
    sources=[
        'python/py_binding.cpp',
        'core/clps_value.cpp',
        'core/taiji_jiejie.cpp',
    ],
    include_dirs=[
        pybind11.get_include(),
        '.',   # 让 python/ 中的 #include "../core/..." 可以找到 core/
    ],
    extra_compile_args=args,
    language='c++'
)

setup(name='taiji_core', ext_modules=[ext])
