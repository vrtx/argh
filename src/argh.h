#pragma once
// argh: A simple, header-only library to parse command line arguments into a any C/C++
//       struct. Supports boolean flags, integers, floats, strings. The command line syntax
//       seeks to be reasonably similar to common command line apps.
//
// Command Line Syntax:
//   Argument Names:
//     Single char "key";        e.g. -f
//     Full name;                e.g. --foo
//     Concatenated flag keys;   e.g. -asdf
//   Argument Values:
//     Deliniated from argument name by space or "="; e.g. -f 1 or -f=1
//     Supports boolean flags (no value parameter)
//     Supports integers, floating point values and strings
//   Remainder Value:
//     Everything after the arguments is considered the remainder
//     Usefule for specifying unnamed parameters; e.g. ./foo infile
//     Note that all arguments must be specified before the remainder
//
// Example Command Line:
//
//   ./foo -dvi /input/file -t=/tmp/path/ --rate 0.9 /output/file
//      Set the boolean flags d and v to true
//      Sets 'i' to "/input/file"
//      Sets 't' to "/temp/file"
//      Sets 'rate' to 0.9
//      Sets the remainder to "/output/file"
//
// Example Program:
//
//   // Declare the output struct as a global (global not required; can be local, extern, etc.)
//   struct Options {
//     const char* infile;
//     const char* tmppath;
//     const char* outfile;
//     bool debug;
//     bool verbose;
//   } gOptions;
//  
//   int main(int argc, char *argv[]) {
//     auto args = argh::Args(gOptions, argc, argv);
//     args.arg(&Options::infile,  'i', "input",   "Specify the input file",   "./in.foo");
//     args.arg(&Options::tmpfile, 't', "temp",    "Path for temporary files", "/tmp/");
//     args.arg(&Options::debug,   'd', "debug",   "Start in daemon mode");
//     args.arg(&Options::verbose, 'v', "verbose", "Level of verbosity");
//     args.remainder(&Options::outfile, "output path");
//     if (!args.parse()) {
//         // parse failed
//          cout << args.errors() << endl;
//          cout << args.help() << endl;
//     }
//     // gOptions is now set to command line values or defaults values. Checks for
//     // required/conflicting arguments or value ranges should generally be done here.
//     return 0;
//   }
//
// Notes:
//   Does not support "argument counting" (e.g. -vvvvv)
#include <sstream>
#include <iostream>
#include <cstdint>
#include <utility>
#include <string>
#include <vector>
#include <map>

namespace argh {

    const int kMaxParams = 256;

    struct ParseError {
        const char* desc;
        const char* source;
        const char* details;
    };

    // Template specializations for string conversion functions
    template<typename T> T fromStr(const char* s)         { return T(s); }
    template<> float       fromStr<float>(const char* s)  { return std::stof(s); }
    template<> double      fromStr<double>(const char* s) { return std::stod(s); }
    template<> int         fromStr<int>(const char* s)    { return std::stoi(s); }
    template<> bool        fromStr<bool>(const char* s)   { return true; }
    template<> const char* fromStr<const char*>(const char* s) { return s; }
    template<> std::string fromStr<std::string>(const char* s) { return s; }
    // TODO: finish sto*

    // Base class represnting a single parameter's info (excluding type/storage info)
    class ParamInfo {
    public:
        ParamInfo(const char key, const std::string name)
                : key_(key), name_(name), isSet_(false) { }
        virtual ~ParamInfo() { }
        virtual char key() const { return key_; }
        virtual bool isSet() const { return isSet_; }
        virtual std::string name() const { return name_; }
        virtual std::string help() const = 0;
        virtual std::string usage() const = 0;
        virtual std::string defaultStr() const = 0;
        virtual bool parseValue(const char* val) = 0;
    private:
        char key_;
        std::string name_;
        bool isSet_;
    };

    // Represents a single parameter (including type)
    template <typename ValueT>
    class Parameter : public ParamInfo {
    public:
        // Initialize a named parameter with default value
        Parameter(ValueT* output,       // pointer to a struct member or variable for storage/output
                  char key,                 // -c character
                  const std::string name,   // --name[=...] name
                  const std::string help,   // text description of the parameter  
                  const ValueT defVal) :    // default value (copied to cfg_->*field member)
            ParamInfo(key, name), output_(output), defVal_(defVal), help_(help), hasDef_(true), parseErr_{nullptr, nullptr, nullptr} { }

        // Initialize a named parameter without a default value
        Parameter(ValueT* output,     // pointer to member field for storage/output
                  char key,                 // -c character
                  const std::string name,   // --name[=...] name
                  const std::string help) : // text description of the parameter
            ParamInfo(key, name), output_(output), defVal_(), help_(help), hasDef_(false), parseErr_{nullptr, nullptr, nullptr} { }

        ~Parameter() { }

        virtual std::string usage() const { 
            std::stringstream ss;
            ss << ParamInfo::key();
            return ss.str();
        }

        virtual std::string defaultStr() const {
            std::stringstream ss;
            ss << defVal_;
            return ss.str();
        }

        virtual std::string help() const {
            std::stringstream ss, is, fs, vs;
            fs << " -" << ParamInfo::key();
            is << "  --" << ParamInfo::name();
            if (hasDef_)
              vs << "[default: " << defaultStr() << "] ";
            ss << std::left;
            ss.width(5);
            ss << fs.str();
            ss.width(14);
            ss << is.str();
            ss.width(24);
            ss << vs.str();
            ss.width();
            ss << std::left << help_ << std::endl;
            return ss.str();
        }

        virtual bool parseValue(const char* valStr) {
            try {
                (*output_) = fromStr<ValueT>(valStr);
            } catch(const std::invalid_argument& ia) {
                parseErr_.desc = "Invalid Argument";
                parseErr_.source = valStr;
                parseErr_.details = ia.what();
                return false;
            } catch(const std::out_of_range& oor) {
                parseErr_.desc = "Value Out of Range";
                parseErr_.source = valStr;
                parseErr_.details = oor.what();
                return false;
            }
            return true;
        }

    private:
        ValueT* output_;
        ValueT defVal_;
        std::string help_;
        bool hasDef_;
        ParseError parseErr_;
    };

    class Parser {
    public:
        Parser(int argc, const char* argv[]) : argc_(argc), argv_(argv), params_{nullptr} { }

        template <typename ArgT>
        void addParam(Parameter<ArgT>* param) {
            params_[(int)param->key()] = param;
        }

        bool parse() {
            auto param = params_[(int)argv_[1][0]];
            if (param->parseValue(argv_[2]))
                std::cout << " Parsed '" << argv_[1][0] << std::endl;

            // TODO

            return false;
        }

        class ParseError {
        public:
            ParseError(std::string description, const int srcV) :
                    desc(description), srcArgv(srcV) { }
            ParseError(std::string description) :
                    desc(description) { }
            std::string desc;   // Description of the error
            int srcArgv;        // argv containing the error
            ParamInfo* param;   // [optional: related parameter(s)]
        };

        std::string errors() const {
            std::string out("Error: ");
            if (errors_.empty()) return "";
            for (auto &e : errors_) {
                out.append(e.desc);
                if (e.srcArgv < argc_) { // sanity check
                    out.append(" @ [");
                    out.append(argv_[e.srcArgv]);
                    out.append("]");
                }
            }
            return out;
        } 

    private:
        int argc_;              // Command line parameter count
        const char** argv_;     // Command line parameter vector
        // int remainderIdx_;    // argv[] index for input after arguments
        std::vector<ParseError> errors_;    // errors encountered while parsing the command line
    public:
        ParamInfo* params_[kMaxParams];     // index of parameters by single-letter key 
    };

    class Args {
    public:
        Args(int argc, const char* argv[]) : parser_(argc, argv) {
            if (argc) procName_ = argv[0];
        }
        ~Args() { }

        // add a named argument with default value
        template <typename ArgT>
        void arg(ArgT* field,
                 const char key,
                 const char* name,
                 const char* help,
                 ArgT defVal) {
            auto p = new Parameter<ArgT>(field, key, name, help, defVal);
            parser_.addParam<ArgT>(p);
        }

        // add a named argument without default value
        template <typename ArgT>
        void arg(ArgT* field,
                 const char key,
                 const char* name,
                 const char* help) {
            auto p = new Parameter<ArgT>(field, key, name, help);
            parser_.addParam<ArgT>(p);
        }

        // add a helpful name for trailing arguments
        void remainder(const char* name) {
            remainderName_ = name;
        }

        // parse the command line input
        bool parse() {
            return parser_.parse();
        }


        // abbreviated usage details
        std::string usage() const {
            std::stringstream usage;
            usage << "Usage: " << procName_ << " -";
            for (const auto &p : parser_.params_)
                if (p) usage << p->key();
            usage << " <" << remainderName_ << ">";
            std::string s;
            s.operator basic_string_view();
            return usage.str();
        }

        // full usage details
        std::string help() {
            std::stringstream details;
            details << usage() << std::endl;
            for (const auto &p : parser_.params_)
                if (p) details << p->help();
            details << std::endl;
            return details.str();
        }

        Parser parser_;
    private:
        std::string remainderName_;     // logical name for the remainder arg(s)
        std::string procName_;          // name of process (argv[0] for now)
    };

} // namespace
