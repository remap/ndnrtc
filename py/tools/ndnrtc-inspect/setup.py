"""Packaging settings."""


from codecs import open
from os.path import abspath, dirname, join
from subprocess import call

from setuptools import Command, find_packages, setup

from ndnrtc_inspect import __version__


this_dir = abspath(dirname(__file__))
with open(join(this_dir, 'README.md'), encoding='utf-8') as file:
    try:
        import pypandoc
        long_description = pypandoc.convert_text(file.read(), 'rst', format='md')
    except (IOError, ImportError):
        long_description = ''

setup(
    name = 'ndnrtc-inspect',
    version = __version__,
    description = 'A Python app for fetching various NDN-RTC stream data from the network and presenting it in a friendly way.',
    long_description = long_description,
    url = 'https://github.com/remap/ndnrtc/tree/master/py/tools/ndnrtc-inspect',
    author = 'Peter Gusev',
    author_email = 'peter@remap.ucla.edu',
    license = 'UNLICENSED',
    classifiers = [
        'Intended Audience :: Developers',
        'Topic :: Utilities',
        'License :: Public Domain',
        'Natural Language :: English',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.2',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4',
    ],
    keywords = 'NDN ndnrtc ndnrtc-stream ndnrtc-inspect',
    packages = find_packages(exclude=['docs', 'tests*']),
    install_requires = ['docopt', 'docopt', 'termcolor', 'cryptography', 'protobuf'],
    dependency_links = [
        'https://github.com/named-data/PyNDN2/tarball/master',
        'https://github.com/named-data/PyCNL/tarball/master'
    ],
    #extras_require = {
    #    'test': ['coverage', 'pytest', 'pytest-cov'],
    #},
    entry_points = {
        'console_scripts': [
            'ndnrtc-inspect=ndnrtc_inspect.cli:main',
        ],
    },
    #cmdclass = {'test': RunTests},
)
