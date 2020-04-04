// Yucong Sun (sunyucong@gmail.com)
//
// Generate driver tracing data to be viewed in chrome http://about:tracing

#include <string>
#include <thread>
#include <map>
#include <iostream>
#include <fstream>
#include <mutex>
#include <deque>
#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
#include "tracing.h"

#include "base/internal/log.h"

namespace {
const int _current_process_id =
#ifdef _WIN32
    GetCurrentProcessId();
#else
    ::getpid();
#endif
}  // namespace

Event::Event()
    : process_id(::_current_process_id),
      thread_id(std::this_thread::get_id()),
      timestamp(std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())) {}

class TraceWriter {
 public:
  ~TraceWriter();
  void log(Event e);
  void flush(std::string file);

 private:
  std::mutex lock;
  std::deque<Event> buffer;

  class JsonItem {
   public:
    void addPair(std::string key, const char *value);
    void addPair(std::string key, const std::string &value);
    void addPair(std::string key, const JsonItem &item);
    template <typename T>
    void addPair(std::string key, T value);
    std::string getString() const { return json + "}"; }

   private:
    bool empty = true;
    std::string json = "{";
    void prependCommaIfNeeded();
  };
};

template <typename T>
void TraceWriter::JsonItem::addPair(std::string key, T value) {
  std::ostringstream ss;
  ss << value;

  prependCommaIfNeeded();
  json += "\"" + key + "\": " + ss.str();
}

TraceWriter::~TraceWriter() {
  std::lock_guard<std::mutex> _lock(lock);
  if (!buffer.empty()) {
    debug_message("Uncollected profiling events: %d", buffer.size());
  }
}

void TraceWriter::flush(std::string filename) {
  std::ofstream file;
  file.open(filename);

  file << "[";
  {
    std::lock_guard<std::mutex> _guard(lock);

    debug_message("Dumping %d events to %s.\n", buffer.size(), filename.c_str());

    bool is_first = true;

    for (auto &e : buffer) {
      if (is_first)
        is_first = false;
      else
        file << ",";
      file << std::endl;

      JsonItem item;
      item.addPair("cat", e.category);
      item.addPair("pid", e.process_id);
      item.addPair("tid", e.thread_id);
      item.addPair("ts", e.timestamp.count());
      item.addPair("ph", e.phase);
      item.addPair("name", e.name);
      JsonItem args;
      for (auto &arg : e.args) {
        args.addPair(arg.first, arg.second);
      }
      item.addPair("args", args);

      file << item.getString();
    }

    buffer.clear();
    buffer.shrink_to_fit();
  }

  file << std::endl << "]";

  file.close();
}

void TraceWriter::log(Event e) {
  std::lock_guard<std::mutex> _guard(lock);

  buffer.emplace_back(e);
}

void TraceWriter::JsonItem::addPair(std::string key, const char *value) {
  prependCommaIfNeeded();
  json += "\"" + key + "\": \"" + value + "\"";
}

void TraceWriter::JsonItem::prependCommaIfNeeded() {
  if (empty)
    empty = false;
  else
    json += ", ";
}

void TraceWriter::JsonItem::addPair(std::string key, const std::string &value) {
  prependCommaIfNeeded();
  json += "\"" + key + "\": \"" + value + "\"";
}

void TraceWriter::JsonItem::addPair(std::string key, const TraceWriter::JsonItem &item) {
  prependCommaIfNeeded();
  json += "\"" + key + "\": " + item.getString();
}

void Tracer::log(const Event &e) { instance().log(e); }

TraceWriter &Tracer::instance() {
  static TraceWriter _instance;
  return _instance;
}

void Tracer::logSimpleEvent(std::string name, std::string category) {
  Event e;
  e.category = category;
  e.name = name;
  e.phase = "i";
  log(e);
}
void Tracer::begin(std::string name, std::string category) {
  Event e;
  e.name = name;
  e.category = category;
  e.phase = "B";
  log(e);
}
void Tracer::end(std::string name, std::string category) {
  Event e;
  e.name = name;
  e.category = category;
  e.phase = "E";
  log(e);
}

void Tracer::setThreadName(std::string name) {
  Event e;

  e.name = "thread_name";
  e.category = "DEFAULT";
  e.phase = "M";
  e.timestamp = std::chrono::milliseconds(0);

  log(e);
}

void Tracer::counter(std::string name, long n) { counter(name, {{name, n}}); }

void Tracer::counter(std::string name, const std::map<std::string, long> &things) {
  Event e;
  e.name = name;
  e.category = "category";
  e.phase = "C";
  e.args = things;
  log(e);
}

void Tracer::flush(std::string filename) { instance().flush(std::move(filename)); }
