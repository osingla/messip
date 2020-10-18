#!/usr/bin/env python3
# encoding: utf-8

from distutils.core import setup, Extension
from distutils.unixccompiler import UnixCCompiler

messip_module = Extension(
    'messip', 
    sources = ['messipmodule.c']
)

setup(name='messip',
      version='0.9.0',
      description='Messip module written in C',
      ext_modules=[messip_module])
