#ifndef DEF_EXCEPTIONS_H
#define DEF_EXCEPTIONS_H

#include <stdexcept>

namespace app {

    class not_implemented_exception: public std::logic_error
    {
        public:
        not_implemented_exception(): std::logic_error{"function not implemented"} {}
        not_implemented_exception(const char* what): std::logic_error{what} {}
    };
}
#endif