#pragma once

#include "process.hpp"

namespace eat::framework {

/// process with an output port that is set to a value provided in the constructor or set_value
///
/// ports:
/// - out (`DataPort<T>`) : output data
template <typename T>
class DataSource : public FunctionalAtomicProcess {
 public:
  DataSource(const std::string &name, T value)
      : FunctionalAtomicProcess(name),
        value_(std::move(value)),
        initialised(true),
        out(add_out_port<DataPort<T>>("out")) {}

  explicit DataSource(const std::string &name)
      : FunctionalAtomicProcess(name), initialised(false), out(add_out_port<DataPort<T>>("out")) {}

  void set_value(T value) {
    value_ = std::move(value);
    initialised = true;
  }

  void process() override {
    if (!initialised) throw std::runtime_error("value has not been set");
    out->set_value(std::move(value_));
    initialised = false;
  }

 private:
  T value_;
  bool initialised;

  DataPortPtr<T> out;
};

/// process with an input port whose value is saved
///
/// ports:
/// - in (`DataPort<T>`) : input data, accessible with get_value
template <typename T>
class DataSink : public FunctionalAtomicProcess {
 public:
  DataSink(const std::string &name) : FunctionalAtomicProcess(name), in(add_in_port<DataPort<T>>("in")) {}

  T &get_value() {
    if (!initialised) throw std::runtime_error("value has not been set");
    return value_;
  }

  void process() override {
    value_ = std::move(in->get_value());
    initialised = true;
  }

 private:
  T value_;
  bool initialised = false;

  DataPortPtr<T> in;
};

/// process with an input port whose value is discarded
///
/// ports:
/// - in (`DataPort<T>`) : input data to discard
template <typename T>
class NullSink : public FunctionalAtomicProcess {
 public:
  NullSink(const std::string &name) : FunctionalAtomicProcess(name), in(add_in_port<DataPort<T>>("in")) {}

  void process() override {
    T value = std::move(in->get_value());
    (void)value;
  }

 private:
  DataPortPtr<T> in;
};

}  // namespace eat::framework
