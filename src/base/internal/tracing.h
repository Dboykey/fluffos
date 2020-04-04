#pragma once

#include <chrono>
#include <string>
#include <map>
#include <thread>

class TraceWriter;

class Event {
 public:
  Event();

  std::string category = "DEFAULT";
  int process_id;
  std::thread::id thread_id;
  std::string phase = "i";
  std::string name = "DEFAULT";
  std::chrono::microseconds timestamp;
  std::map<std::string, long> args;
};

class Tracer {
 public:
  static void setThreadName(std::string name);
  static void log(const Event &e);
  static void counter(std::string name, long n);
  static void counter(std::string name, const std::map<std::string, long> &things);
  static void logSimpleEvent(std::string name, std::string category = "DEFAULT");
  static void begin(std::string name, std::string category = "DEFAULT");
  static void end(std::string name, std::string category = "DEFAULT");
  static void flush(std::string filename);

 private:
  static TraceWriter &instance();
};

class ScopedTracer {
 public:
  explicit ScopedTracer(const std::string &name, const std::string &category = "DEFAULT") {
    this->_name = name;
    this->_catgeory = category;

    Tracer::begin(name, category);
  }

  ~ScopedTracer() { Tracer::end(_name, _catgeory); }

 private:
  std::string _name;
  std::string _catgeory;
};
