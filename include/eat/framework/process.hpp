#pragma once
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace eat::framework {

class Port;
class DataPortBase;
class StreamPortBase;
class Process;
class FunctionalAtomicProcess;
class StreamingAtomicProcess;
class AtomicProcess;
class CompositeProcess;
class Graph;

using PortPtr = std::shared_ptr<Port>;
using ProcessPtr = std::shared_ptr<Process>;
using GraphPtr = std::shared_ptr<Graph>;

using DataPortBasePtr = std::shared_ptr<DataPortBase>;
using StreamPortBasePtr = std::shared_ptr<StreamPortBase>;
using AtomicProcessPtr = std::shared_ptr<AtomicProcess>;
using FunctionalAtomicProcessPtr = std::shared_ptr<FunctionalAtomicProcess>;
using StreamingAtomicProcessPtr = std::shared_ptr<StreamingAtomicProcess>;
using CompositeProcessPtr = std::shared_ptr<CompositeProcess>;

/// A graph of processes, storing a collection of process references, and
/// connections between the ports
class Graph {
 public:
  virtual ~Graph() {}

  /// construct and register a process with a given type
  template <typename T, typename... Args>
  std::shared_ptr<T> add_process(Args &&...args);

  ProcessPtr register_process(ProcessPtr process);
  void connect(const PortPtr &a, const PortPtr &b);

  const std::vector<ProcessPtr> &get_processes() const { return processes; }
  const std::map<PortPtr, PortPtr> &get_port_inputs() const { return port_inputs; }

 private:
  std::vector<ProcessPtr> processes;

 protected:
  std::map<PortPtr, PortPtr> port_inputs;
};

/// abstract process, for referencing either an atomic or composite process
class Process {
 public:
  explicit Process(const std::string &name);
  virtual ~Process() {}

  /// construct and register a port with a given type and name
  template <typename T>
  std::shared_ptr<T> add_in_port(const std::string &name);

  /// construct and register a port with a given type and name
  template <typename T>
  std::shared_ptr<T> add_out_port(const std::string &name);

  /// get an input port with a given name and type; will throw if there is no
  /// port with the given name, or it is not castable to the right type
  template <typename T = Port>
  std::shared_ptr<T> get_in_port(const std::string &name) const;

  /// see get_in_port
  template <typename T = Port>
  std::shared_ptr<T> get_out_port(const std::string &name) const;

  // get all ports in one map for compatibility
  std::map<std::string, PortPtr> get_port_map() const;

  const std::map<std::string, PortPtr> &get_in_port_map() const { return in_ports; }
  const std::map<std::string, PortPtr> &get_out_port_map() const { return out_ports; }

  const std::string &name() const { return name_; }

 private:
  std::map<std::string, PortPtr> in_ports;
  std::map<std::string, PortPtr> out_ports;
  std::string name_;
};

/// abstract process which actually does something (FunctionalAtomicProcess or
/// StreamingAtomicProcess), rather than a graph of other processes (CompositeProcess)
class AtomicProcess : public Process {
 public:
  using Process::Process;

 private:
};

/// non-streaming process
///
/// once all processes connected to input ports have been processed, process()
/// will be called once, before processes connected to the output ports are ran
class FunctionalAtomicProcess : public AtomicProcess {
 public:
  using AtomicProcess::AtomicProcess;
  virtual void process() = 0;
};

/// streaming process with the following callbacks:
///
/// - initialise() will be called once, after all processes connected to this
///   via non-streaming input ports have been ran (so non-streaming inputs are available)
///
/// - process() will be called many times, as long as any streaming ports
///   are not closed; it should read from streaming input ports, and write to
///   streaming output ports, closing them once there's no more data to write
///
/// - finalise() will be called once, before processes connected to this via
///   non-streaming output ports are ran
class StreamingAtomicProcess : public AtomicProcess {
 public:
  using AtomicProcess::AtomicProcess;
  virtual void initialise() {}
  virtual void process() {}
  virtual void finalise() {}

  /// get progress for this process as a fraction between 0 and 1 if known
  virtual std::optional<float> get_progress() { return std::nullopt; }
};

/// A process which just contains some other processes, and connections between
/// them
class CompositeProcess : public Process, public Graph {
 public:
  void connect(const PortPtr &a, const PortPtr &b);

  using Process::Process;
};

/// a port which carries data between processes
///
/// whether it's an input or output port depends on how it's connected
class Port {
 public:
  explicit Port(const std::string &name);
  virtual ~Port() = default;
  const std::string &name() const { return name_; }

  /// are connections between this and other valid (i.e. the same type)
  virtual bool compatible(const PortPtr &other) const = 0;

 private:
  std::string name_;
};

/// abstract port for non-streaming data; see DataPort<T>
class DataPortBase : public Port {
 public:
  // methods used by evaluate to move data around
  virtual void move_to(DataPortBase &) = 0;
  virtual void copy_to(DataPortBase &) = 0;
  using Port::Port;
};

/// non-streaming data port for a specific type, T
///
/// T should be copyable and movable (value semantics), so that processes will
/// not interact (i.e. see changes from non-upstream processes), and don't have
/// to copy manually
///
/// for heavy types (e.g. vector) it may be more efficient to use a smart
/// pointer with const contents (like std::shared_ptr<const std::vector<...>>)
/// to avoid copying unnecessarily
///
/// for types with RAII behaviour, readers should move from get_value to ensure
/// that resources are freed as soon as possible
template <typename T>
class DataPort : public DataPortBase {
 public:
  virtual bool compatible(const PortPtr &other) const override;

  /// set the value -- use this from a process for which this port is an output
  template <typename U>
  void set_value(U &&value);

  /// get the value -- use this from a process for which this port is an input
  const T &get_value() const;
  T &get_value();

  // internals used to connect ports:

  virtual void move_to(DataPortBase &other) override;
  virtual void copy_to(DataPortBase &other) override;

  using DataPortBase::DataPortBase;

 private:
  T value;
};

template <typename T>
using DataPortPtr = std::shared_ptr<DataPort<T>>;

/// port that has a stream of data with a given type (see StreamPort<T>)
///
/// the output side calls push(data) n times then close() once
///
/// the input side calls pop() while available(), and can know that no more data
/// will become available if eof()
class StreamPortBase : public Port {
 public:
  /// end the stream of data
  virtual void close() = 0;

  /// has the stream ended? true when there's no more data to read, and the other side has called close()
  virtual bool eof() = 0;

  /// has close been called (i.e. eof() will become true once the queue is
  /// drained)
  virtual bool eof_triggered() = 0;

  // internals used to connect ports:

  virtual void copy_to(StreamPortBase &other) = 0;
  virtual void move_to(StreamPortBase &other) = 0;
  virtual void clear() = 0;

  /// get a process with a compatible streaming input and a non-streaming
  /// output compatible with get_buffer_writer(), which writes the inputs to a
  /// buffer
  virtual ProcessPtr get_buffer_writer(const std::string &name) = 0;
  /// get a process with a compatible streaming output and a non-streaming
  /// input compatible with get_buffer_reader(), which reads from a buffer
  virtual ProcessPtr get_buffer_reader(const std::string &name) = 0;

  using Port::Port;
};

/// stream port containing items of type T
template <typename T>
class StreamPort : public StreamPortBase {
 public:
  virtual bool compatible(const PortPtr &other) const override;

  void push(T value);
  bool available() const;
  T pop();

  virtual void close() override;

  virtual bool eof() override;

  // internals

  virtual bool eof_triggered() override;
  virtual void copy_to(StreamPortBase &other) override;
  virtual void move_to(StreamPortBase &other) override;
  virtual void clear() override;

  virtual ProcessPtr get_buffer_writer(const std::string &name) override;
  virtual ProcessPtr get_buffer_reader(const std::string &name) override;

  using StreamPortBase::StreamPortBase;

 private:
  std::deque<T> queue;
  bool eof_ = false;
};

template <typename T>
using StreamPortPtr = std::shared_ptr<StreamPort<T>>;

/// for specialising get_buffer_writer / get_buffer_reader for different types
template <typename T>
struct MakeBuffer {
  static ProcessPtr get_buffer_writer(const std::string &name);
  static ProcessPtr get_buffer_reader(const std::string &name);
};

// implementations

template <typename T, typename... Args>
std::shared_ptr<T> Graph::add_process(Args &&...args) {
  auto process = std::make_shared<T>(std::forward<Args>(args)...);
  register_process(process);
  return process;
}

template <typename T>
std::shared_ptr<T> Process::add_in_port(const std::string &name) {
  auto port = std::make_shared<T>(name);
  auto result = in_ports.emplace(port->name(), port);
  if (!result.second) throw std::runtime_error("duplicate input port: " + name);
  return port;
}

template <typename T>
std::shared_ptr<T> Process::add_out_port(const std::string &name) {
  auto port = std::make_shared<T>(name);
  auto result = out_ports.emplace(port->name(), port);
  if (!result.second) throw std::runtime_error("duplicate output port: " + name);
  return port;
}

template <typename T>
std::shared_ptr<T> Process::get_in_port(const std::string &name) const {
  auto port_it = in_ports.find(name);
  if (port_it == in_ports.end()) throw std::runtime_error("process has no input port named " + name);
  auto port_t = std::dynamic_pointer_cast<T>(port_it->second);
  if (!port_t) throw std::runtime_error("bad port type when requesting " + name);
  return port_t;
}

template <typename T>
std::shared_ptr<T> Process::get_out_port(const std::string &name) const {
  auto port_it = out_ports.find(name);
  if (port_it == out_ports.end()) throw std::runtime_error("process has no output port named " + name);
  auto port_t = std::dynamic_pointer_cast<T>(port_it->second);
  if (!port_t) throw std::runtime_error("bad port type when requesting " + name);
  return port_t;
}

template <typename T>
bool DataPort<T>::compatible(const PortPtr &other) const {
  return static_cast<bool>(std::dynamic_pointer_cast<DataPort<T>>(other));
}

template <typename T>
template <typename U>
void DataPort<T>::set_value(U &&new_value) {
  value = std::forward<U>(new_value);
}

template <typename T>
const T &DataPort<T>::get_value() const {
  return value;
}

template <typename T>
T &DataPort<T>::get_value() {
  return value;
}

template <typename T>
void DataPort<T>::move_to(DataPortBase &other) {
  dynamic_cast<DataPort<T> &>(other).set_value(std::move(value));
}

template <typename T>
void DataPort<T>::copy_to(DataPortBase &other) {
  dynamic_cast<DataPort<T> &>(other).set_value(value);
}

template <typename T>
bool StreamPort<T>::compatible(const PortPtr &other) const {
  return static_cast<bool>(std::dynamic_pointer_cast<StreamPort<T>>(other));
}

template <typename T>
bool StreamPort<T>::available() const {
  return queue.size();
}

template <typename T>
void StreamPort<T>::push(T value) {
  if (eof_) throw std::runtime_error("push to closed queue");
  queue.push_back(std::move(value));
}

template <typename T>
T StreamPort<T>::pop() {
  if (!queue.size()) throw std::runtime_error("pop from empty queue");
  T value = std::move(queue.front());  // ?
  queue.pop_front();
  return value;
}

template <typename T>
bool StreamPort<T>::eof() {
  return !queue.size() && eof_;
}

template <typename T>
void StreamPort<T>::close() {
  eof_ = true;
}

template <typename T>
bool StreamPort<T>::eof_triggered() {
  return eof_;
}

template <typename T>
void StreamPort<T>::copy_to(StreamPortBase &other) {
  auto &other_t = dynamic_cast<StreamPort<T> &>(other);
  for (auto &item : queue) {
    other_t.push(item);
  }
  if (eof_) other_t.close();
}

template <typename T>
void StreamPort<T>::move_to(StreamPortBase &other) {
  auto &other_t = dynamic_cast<StreamPort<T> &>(other);
  for (auto &&item : queue) {
    other_t.push(std::move(item));
  }
  if (eof_) other_t.close();
}

template <typename T>
void StreamPort<T>::clear() {
  queue.clear();
}

// default buffering implementation, writing to an in-memory buffer
// TODO: make the output a shared_ptr or something so that it doesn't need to
// be copied?

namespace detail {

template <typename T>
class InMemBufferWrite : public StreamingAtomicProcess {
 public:
  InMemBufferWrite(const std::string &name)
      : StreamingAtomicProcess(name),
        in(add_in_port<StreamPort<T>>("in")),
        out(add_out_port<DataPort<std::vector<T>>>("out")) {}

  void process() override {
    while (in->available()) {
      buf.emplace_back(in->pop());
    }
  }

  void finalise() override { out->set_value(std::move(buf)); }

 private:
  StreamPortPtr<T> in;
  DataPortPtr<std::vector<T>> out;

  std::vector<T> buf;
};

template <typename T>
class InMemBufferRead : public StreamingAtomicProcess {
 public:
  InMemBufferRead(const std::string &name)
      : StreamingAtomicProcess(name),
        in(add_in_port<DataPort<std::vector<T>>>("in")),
        out(add_out_port<StreamPort<T>>("out")) {}

  void initialise() override { buf = in->get_value(); }

  void process() override {
    if (idx < buf.size()) {
      out->push(buf[idx]);
      idx++;
    } else
      out->close();
  }

 private:
  DataPortPtr<std::vector<T>> in;
  StreamPortPtr<T> out;

  std::optional<float> get_progress() override {
    if (buf.size())
      return static_cast<float>(idx) / static_cast<float>(buf.size());
    else
      return std::nullopt;
  }

  std::vector<T> buf;
  size_t idx = 0;
};

}  // namespace detail

template <typename T>
ProcessPtr StreamPort<T>::get_buffer_writer(const std::string &name) {
  return MakeBuffer<T>::get_buffer_writer(name);
}

template <typename T>
ProcessPtr StreamPort<T>::get_buffer_reader(const std::string &name) {
  return MakeBuffer<T>::get_buffer_reader(name);
}

template <typename T>
ProcessPtr MakeBuffer<T>::get_buffer_reader(const std::string &name) {
  return std::make_shared<detail::InMemBufferRead<T>>(name);
}

template <typename T>
ProcessPtr MakeBuffer<T>::get_buffer_writer(const std::string &name) {
  return std::make_shared<detail::InMemBufferWrite<T>>(name);
}

}  // namespace eat::framework
