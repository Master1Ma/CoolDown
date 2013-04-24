#include "job_info.h"
#include "client.h"
#include <algorithm>
#include <iterator>
#include <Poco/Logger.h>
#include <Poco/Util/Application.h>
#include <Poco/Bugcheck.h>
#include <Poco/Exception.h>

using std::back_inserter;
using Poco::Logger;
using Poco::Util::Application;
using Poco::Exception;


namespace CoolDown{
    namespace Client{
        namespace{
            string retrieve_fileid(const Torrent::File& file){
                return file.checksum();
            }

            int retrieve_chunk_size(const Torrent::File& file){
                return file.chunk().size();
            }
        }

        //LocalFileInfo
        LocalFileInfo::LocalFileInfo(const string& top_path)
        :top_path(top_path){
        }

        retcode_t LocalFileInfo::add_file(const string& fileid, const string& relative_path, const string& filename, Int64 filesize){
            FastMutex::ScopedLock lock(mutex_);
            Logger& logger_ = Application::instance().logger();
            try{
                //make sure we don't add a file twice
                map<string, FilePtr>::iterator iter = files.find(fileid);
                poco_assert( iter == files.end() );

                string filepath = top_path + relative_path + filename;
                File dir( top_path + relative_path );
                dir.createDirectories();

                files[fileid] = FilePtr( new File(filepath) );
                poco_debug_f1(logger_, "filepath : %s", filepath);

                if( files[fileid]->exists() == false ){
                    bool create_file = files[fileid]->createFile();
                    if( true == create_file ){
                        //we just create the file, so set the filesize to the right one
                        files[fileid]->setSize(filesize);
                    }else{
                        //we have that file in our disk, so don't truncate that file.
                    }

                    poco_debug_f1(logger_, "createDirectory returns %d", (int)create_file);
                }else{
                    poco_debug_f3(logger_, "file already exists, fileid '%s', relative_path '%s', filename '%s'",
                            fileid, relative_path, filename);
                }
            }catch(Exception& e){
                Application::instance().logger().warning( Poco::format("Got exception while LocalFileInfo::add_file, fileid '%s'"
                            "relative_path '%s' filename '%s'", fileid, relative_path, filename));
                return ERROR_UNKNOWN;
            }
            return ERROR_OK;
        }

        FilePtr LocalFileInfo::get_file(const string& fileid){
            FastMutex::ScopedLock lock(mutex_);
            FilePtr res;
            map<string, FilePtr>::iterator iter = files.find(fileid);
            if( iter != files.end() ){
                res = iter->second;
            }
            return res;
        }
        bool LocalFileInfo::has_file(const string& fileid){
            FastMutex::ScopedLock lock(mutex_);
            return files.end() != files.find(fileid);
        }

        

        //TorrentFileInfo 
        TorrentFileInfo::TorrentFileInfo(const Torrent::File& file)
        :file_(file), 
        chunk_count_(file_.chunk().size()), 
        fileid_(file_.checksum()){
        }

        TorrentFileInfo::~TorrentFileInfo(){
        }

        int TorrentFileInfo::chunk_count() const{
            return this->chunk_count_;
        }

        uint64_t TorrentFileInfo::size() const{
            return file_.size();
        }

        string TorrentFileInfo::checksum() const{
            return file_.checksum();
        }

        string TorrentFileInfo::fileid() const{
            return this->fileid_;
        }

        string TorrentFileInfo::relative_path() const{
            return file_.relativepath();
        }

        string TorrentFileInfo::filename() const{
            return file_.filename();
        }

        string TorrentFileInfo::chunk_checksum(int chunk_pos) const{
            if( chunk_pos >= chunk_count_ ){
                return "";
            }
            return file_.chunk().Get(chunk_pos).checksum();

        }
        uint64_t TorrentFileInfo::chunk_offset(int chunk_pos) const{
            if( chunk_pos >= chunk_count_ ){
                return 0;
            }
            uint64_t chunk_size = file_.chunk().Get(0).size();
            return chunk_size * chunk_pos;
        }


        int TorrentFileInfo::chunk_size(int chunk_pos) const{
            if( chunk_pos >= chunk_count_ ){
                return -1;
            }
            return file_.chunk().Get(chunk_pos).size();
        }


        //TorrentInfo
        TorrentInfo::TorrentInfo(const Torrent::Torrent& torrent)
        :torrent_(torrent), 
        file_count_(torrent_.file().size()){
            for(int pos = 0; pos != torrent_.file().size(); ++pos){
                const Torrent::File& file = torrent_.file().Get(pos);
                fileMap_[file.checksum()] = TorrentFileInfoPtr(new TorrentFileInfo(file) );
            }
        }

        TorrentInfo::~TorrentInfo(){
        }

        const TorrentInfo::file_map_t& TorrentInfo::get_file_map() const{
            return this->fileMap_;
        }
        
        int TorrentInfo::get_file_count() const{
            return this->file_count_;
        }
        
        const TorrentFileInfoPtr& TorrentInfo::get_file(const string& fileid){
            map<string, TorrentFileInfoPtr>::iterator iter = fileMap_.find(fileid);
            poco_assert( iter != fileMap_.end() );
            return iter->second;
        }

        string TorrentInfo::tracker_address() const{
            return torrent_.trackeraddress();
        }

        //DownloadInfo
        DownloadInfo::DownloadInfo()
        :is_finished(false),
        is_download_paused(true),
        is_upload_paused(true),
        upload_speed_limit(1<<31),
        download_speed_limit(1<<31)
        {
        }

        //JobInfo
        JobInfo::JobInfo(const Torrent::Torrent& torrent, const string& top_path)
        :app_(dynamic_cast<CoolClient&>(Application::instance()) ),
         logger_(app_.logger()),
         localFileInfo(top_path),
         torrentInfo(torrent)
        {
            vector<int> chunk_size_list;
            transform(torrent.file().begin(), torrent.file().end(), back_inserter(chunk_size_list), retrieve_chunk_size);
            transform(torrent.file().begin(), torrent.file().end(), back_inserter(fileidlist_), retrieve_fileid);
            for(int i = 0; i != fileidlist_.size(); ++i){
                string fileid( fileidlist_[i] );
                poco_debug_f1(logger_, "add '%s' to Job", fileid);
                downloadInfo.percentage_map[fileid] = 0;
                downloadInfo.bitmap_map[fileid] = new file_bitmap_t( chunk_size_list[i], 0 );
            }
        }

        JobInfo::~JobInfo(){
        }

        string JobInfo::clientid() const{
            return app_.clientid();
        }

        const vector<string>& JobInfo::fileidlist() const{
            return this->fileidlist_;
        }

    }
}
