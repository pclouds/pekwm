//
// Util.hh for pekwm
// Copyright (C) 2002-2005 Claes Nasten <pekdon{@}pekdon{.}net>
//
// This program is licensed under the GNU GPL.
// See the LICENSE file for more information.
//

#include "../config.h"

#ifndef _UTIL_HH_
#define _UTIL_HH_

#include "Types.hh"

#include <string>
#include <vector>
#include <functional>
#include <sstream>

//! @brief Namespace Util used for various small file/string tasks.
namespace Util {
  void forkExec(std::string command);
  bool isFile(const std::string &file);
  bool isExecutable(const std::string &file);

  std::string getFileExt(const std::string &file);
  std::string getDir(const std::string &file);
  void expandFileName(std::string &file);
  uint splitString(const std::string &str, std::vector<std::string> &vals,
                   const char *sep, uint max = 0);
  template<class T> std::string to_string (T o_t)
    {
      std::ostringstream o_oss;
      o_oss << o_t;
      return o_oss.str ();
    }

  //! @brief Removes leading blanks( \n\t) from string.
  inline void trimLeadingBlanks(std::string &trim) {
    std::string::size_type first = trim.find_first_not_of(" \n\t");
    if ((first != std::string::npos) &&
        (first != (std::string::size_type) trim[0]))
      trim = trim.substr(first, trim.size() - first);
  }
  //! @brief Returns true if value represents true(1 or TRUE).
  inline bool isTrue(const std::string &value) {
    if (value.size() > 0)
      {
        if ((value[0] == '1') // check for 1 / 0
            || !strncasecmp(value.c_str(), "TRUE", 4))
          return true;
      }
    return false;
  }

  //! @brief for_each delete utility.
  template<class T> struct Free : public std::unary_function<T, void> {
    void operator ()(T t) { delete t; }
  };
}

#ifndef HAVE_SETENV
int setenv(const char *name, const char *value, int overwrite);
#endif // HAVE_SETENV

#endif // _UTIL_HH_
