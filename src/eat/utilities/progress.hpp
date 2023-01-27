#pragma once

#include <cmath>
#include <iostream>
#include <string>

#ifdef WIN32
#define NOMINMAX
#include <windows.h>
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

namespace eat::utilities {

/// allows printing a multi-line string which can be overwritten on subsequent invocations
///
/// this makes it easy to build things like progress bars -- just build them a
/// string and don't worry about how to re-draw it
class RefreshWindow {
 public:
#ifdef WIN32
  RefreshWindow() {
    HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(handleOut, &consoleMode);
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    consoleMode |= DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(handleOut, consoleMode);
  }
#endif

  void print(const std::string &msg) {
    if (msg == last_msg) return;

    if (last_msg.size() > 0) {
      std::cout << "\033[1G";  // go to the start of the current line
      std::cout << "\033[0K";  // clear to the end
      for (char c : last_msg)
        if (c == '\n') {
          std::cout << "\033[1A";  // up a line
          std::cout << "\033[0K";  // clear to the end
        }
    }

    std::cout << msg;

    last_msg = msg;

    std::cout.flush();
  }

 private:
  std::string last_msg;
};

/// format a simple progress bar to out with a given width and a progress in
/// the range 0-1
inline void format_bar(std::string &out, int width, float progress) {
  auto bars = std::lround(static_cast<float>(width) * progress);
  auto percent = std::lround(100.0f * progress);

  out += '[';
  for (decltype(bars) i = 0; i < width; i++) out += i < bars ? '|' : ' ';
  out += ']';
  out += ' ';
  out += std::to_string(percent);
  out += '%';
}

/// write an overall and current task progress to out
inline void format_progress(std::string &out, int width, float overall_progress, const std::string &current_task,
                            float task_progress) {
  // all tasks:    [||||      ] 30%
  // current task: render
  // current task: [||||||    ] 50%

  out.clear();
  out += "all tasks:    ";
  format_bar(out, width, overall_progress);
  out += "\ncurrent task: ";
  out += current_task;
  out += "\ncurrent task: ";
  format_bar(out, width, task_progress);
  out += "\n";
}

}  // namespace eat::utilities
