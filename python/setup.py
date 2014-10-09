#!/usr/bin/python
from distutils.core import setup, Extension

VERSION = "1.1"

setup(name = "python-nscang",
      version = VERSION,
      description = "Python NSCA-ng client",
      author = "Alexander Golovko",
      author_email = "alexandro@onsec.ru",
      license = "BSD",
      long_description = \
"""
Python NSCA-ng client.
""",
      ext_modules = [Extension("nscang",
                               ["nscang.c", "client.c"],
                               libraries = ["pthread", "ssl", "crypto"],
                               define_macros = [("VERSION", '"%s"' % VERSION)])],
      )
