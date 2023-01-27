EBU ADM Toolbox
===============

The EAT is a set of tools for processing ADM (Audio Definition Model) files. It can convert ADM files between profiles, validate them, render them, fix common issues, and more.

It contains:

- A :ref:`framework <framework>` for building processing graphs that operate on audio and associated data.
- A :ref:`set of ADM-related processes <available_processes>` that sit within this framework.
- A :ref:`command-line tool<eat-process>` ``eat-process``, which processes ADM files using the processes from the framework, as defined in a :ref:`configuration file <config_file>`.
- A set of :ref:`example configuration files <example_configs>` for the tool, for doing things like profile conversion, loudness measurement, fixing common issues, validation etc.

See the `README file on github <https://github.com/ebu/ebu-adm-toolbox#readme>`_ for installation instructions and license information.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   framework

   config_file

   api/library_root

   example_configs


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
