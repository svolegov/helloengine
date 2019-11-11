#pragma once

#include <string>
#include <iostream>
#include <fstream>

namespace chesseng {
class Log {

  public:
  static void log(std::string s) {
    filelog() << s << std::endl;
  }

  static void logAndPrint(std::string s) {
    std::cout << s << std::endl;
    log(s);
  }

  private:
    static std::ofstream& filelog() {
      static std::ofstream outfile ("out.txt");
      return outfile;
    }
};

void loggedcoutline(std::string s);

}
