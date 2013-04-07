#ifndef VERIFICATION_H
#define VERIFICATION_H

#include "error_code.h"
#include <string>
#include <Poco/Mutex.h>
#include <Poco/SHA1Engine.h>

using std::string;
using Poco::FastMutex;
using Poco::SHA1Engine;

namespace CoolDown{
    namespace Client{

        class Verification{
            public:
                static string get_file_verification_code(const string& fullpath) ;
                static string get_verification_code(const string& content) ;
                static string get_verification_code(const char* begin, const char* end) ;
                static bool veritify(const char* begin, const char* end, const string& vc) ;
                static bool veritify(const string& source, const string& vc) ;

            private:
                static string get_verification_code_without_lock(const char* begin, const char* end);
                static retcode_t calc_piece_verification_code(const char* begin, const char* end);
                static FastMutex mutex_;
                static SHA1Engine engine_;

                Verification();
                ~Verification();
        };
    }
}

#endif
