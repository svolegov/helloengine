#include "log.h"

namespace chesseng{
void loggedcoutline(std::string s) {
    Log::log("Out: ["+s+"]");
    std::cout << s << std::endl;
}

}

