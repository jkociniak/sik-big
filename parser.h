//
// Created by kiwipodptak on 25.05.19.
//

#ifndef SIK_DUZY_PARSER_H
#define SIK_DUZY_PARSER_H

#include "utilities.h"
#include <string>

namespace bpo = boost::program_options;

class parser {
  private:

  public:
    parser() = default;

    class parse_ex : public std::exception {
      private:
        std::string error;

      public:
        parse_ex(std::string o, std::string val);

        const char* what() const noexcept override;
    };

    class options_exception : public std::exception {
      private:
        std::stringstream desc_ss;
        std::string what_string;

      public:
        options_exception(bpo::options_description d, std::string msg);
        const char* what() const noexcept override;

        std::stringstream& get_desc();
    };

    bpo::variables_map parse_options(int argc, char* argv[]);
};

#endif //SIK_DUZY_PARSER_H
