Example Configuration Files
===========================

.. note that this is included from ../docs/example_configs, so paths are
   relative to that

.. default-domain:: process

- :download:`fix_dolby.json <../example_configs/fix_dolby.json>`

  fix issues in Dolby Atmos ADM Profile files to be compatible with the BS.2127
  renderer

  runs :ref:`fix_ds_frequency`, :ref:`fix_block_durations` and :ref:`fix_stream_pack_refs`.

  required options: ``input.path`` and ``output.path``

- :download:`track_stream_to_channel.json <../example_configs/track_stream_to_channel.json>`

  replace
  audioTrackUid->audioTrackFormat->audioStreamFormat->audioChannelFormat
  references with audioTrackUid->audioChannelFormat references

  runs :ref:`convert_track_stream_to_channel` and :ref:`remove_unused_elements`

  required options: ``input.path`` and ``output.path``

- :download:`render.json <../example_configs/render.json>`

  render an ADM BW64 file

  runs :ref:`add_block_rtimes` and :ref:`render`

  required options: ``input.path``, ``render.layout`` and ``output.path``

- :download:`measure_loudness.json <../example_configs/measure_loudness.json>`

  update the loudness of one audioProgramme in a BW64 wav file

  this currently works by rendering the programme to 4+5+0 and measuring the
  loudness of the output, so this may not be accurate for channel-based content
  which would be better measured directly

  required options: ``input.path`` and ``output.path``

- :download:`measure_all_loudness.json <../example_configs/measure_all_loudness.json>`

  update the loudness of all audioProgrammes in a BW64 wav file

  this currently works by rendering the programme to 4+5+0 and measuring the
  loudness of the output, so this may not be accurate for channel-based content
  which would be better measured directly

  required options: ``input.path`` and ``output.path``

- :download:`conform_block_timing.json <../example_configs/conform_block_timing.json>`

  conform the timing of object type audioblockformats to the emission profile

  this first removes the jump position flag from any audioblockformats in which it is present.
  these blocks are replaced with multiple blocks representing the same change where appropriate.
  it then ensures no block other than the first is shorter than 5ms in duration by combining short blocks, starting from the end.

  required options: ``input.path`` and ``output.path``

- :download:`conform_to_emission.json <../example_configs/conform_to_emission.json>`

  read and write an ADM BW64 file, applying various processes to make it closer
  to the emission profile

  required options: ``input.path`` and ``output.path``

- :download:`validate_emission.json <../example_configs/validate_emission.json>`

  read an ADM BW64 file and check it for compatibility with the emission profile

  required options: ``input.path``
