// -----------------------------------------------------------------------------
// MadModem MSHV Boost subset compatibility shim
// -----------------------------------------------------------------------------
// The MSHV MSK144 generator vendors a very small, old Boost subset flattened
// under third_party/mshv_gpl/port/boost. AppleClang is detected by the bundled
// select_compiler_config.hpp as boost/config/compiler/clang.hpp, but that file
// was not present in the subset. For the Boost.CRC code used here, Clang can be
// treated as GCC-compatible, which is exactly what this wrapper does.
#ifndef MADMODEM_MSHV_BOOST_CONFIG_COMPILER_CLANG_HPP
#define MADMODEM_MSHV_BOOST_CONFIG_COMPILER_CLANG_HPP

#include "../../gcc.hpp"

#ifndef BOOST_COMPILER
#  define BOOST_COMPILER "Clang compiler"
#endif

#endif // MADMODEM_MSHV_BOOST_CONFIG_COMPILER_CLANG_HPP
