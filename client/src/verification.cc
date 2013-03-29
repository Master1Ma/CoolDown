#include "verification.h"
#include <Poco/SharedMemory.h>
#include <Poco/File.h>

using Poco::DigestEngine;
using Poco::SharedMemory;
using Poco::File;

namespace CoolDown{
    namespace Client{

        Verification::Verification(){
        }

        Verification::~Verification(){
        }

        string Verification::get_file_verification_code(const string& fullpath) {
            const static size_t CHUNK_SIZE = 1 << 28;
            FastMutex::ScopedLock lock(mutex_);
            File f(fullpath);
            SharedMemory sm(f, SharedMemory::AM_READ);
            engine_.update(sm.begin(), f.getSize() );
            /*
            char* start = sm.begin();
            while( start + CHUNK_SIZE < sm.end() ){
                this->calc_piece_verification_code( start, start + CHUNK_SIZE );
                start += CHUNK_SIZE;
            }

            this->calc_piece_verification_code( start, sm.end() );
            */
            return DigestEngine::digestToHex(engine_.digest());
        }

        string Verification::get_verification_code(const char* begin, const char* end) {
            FastMutex::ScopedLock lock(mutex_);
            return this->get_verification_code_without_lock(begin, end);
        }

        bool Verification::veritify(const char* begin, const char* end, const string& vc) {
            FastMutex::ScopedLock lock(mutex_);
            return vc == this->get_verification_code_without_lock(begin, end);
        }

        bool Verification::veritify(const string& source, const string& vc) {
            FastMutex::ScopedLock lock(mutex_);
            return vc == this->get_verification_code_without_lock(source.data(), source.data() + source.length());
        }

        string Verification::get_verification_code_without_lock(const char* begin, const char* end){
            if( end < begin ){
                return "";
            }
            engine_.reset();
            engine_.update(begin, end - begin);
            return DigestEngine::digestToHex(engine_.digest());
        }

        retcode_t Verification::calc_piece_verification_code(const char* begin, const char* end){
            if( end < begin ){
                return ERROR_VERIFY_INVALID_RANGE;
            }
            engine_.update(begin, end - begin);
            return ERROR_OK;
        }

    }
}
