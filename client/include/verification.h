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
                Verification();
                ~Verification();
                string get_file_verification_code(const string& fullpath) ;
                string get_verification_code(const char* begin, const char* end) ;
                bool veritify(const char* begin, const char* end, const string& vc) ;
                bool veritify(const string& source, const string& vc) ;

            private:
                string get_verification_code_without_lock(const char* begin, const char* end);
                retcode_t calc_piece_verification_code(const char* begin, const char* end);
                FastMutex mutex_;
                SHA1Engine engine_;
        };
    }
}

#endif
