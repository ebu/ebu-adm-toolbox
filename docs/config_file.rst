.. _config_file:

Configuration File
==================

.. default-domain:: process

EAT processing pipelines (see :ref:`framework`) can be configured through a
JSON file, which specifies a list of processes to run, and connections between
them.

A basic example might look something like this:

.. code-block:: json

    {
      "version": 0,
      "processes": [
        {
          "name": "input",
          "type": "read_adm_bw64",
          "parameters": {
            "path": "/tmp/in.wav"
          },
          "out_ports": ["out_axml", "out_samples"]
        },
        {
          "name": "output",
          "type": "write_adm_bw64",
          "parameters": {
            "path": "/tmp/out.wav"
          },
          "in_ports": ["in_axml", "in_samples"]
        }
      ]
    }

At the root there are three possible keys:

- ``version``

  The version number of the configuration file format, to ensure compatibility with future versions of the EAT.

- ``processes``

  This contains an array of process definitions. Each process is connected to the
  previous one in the sequence through the ports specified in ``in_ports`` and
  ``out_ports``, if they are specified.

  Each process definitions contain the following keys:

  - ``name``: The name to give the process, also used to look up port names.
  - ``type``: The type of the process, see :ref:`available_processes` for the available options.
  - ``out_ports`` (optional): a list of port names, which will be connected to the ``in_ports`` of the next process.
  - ``in_ports`` (optional): a list of port names, which will be connected to the ``out_ports`` of the previous process.
  - ``parameters`` (optional): an object containing parameters specific to this process type (e.g. ``path`` in the above example).

- ``connections`` (optional)

  A list of connections to make between the processes, to allow the creation of arbitrary (non-linear) process graphs.

  Each entry should be an array containing two strings representing the output
  port and input port to connect. Each of these should be of the form
  ``process_name.port_name``.

  For example, this is equivalent to the above configuration:

  .. code-block:: json

      {
        "version": 0,
        "processes": [
          {
            "name": "input",
            "type": "read_adm_bw64",
            "parameters": {
              "path": "/tmp/in.wav"
            }
          },
          {
            "name": "output",
            "type": "write_adm_bw64",
            "parameters": {
              "path": "/tmp/out.wav"
            }
          }
        ],
        "connections": [
          ["input.out_axml", "output.in_axml"],
          ["input.out_samples", "output.in_samples"]
        ]
      }

.. _eat-process:

Running Processes
-----------------

To run the processes described by a configuration file, use the ``eat-process`` tool. This takes a name of a configuration file, and some options.

Options can be used to specify or override a value in the configuration file. For example, the paths in the above examples could be omitted, and a user could run:

.. code-block:: sh

   eat-process example.json -o input.path in.wav -o output.path out.wav

Each option name (e.g. ``input.path``) is the name of a process, followed by a ``.`` and an option name for that process. Option names are turned into rfc6901 "JSON pointers", by replacing ``.`` with ``/`` and prepending a ``/``, so it's possible to modify nested objects and arrays, too.

Setting options on the command-line like this is equivalent to adding or replacing values in the ``parameters`` block in the configuration file.

There are two forms of this for different situations:

- ``-o`` or ``--option``: The value is parsed as JSON. If this fails, then the value is assumed to be a string.
- ``-s`` or ``--strict-option``: The value is parsed as JSON. If this fails, an error is raised.

``-o`` is most useful or interactive usage, while ``-s`` can help prevent errors when used in scripts, but requires strings to be quoted.

``-p`` or ``--progress`` shows a progress bar while processing.

.. _available_processes:

Available Process Types
-----------------------

The following process types are available:

.. process:process:: read_adm

   read ADM data from a BW64 file

   :param string path: path to wav file to read
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: read_bw64

   read samples from a BW64 file

   :param string path: path to wav file to read
   :param int block_size: size of chunks to read
   :output Stream<InterleavedBlockPtr> out_samples: output samples

.. process:process:: read_adm_bw64

   read ADM data and samples from a BW64 file

   :param string path: path to wav file to read
   :param int block_size: size of chunks to read
   :output Stream<InterleavedBlockPtr> out_samples: output samples
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: write_adm_bw64

   write ADM data and samples to a BW64 file

   :param string path: path to wav file to write
   :input Stream<InterleavedBlockPtr> in_samples: input samples
   :input Data<ADMData> in_axml: input ADM data

.. process:process:: write_bw64

   write samples to a BW64 file

   :param string path: path to wav file to write
   :input Stream<InterleavedBlockPtr> in_samples: input samples

.. process:process:: remove_unused

   remove unreferenced elements from an ADM document, and re-pack the channels
   to remove unreferenced channels

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data
   :input Stream<InterleavedBlockPtr> in_samples: input samples
   :output Stream<InterleavedBlockPtr> out_samples: output samples

.. process:process:: remove_unused_elements

   remove unreferenced elements from an ADM document

   in contrast with make_remove_unused, this doesn't do anything with the
   audio, so can be useful if previous changes will not have affected the
   use of channels

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: remove_elements

   remove ADM elements with given IDs

   :param array of strings ids: IDs of elements to remove
   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: validate

   Check ADM data against a given profile. Prints any errors and raises an
   exception if any errors are fined.

   :param object profile:
       Profile specification; ``type`` specifies the profile type. The
       following types are defined:

       - ``itu_emission``: the ITU emission profile. ``level`` (int from 0 to
         2) specifies the profile level.
   :input Data<ADMData> in_axml: input ADM data

.. process:process:: fix_ds_frequency

   add a ``frequency`` element with ``lowPass="120"`` for DirectSpeakers
   channels with ``LFE`` in their name

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: fix_block_durations

   calls :cpp:func:`adm::updateBlockFormatDurations` to fix rounding errors in
   audioBlockFormat durations

   .. note::
      There is currently no limit to the amount that the durations may be
      modified by -- they are always set to match the rtime of the block after,
      or to match the end of the object/programme/file.

   .. note::
      The length of the audioProgramme is not currently inferred from the file
      length, so must be specified.

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: fix_stream_pack_refs

   removes audioPackFormatIDRef in audioStreamFormats that are of type PCM and
   have an audioChannelFormatIDRef

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: convert_track_stream_to_channel

   Replace
   audioTrackUid->audioTrackFormat->audioStreamFormat->audioChannelFormat
   references with audioTrackUid->audioChannelFormat references.

   This doesn't remove any unused elements, so use
   :ref:`remove_unused_elements` or :ref:`remove_unused` after this.

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: render

   render ADM to loudspeaker signals according to BS.2127

   :param string layout: BS.2051 layout name
   :input Data<ADMData> in_axml: input ADM data
   :input Stream<InterleavedBlockPtr> in_samples: input samples
   :output Stream<InterleavedBlockPtr> out_samples: output samples

.. process:process:: add_block_rtimes

   ensure that blocks with a specified duration have an rtime

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: measure_loudness

   measure loudness of loudspeaker signals according to BS.1770

   :param string layout: BS.2051 layout name
   :input Stream<InterleavedBlockPtr> in_samples: input samples
   :output Data<adm::LoudnessMetadata> out_loudness: output loudness data

.. process:process:: set_programme_loudness

   set audioProgramme loudness metadata

   :param string id: audioProgrammeId to modify
   :input Data<adm::LoudnessMetadata> in_loudness: input loudness data
   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: update_all_programme_loudnesses

   measure the loudness of all audioProgrammes (by rendering
   them to 4+5+0) and updates the axml to match

   :input Data<ADMData> in_axml: input ADM data
   :input Stream<InterleavedBlockPtr> in_samples: input samples
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: set_profiles

   set the list of profiles in an ADM document

   :param list profiles: see :ref:`validate`
   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: set_position_defaults

   add explicit default values for elevation and Z position coordinates

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: remove_silent_atu

   replace silent audioTrackUID references (with ID 0) with real audioTrackUIDs
   that reference a silent track

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data
   :input Stream<InterleavedBlockPtr> in_samples: input samples
   :output Stream<InterleavedBlockPtr> out_samples: output samples

.. process:process:: remove_jump_position

   Remove the jumpPosition sub-elements from audioBlockFormat sub elements of type objects.
   Where interpolationLength is set such that the interpolation does not occur across the
   whole block, split into two blocks representing the interpolated and fixed parts.

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: resample_blocks

   Change the timing information of audioBlockFromat sub-elements, such that
   no block is shorter than min-duration. The first block is a special case,
   in that if it has a duration of 0, that will be preserved regardless of
   min-duration.
   The min-duration parameter is in adm time format, eg 100S44100 for fractional representation or 00:00:00050 for timecode representation
   The representation format used for min-duration must match that used in the audioBlockFormats of the input xml.

   :param string min-duration: The minimum duration allowed for output blocks, in adm time format
   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: remove_object_times_data_safe

   remove time/duration from audioObjects where it is safe to do so (doesn't
   potentially change the rendering) and can be done by only changing the
   metadata (no audio changes, no converting common definitions
   audioChannelFormats to real audioChannelFormats

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: remove_object_times_common_unsafe

   remove start and duration from audioObjects which only reference common
   definitions audioChannelFormats

   this could cause rendering changes if there are non-zero samples outside
   the range of the audioObject, but should be safe on EPS output

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: remove_importance

   remove importance values from all audioObjects, audioPackFormats and
   audioBlockFormats

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: drop_blockformat_subelements

   Drop specified sub-elements from all AudioBlockFormats
   This processor simply removes the subelements and does not attempt to replace them in any way.

   :param list objects_subelements: A list of subelements to remove from AudioBlockFormats with Object type. Valid values are
    "Diffuse", "ChannelLock", "ObjectDivergence", "JumpPosition", "ScreenRef", "Width", "Depth", "Height", "Gain", "Importance", "Headlocked" and "HeadphoneVirtualise"
   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: rewrite_content_objects_emission

   rewrite the programme-content-object structure to make it compatible with
   emission profile rules

   the restrictions are:

   - audioContents can only reference one audioObject
   - only audioObjects containing Objects content can be nested
   - there's an undefined "maximum nest level" of 2

   this may drop audioContents or audioObjects that are too nested (only ones
   that have audioObject references), so any information which applies to the
   audioObjects below will be lost

   :param int max_objects_depth=2: the maximum object depth allowed for any
       object, defined as the maximum number of audioObject references between the
       object and any objects with audio content (audioPackFormat/audioTrackFormat)
   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: infer_object_interact

   ensure that all audioObjects have an interact parameter based on the
   presence or absence of the audioObjectInteraction element

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data

.. process:process:: set_version

   set the audioFormatExtended version

   :input Data<ADMData> in_axml: input ADM data
   :output Data<ADMData> out_axml: output ADM data
   :param string version: the version string to set

.. process:process:: set_content_dialogue_default

   set missing audioContent dialogue values to mixed

   :input data<admdata> in_axml: input adm data
   :output data<admdata> out_axml: output adm data
