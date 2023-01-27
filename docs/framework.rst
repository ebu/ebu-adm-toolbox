.. _framework:

Framework
=========

.. cpp:namespace:: eat::framework

The framework of the ADM Toolbox provides a structure for components which
process ADM files to fit into.

This takes the form of a *processing graph*: The individual components are
*processes*, which have input and output *ports* through which they
communicate. These can be connected together in a *graph* structure, which is a
collection of processes, and connections between their ports.

For example, consider a "read BW64" process with output ports for audio samples
and an ADM document, and a "write BW64" process with input ports for audio
samples and an ADM document. These may be connected together to form a kind of
BW64 copy operation.

Processes
---------

There are several kinds of process, shown in the following inheritance diagram:

.. graphviz::

   digraph {
        rankdir="BT";
        node[shape="box"];

        CompositeProcess -> Process;
        AtomicProcess -> Process;
        FunctionalAtomicProcess -> AtomicProcess;
        StreamingAtomicProcess -> AtomicProcess;
   }

:class:`Process` is the base class for all process types. It has input and
output ports, and can be added to graphs.

This is split into two types: :class:`AtomicProcess` and
:class:`CompositeProcess`. Atomic processes actually implement some processing
(i.e. they are not divisible), while composite processes just contain other
processes and connections between them, which may be themselves be composite or
atomic.

Atomic processes are further divided into two types:
:class:`FunctionalAtomicProcess` and :class:`StreamingAtomicProcess`.

.. cpp:namespace-push:: FunctionalAtomicProcess

In a functional processes, the outputs are a function of the inputs: they
implement a :func:`process` method, which is called once, and should read from
the input ports and write to the output ports.

.. cpp:namespace-pop::
.. cpp:namespace-push:: StreamingAtomicProcess

While functional processes are limited to operating on single pieces of data,
streaming processes operate on streams of data. They implement three methods:

- :func:`initialise` is called once, and can read from non-streaming input ports
- :func:`process` should read from streaming input ports and write to streaming
  output ports, and is called repeatedly as long as any ports are not closed
  (see below)
- :func:`finalise` is called once, and can write to non-streaming output ports

For example, a loudness meter would be a streaming process: ``process`` would
read audio samples from a streaming input port, performing the analysis. and
the accumulated loudness values would be written to a non-streaming output port
in ``finalise``.

.. cpp:namespace-pop::

Ports
-----

As aluded to above, processes can have two kinds of ports, *data ports* and
*streaming ports*. Additionally, each port has a type, and can only be
connected to ports of the same type. This is shown in this inheritance diagram:

.. graphviz::

   digraph {
        rankdir="BT";
        node[shape="box"];
        StreamPortBase -> Port;
        DataPortBase -> Port;
        "StreamPort<T>" -> StreamPortBase;
        "DataPort<T>" -> DataPortBase;
   }

:class:`Port` can be used to reference any type of port, and is mainly used for
making connections. :class:`DataPort` and :class:`StreamPort` are concrete
ports of a particular type, and are mostly used inside processes.
:class:`DataPortBase` and :class:`StreamPortBase` are interfaces used by the
implementation.

.. cpp:namespace-push:: template <typename T> DataPort

Data ports hold a value of the given type. A process writing to a data port
should use :func:`set_value`, while a process reading from a data port should
use :func:`get_value`.

The framework moves or copies this value between connected ports.

.. cpp:namespace-pop::

.. cpp:namespace-push:: template <typename T> StreamPort

Stream ports hold a queue of values of the given type, and an ``eof`` (End Of
File) flag.

Writers should call :func:`push` to write items, followed by :func:`close` to
signal that no more items will be pushed.

Readers should use :func:`available` to see if there are any items in the
queue, and :func:`pop` to read an item. When :func:`eof` returns true, there
are no items left to read, and the writer has closed the port.

The framework moves or copies items and the ``eof`` flag between ports.

.. cpp:namespace-pop::

See :ref:`port_value_semantics` for more detail on how data is transferred between ports.

An Example
----------

The graph below is for an application which normalises the levels in an audio
file while retaining ADM metadata:

.. graphviz::

   digraph g {
     rankdir=LR;
     node [shape=record,height=.1]
     pr10682112[label = "{{} | in_path | {<po10682320>out}}"];
     pr10682592[label = "{{} | out_path | {<po10682800>out}}"];
     pr10683024[label = "{{<po10683264>in_path} | reader | {<po10683440>out_axml|<po10683664>out_samples}}"];
     pr10686880[label = "{{<po10687120>in_samples} | normalise | {<po10687984>out_samples}}"];
     pr10692608[label = "{{<po10693024>in_axml|<po10692848>in_path|<po10693248>in_samples} | writer | {}}"];
     pr10682112:po10682320:e -> pr10683024:po10683264:w;
     pr10683024:po10683664:e -> pr10686880:po10687120:w[color=red];
     pr10682592:po10682800:e -> pr10692608:po10692848:w;
     pr10683024:po10683440:e -> pr10692608:po10693024:w;
     pr10686880:po10687984:e -> pr10692608:po10693248:w[color=red];
   }

Red lines represent streaming connections. Processes are shown as columns with
the input ports on the left, output ports on the right, and the process name in
the middle.

The components are as follows:

- *in_path* and *out_path* are :class:`DataSource` processes which produce the
  input and output path name
- *reader* is a composite process which reads ADM metadata and samples
  (streaming) from a BW64 file
- *writer* is a composite process which writes ADM metadata and samples
  (streaming) to a BW64 file.
- *normalise* is a composite process which normalises its input samples to
  produce some output samples.

If these composite processes are expanded it looks like this (you may have to
open in a new tab...):

.. graphviz::

   digraph g {
     rankdir=LR;
     node [shape=record,height=.1]
     pr33627904[label = "{{} | in_path | {<po33628112>out}}"];
     pr33628384[label = "{{} | out_path | {<po33628592>out}}"];
     subgraph cluster_cp33628816 {
       label="reader"
       pr33630320[label = "{{<po33630512>in_path} | adm reader | {<po33630688>out_axml}}"];
       pr33630912[label = "{{<po33631152>in_path} | audio reader | {<po33631328>out_samples}}"];
       po33629056[label="in_path",style=rounded];
       po33629232[label="out_axml",style=rounded];
       po33629456[label="out_samples",style=rounded];
       pr33630320:po33630688:e -> po33629232:w;
       pr33630912:po33631328:e -> po33629456:w[color=red];
       po33629056:e -> pr33630320:po33630512:w;
       po33629056:e -> pr33630912:po33631152:w;
     }
     subgraph cluster_cp33632672 {
       label="normalise"
       pr33634640[label = "{{<po33634864>in_samples} | analyse | {<po33635728>out_rms}}"];
       pr33635920[label = "{{<po33637888>in_rms|<po33636160>in_samples} | apply | {<po33637024>out_samples}}"];
       po33632912[label="in_samples",style=rounded];
       po33633776[label="out_samples",style=rounded];
       pr33635920:po33637024:e -> po33633776:w[color=red];
       po33632912:e -> pr33634640:po33634864:w[color=red];
       po33632912:e -> pr33635920:po33636160:w[color=red];
       pr33634640:po33635728:e -> pr33635920:po33637888:w;
     }
     subgraph cluster_cp33638400 {
       label="writer"
       pr33639904[label = "{{<po33640272>in_axml|<po33640096>in_file} | adm writer | {}}"];
       pr33640496[label = "{{<po33640720>in_path|<po33640896>in_samples} | audio writer | {<po33641760>out_file}}"];
       po33638640[label="in_path",style=rounded];
       po33638816[label="in_axml",style=rounded];
       po33639040[label="in_samples",style=rounded];
       pr33640496:po33641760:e -> pr33639904:po33640096:w;
       po33638816:e -> pr33639904:po33640272:w;
       po33638640:e -> pr33640496:po33640720:w;
       po33639040:e -> pr33640496:po33640896:w[color=red];
     }
     pr33627904:po33628112:e -> po33629056:w;
     po33629456:e -> po33632912:w[color=red];
     pr33628384:po33628592:e -> po33638640:w;
     po33629232:e -> po33638816:w;
     po33633776:e -> po33639040:w[color=red];
   }

Here, composite processes are shown as boxes containing their constituent
processes, with rounded boxes representing their input and output ports (due to the
limitations of graphviz).

Zooming in, *reader* looks like this:

.. graphviz::

   digraph g {
     rankdir=LR;
     node [shape=record,height=.1]
     subgraph cluster_cp33628816 {
       label="reader"
       pr33630320[label = "{{<po33630512>in_path} | adm reader | {<po33630688>out_axml}}"];
       pr33630912[label = "{{<po33631152>in_path} | audio reader | {<po33631328>out_samples}}"];
       po33629056[label="in_path",style=rounded];
       po33629232[label="out_axml",style=rounded];
       po33629456[label="out_samples",style=rounded];
       pr33630320:po33630688:e -> po33629232:w;
       pr33630912:po33631328:e -> po33629456:w[color=red];
       po33629056:e -> pr33630320:po33630512:w;
       po33629056:e -> pr33630912:po33631152:w;
     }
   }

It consists of two independent processes which read the samples and ADM data,
so there is no ordering constraint between them.

Writer is more complex:

.. graphviz::

   digraph g {
     rankdir=LR;
     node [shape=record,height=.1]
     subgraph cluster_cp33638400 {
       label="writer"
       pr33639904[label = "{{<po33640272>in_axml|<po33640096>in_file} | adm writer | {}}"];
       pr33640496[label = "{{<po33640720>in_path|<po33640896>in_samples} | audio writer | {<po33641760>out_file}}"];
       po33638640[label="in_path",style=rounded];
       po33638816[label="in_axml",style=rounded];
       po33639040[label="in_samples",style=rounded];
       pr33640496:po33641760:e -> pr33639904:po33640096:w;
       po33638816:e -> pr33639904:po33640272:w;
       po33638640:e -> pr33640496:po33640720:w;
       po33639040:e -> pr33640496:po33640896:w[color=red];
     }
   }

libbw64 does not support editing files, so the samples and ADM metadata need to
be written using the same ``Bw64Writer`` object. To do this, the *audio writer*
process sends the writer object out of a port, which is used by the *adm
writer* process. These could technically be merged into one atomic process, but
this way the ADM metadata does not have to be available before the samples.

The *normalise* process looks like this:

.. graphviz::

   digraph g {
     rankdir=LR;
     node [shape=record,height=.1]
     subgraph cluster_cp33632672 {
       label="normalise"
       pr33634640[label = "{{<po33634864>in_samples} | analyse | {<po33635728>out_rms}}"];
       pr33635920[label = "{{<po33637888>in_rms|<po33636160>in_samples} | apply | {<po33637024>out_samples}}"];
       po33632912[label="in_samples",style=rounded];
       po33633776[label="out_samples",style=rounded];
       pr33635920:po33637024:e -> po33633776:w[color=red];
       po33632912:e -> pr33634640:po33634864:w[color=red];
       po33632912:e -> pr33635920:po33636160:w[color=red];
       pr33634640:po33635728:e -> pr33635920:po33637888:w;
     }
   }

The *analyse* process takes streaming audio and measures the RMS level of the
whole of each channel; these are produced on a data port. These RMS levels are
used by the *apply* process to modify the level of the input samples.

Evaluation
~~~~~~~~~~

To evaluate the graph, the first step is to flatten it, expanding composite
processes:

.. graphviz::

   digraph g {
     rankdir=LR;
     node [shape=record,height=.1]
     pr20684544[label = "{{} | in_path | {<po20684752>out}}"];
     pr20685024[label = "{{} | out_path | {<po20685232>out}}"];
     pr20686960[label = "{{<po20687152>in_path} | adm reader | {<po20687328>out_axml}}"];
     pr20687552[label = "{{<po20687792>in_path} | audio reader | {<po20687968>out_samples}}"];
     pr20691280[label = "{{<po20691504>in_samples} | analyse | {<po20692368>out_rms}}"];
     pr20692560[label = "{{<po20694528>in_rms|<po20692800>in_samples} | apply | {<po20693664>out_samples}}"];
     pr20696544[label = "{{<po20696912>in_axml|<po20696736>in_file} | adm writer | {}}"];
     pr20697136[label = "{{<po20697360>in_path|<po20697536>in_samples} | audio writer | {<po20698400>out_file}}"];
     pr20684544:po20684752:e -> pr20686960:po20687152:w;
     pr20684544:po20684752:e -> pr20687552:po20687792:w;
     pr20687552:po20687968:e -> pr20691280:po20691504:w[color=red];
     pr20687552:po20687968:e -> pr20692560:po20692800:w[color=red];
     pr20691280:po20692368:e -> pr20692560:po20694528:w;
     pr20697136:po20698400:e -> pr20696544:po20696736:w;
     pr20686960:po20687328:e -> pr20696544:po20696912:w;
     pr20685024:po20685232:e -> pr20697136:po20697360:w;
     pr20692560:po20693664:e -> pr20697136:po20697536:w[color=red];
   }

This exposes a problem: there is a streaming connection from *audio reader* to
*analyse* and *apply*, but there's a non-streaming connection between *analyse*
and *apply*. Because non-streaming ports are read before streaming and written
after streaming (see :class:`StreamingAtomicProcess`), it's not possible to
stream between all three processes simultaneously.

To deal with this situation, *buffer writer* and *buffer reader* processes are
automatically inserted to split enough streaming connections that this does not
occur. The graph then looks like this:

.. graphviz::

   digraph g {
     rankdir=LR;
     node [shape=record,height=.1]
     pr19980032[label = "{{} | in_path | {<po19980240>out}}"];
     pr19980512[label = "{{} | out_path | {<po19980720>out}}"];
     pr19982448[label = "{{<po19982640>in_path} | adm reader | {<po19982816>out_axml}}"];
     pr19983040[label = "{{<po19983280>in_path} | audio reader | {<po19983456>out_samples}}"];
     pr19986768[label = "{{<po19986992>in_samples} | analyse | {<po19987856>out_rms}}"];
     pr19988048[label = "{{<po19990016>in_rms|<po19988288>in_samples} | apply | {<po19989152>out_samples}}"];
     pr19992032[label = "{{<po19992400>in_axml|<po19992224>in_file} | adm writer | {}}"];
     pr19992624[label = "{{<po19992848>in_path|<po19993024>in_samples} | audio writer | {<po19993888>out_file}}"];
     pr20001056[label = "{{<po20001280>in_samples} | buffer writer | {<po19996256>out_path}}"];
     pr20003712[label = "{{<po20003984>in_path} | buffer reader | {<po20004160>out_samples}}"];
     pr19980032:po19980240:e -> pr19982448:po19982640:w;
     pr19980032:po19980240:e -> pr19983040:po19983280:w;
     pr19983040:po19983456:e -> pr19986768:po19986992:w[color=red];
     pr20003712:po20004160:e -> pr19988048:po19988288:w[color=red];
     pr19986768:po19987856:e -> pr19988048:po19990016:w;
     pr19992624:po19993888:e -> pr19992032:po19992224:w;
     pr19982448:po19982816:e -> pr19992032:po19992400:w;
     pr19980512:po19980720:e -> pr19992624:po19992848:w;
     pr19988048:po19989152:e -> pr19992624:po19993024:w[color=red];
     pr19983040:po19983456:e -> pr20001056:po20001280:w[color=red];
     pr20001056:po19996256:e -> pr20003712:po20003984:w;
   }

Now *audio reader*, *analyse* and *buffer writer* can run together,
followed by *buffer reader*, *apply* and *audio writer*, because there
are no non-streaming connections within each of these sub-graphs.

The type of buffer writer and reader used can be specialised for each type of
streaming port by specialising :class:`MakeBuffer`. The default implementation
buffers stream values into a ``std::vector``, which defeats the memory savings
of streaming. A specialisation is provided for audio samples which writes to a
temporary wav file instead.

.. _port_value_semantics:

Port Value Semantics
--------------------

One output port may be connected to multiple input ports. To implement this,
the value stored in the port is copied to all but the last connected port, and
moved to the last output port.

Thus, the data stored in a port should have value semantics -- that is a copy
creates a new value with the same contents, and changing one copy does not
affect other copies. This is done because it's much easier to implement
sensible reference semantics on top of value semantics, than it is the other
way around.

Basic types (ints, floats etc.), POD types and STL containers meet this
criteria, while ``std::shared_ptr`` and some custom classes do not.

To work with types like libadm documents which are always accessed through a
``shared_ptr``, :class:`ValuePtr` is provided. This allows each reader to chose
whether they want a const or a non-const pointer, which can save copying the
document in cases where all readers access the value through a const pointer,
or there is only one reader which access the value through a non-const pointer.


Other Features
--------------

This section is lists features that the framework is designed to support, but
are not currently implemented.

Progress
~~~~~~~~

When processing large files it would be nice to indicate the progress to the
user. There are two parts to this:

- Each :class:`ExecStep` in a :class:`Plan` represents one step of the
  evaluation. These should provide more information about what they are doing
  (e.g. a name and a list of processes it will run) so that the overall process
  through the graph can be reported.

- Streams should be able to optionally report their progress as a percentage.
  Often there will be just one process in a streaming sub-graph that knows how
  far through it is (e.g. a file reader), and this can be reported through a
  callback.

Streaming ADM
~~~~~~~~~~~~~

A streaming ADM BW64 file can be thought of as a sequence of ADM documents with
associated ranges of samples. To process these within this framework, one
solution would be to allow the graph to run multiple times (once on each
document). This should allow components to be shared between streaming and
non-streaming uses.

Duplicating Streaming Processes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The example in the last section is wasteful, in that the samples from the
original file are written to and read from a temporary file in order to break a
streaming connection -- It would be better if the original file could be read a
second time.

Processes should be able to be specify that they are safe to copy (i.e. will
always produce the same output given the same inputs with no side-effects), and
the framework should prefer to use this if possible to break streaming
connections.

Exceptions
~~~~~~~~~~

Processes can raise exceptions while running. Currently these are just
propagated to the user, but should be wrapped in another exception that records
which process they came from in order to improve error reporting.

Writing Processes
-----------------

This section briefly explains how to write some different kinds of processes.

Functional Atomic Process
~~~~~~~~~~~~~~~~~~~~~~~~~

.. cpp:namespace-push:: FunctionalAtomicProcess

An example of a process that adds 1 to an integer input:

.. code-block:: c++

   class AddOne : public FunctionalAtomicProcess {
    public:
     AddOne(const std::string &name)
         : FunctionalAtomicProcess(name),
           in(add_in_port<DataPort<int>>("in")),
           out(add_out_port<DataPort<int>>("out")) {}

     virtual void process() override {
       out->set_value(in->get_value() + 1);
     }

    private:
     DataPortPtr<int> in;
     DataPortPtr<int> out;
   };

Note that:

- The process name is passed in through the constructor, and should normally be
  passed straight to the :class:`FunctionalAtomicProcess` constructor.

- Ports are added through :func:`Process::add_in_port` and :func:`Process::add_out_port`, with
  the port type as a parameter. These are saved (as the corresponding pointer
  type) for use in process.

- :func:`DataPort::get_value` and :func:`DataPort::set_value` are used to get
  and set the values of input and output port respectively.

For heavier types, :func:`std::move` should be used in :func:`process` like
this example with a std::vector input and output:

.. code-block:: c++

   virtual void process() override {
     // move from the input port to avoid copying the data
     std::vector<int> value = std::move(in->get_value());

     // modify it
     value.push_back(7);

     // move to the output port to avoid copying again
     out->set_value(std::move(value));
   }

For types using :class:`ValuePtr`, it will look something like this for
modifying a value in-place:

.. code-block:: c++

   virtual void process() override {
     // get the wrapper
     ValuePtr<std::vector<int>> value_ptr = std::move(in->get_value());

     // extract the value; this will copy or move if it's the last user
     std::shared_ptr<std::vector<int>> value = value_ptr.move_or_copy();

     // modify it
     value->push_back(7);

     // move to the output port
     out->set_value(std::move(value));
   }

Or this if read-only access is OK:

.. code-block:: c++

   virtual void process() override {
     // get the wrapper
     ValuePtr<std::vector<int>> value_ptr = std::move(in->get_value());

     // extract a reference to the value
     std::shared_ptr<const std::vector<int>> value = value_ptr.read();

     // use it somehow
     out->set_value(value->at(0));
   }

.. cpp:namespace-pop::

Streaming Atomic Process
~~~~~~~~~~~~~~~~~~~~~~~~

.. cpp:namespace-push:: StreamingAtomicProcess

An example of a process that produces a stream that's the same as the input, but one greater:

.. code-block:: c++

   class AddOneStream : public StreamingAtomicProcess {
    public:
     AddOneStream(const std::string &name)
         : StreamingAtomicProcess(name),
           in(add_in_port<StreamPort<int>>("in")),
           out(add_out_port<StreamPort<int>>("out")) {}

     virtual void process() override {
       while (in->available())
         out->push(in->pop() + 1);

       if (in->eof())
         out->close();
     }

    private:
     StreamPortPtr<int> in;
     StreamPortPtr<int> out;
   };

Note that:

- All ports must be empty and closed (check with :func:`StreamPort::eof`) for
  the streaming to finish. Generally output ports should be closed once
  corresponding inputs have ended (:func:`StreamPort::eof`). It's valid to
  close a port multiple times (it has no effect), so there's no need to track
  if it's been closed or not.

- :func:`StreamPort::push` and :func:`StreamPort::pop` behave similarly to
  :func:`DataPort::set_value` and :func:`DataPort::get_value`, so the above
  information about :func:`std::move` and :class:`ValuePtr` apply here too.

- :func:`StreamPort::pop` will throw an exception if there's nothing in the
  queue, so use :func:`StreamPort::available`.

- To get data from input data ports or send data to output data ports, override
  :func:`initialise` or :func:`finalise` respectively.

.. cpp:namespace-pop::

Composite Process
~~~~~~~~~~~~~~~~~

Here's a composite process which chains together two ``AddOne`` processes
defined earlier:

.. code-block:: c++

   class AddTwo : public CompositeProcess {
    public:
     AddTwo(const std::string &name)
         : CompositeProcess(name) {
       // add ports for this process
       auto in = add_in_port<DataPort<int>>("in");
       auto out = add_out_port<DataPort<int>>("out");

       // add sub-processes
       auto p1 = add_process<AddOne>("p1");
       auto p2 = add_process<AddOne>("p2");

       // connect everything together
       connect(in, p1->get_in_port("in"));
       connect(p1->get_out_port("out"), p2->get_in_port("in"));
       connect(p2->get_out_port("out"), out);
     }
   };

- Ports are added using the same functions as for atomic processes.
- Sub-processes are added using :func:`Graph::add_process` or
  :func:`Graph::register_process`.
- Ports are connected using :func:`Graph::connect`. :func:`Process::get_out_port` and
  :func:`Process::get_in_port` are used to access ports of sub-processes by
  name. All external ports, and ports of sub-processes must be connected.
- There's no need to keep a reference to the ports or processes.

Building Processing Graphs
--------------------------

Processing graphs can be built with the :class:`Graph` class, and ran using
:func:`evaluate`, like this:

.. code-block:: c++

   Graph g;

   auto p1 = g.add_process<AddOne>("p1");
   auto p2 = g.add_process<AddOne>("p2");

   // connect processes together
   g.connect(p1->get_out_port("out"), p2->get_in_port("in"));

   // add input
   auto in = g.add_process<DataSource<int>>("input", 5);
   g.connect(in, p1->get_in_port("in"));

   // add output
   auto out = g.add_process<DataSink<int>>("output");
   g.connect(p2->get_out_port("out"), out);

   // run the graph
   evaluate(g);

   // check the output
   assert(out->get_value() == 7);

This is exactly the same API as is used for building composite processes.

Again, all ports of all processes mus be connected. Inputs and outputs can
accessed by adding and connecting :class:`DataSource` and :class:`DataSink`
processes, and unused output ports can be terminated with :class:`NullSink`
processes.
