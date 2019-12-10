#ifndef FATAL_ERR
#define FATAL_ERR

#include <string>
#include <exception>
#include <stdexcept>
#include <iostream>
#include "strfmt.h"

class sigsegv_exception: public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "SIGSEGV, possible access to unlocked vm.";
    }
};

class fatal_exception : public std::runtime_error
{
public:
    explicit fatal_exception(const std::string& msg)
        : std::runtime_error(msg)
    {
        std::cerr << "FATAL_RISE: " << msg << std::endl;
    }
};

[[ noreturn ]] void inline fatal_func_handler(const std::string& text, const char* file, const int line)
{
    throw fatal_exception(stringfmt("%s\n\tat %s:%i", text, file, line));
}
#define FATAL_RISE(TEXT) fatal_func_handler(TEXT, __FILE__, __LINE__)

#endif // FATAL_ERR

