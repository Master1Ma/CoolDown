#include "verification.h"
#include <Poco/SharedMemory.h>
#include <Poco/File.h>
#include <Poco/Bugcheck.h>

using Poco::DigestEngine;
using Poco::SharedMemory;
using Poco::File;

namespace CoolDown{
    namespace Client{

        Verification::Verification(){
        }

        Verification::~Verification(){
        }

        FastMutex Verification::mutex_;
        SHA1Engine Verification::engine_;

        string Verification::get_file_verification_code(const string& fullpath) {
            FastMutex::ScopedLock lock(mutex_);
            File f(fullpath);
            SharedMemory sm(f, SharedMemory::AM_READ);
            engine_.update(sm.begin(), f.getSize() );
            /*
            const static size_t CHUNK_SIZE = 1 << 28;
            char* start = sm.begin();
            while( start + CHUNK_SIZE < sm.end() ){
                this->calc_piece_verification_code( start, start + CHUNK_SIZE );
                start += CHUNK_SIZE;
            }

            this->calc_piece_verification_code( start, sm.end() );
            */
            return DigestEngine::digestToHex(engine_.digest());
        }
        void Verification::get_file_checksum_list(const File& file, int chunk_size, ChecksumList* pList){
            poco_assert( chunk_size > 0 );
            poco_assert( pList != NULL );
            poco_assert( file.exists() );

            FastMutex::ScopedLock lock(mutex_);
            SharedMemory sm(file, SharedMemory::AM_READ);
            char* start = sm.begin();
            while( start + chunk_size < sm.end() ){
                pList->push_back( get_verification_code_without_lock(start, start + chunk_size ) );
                start += chunk_size;
            }
            pList->push_back( get_verification_code_without_lock(start, sm.end()) );
        }

        string Verification::get_verification_code(const char* begin, const char* end) {
            FastMutex::ScopedLock lock(mutex_);
            return Verification::get_verification_code_without_lock(begin, end);
        }
        string Verification::get_verification_code(const string& content){
            FastMutex::ScopedLock lock(mutex_);
            return Verification::get_verification_code_without_lock(content.data(), content.data()+content.length());
        }

        bool Verification::veritify(const char* begin, const char* end, const string& vc) {
            FastMutex::ScopedLock lock(mutex_);
            return vc == Verification::get_verification_code_without_lock(begin, end);
        }

        bool Verification::veritify(const string& source, const string& vc) {
            FastMutex::ScopedLock lock(mutex_);
            return vc == Verification::get_verification_code_without_lock(source.data(), source.data() + source.length());
        }

        string Verification::get_verification_code_without_lock(const char* begin, const char* end){
            poco_assert( end >= begin );
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
