#include "job_info.h"
#include "client.h"
#include <set>
#include <algorithm>
#include <iterator>
#include <Poco/Logger.h>
#include <Poco/Util/Application.h>
#include <Poco/Bugcheck.h>
#include <Poco/Exception.h>
#include <boost/foreach.hpp>

using std::set;
using std::back_inserter;
using Poco::Logger;
using Poco::Util::Application;
using Poco::Exception;


namespace CoolDown{
    namespace Client{
        namespace{
            //string retrieve_fileid(const Torrent::File& file){
            //    return file.checksum();
            //}

            //int retrieve_chunk_size(const Torrent::File& file){
            //    return file.chunk().size();
            //}
        }

        //FileIdentityInfo
        FileIdentityInfo::FileIdentityInfo(){
        }

        FileIdentityInfo::FileIdentityInfo(const string& relative_path, const string& filename)
            :relative_path(relative_path), filename(filename){
            }

        bool operator== (const FileIdentityInfo& lhs, const FileIdentityInfo& rhs){
            return lhs.relative_path == rhs.relative_path 
                   && lhs.filename == rhs.filename;
        }

        //LocalFileInfo
        LocalFileInfo::LocalFileInfo(const string& top_path)
        :top_path_(top_path){
        }

        retcode_t LocalFileInfo::add_file(const string& fileid, const string& relative_path, const string& filename, Int64 filesize){
            FastMutex::ScopedLock lock(mutex_);
            Logger& logger_ = Application::instance().logger();
            try{
                poco_debug_f4(logger_, "Call add_file with fileid '%s', relative_path : '%s', filename : '%s', filesize : '%Ld'",
                        fileid, relative_path, filename, filesize);
                //make sure we don't add a file twice
                //map<string, FilePtr>::iterator iter = files.find(fileid);
                //a torrent may contain the same file servral times, so this assert is wrong.
                //poco_assert( iter == files.end() );

                string filepath = Poco::format("%s%s%s", top_path_, relative_path, filename);
                StringList& same_files_path_list = same_files_[fileid];
                same_files_path_list.push_back(filepath);

                if( same_files_path_list.size() > 1 ){
                    poco_notice_f2(logger_, "Find same file of path '%s' and path '%s'", same_files_[fileid][0], filepath);
                    return ERROR_OK;
                }

                File dir( top_path_ + relative_path );
                dir.createDirectories();

                files[fileid] = FilePtr( new File(filepath) );
                poco_debug_f1(logger_, "in LocalFileInfo::add_file, filepath : %s", filepath);

                if( files[fileid]->exists() == false ){
                    bool create_file = files[fileid]->createFile();
                    if( true == create_file ){
                        //we just create the file, so set the filesize to the right one
                        files[fileid]->setSize(filesize);
                    }else{
                        //we have that file in our disk, so don't truncate that file.
                    }

                }else{
                    //poco_debug_f3(logger_, "file already exists, fileid '%s', relative_path '%s', filename '%s'",
                    //        fileid, relative_path, filename);
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
        
        bool LocalFileInfo::has_local_file(const string& fileid, const string& relative_path, const string& filename){
            FastMutex::ScopedLock lock(mutex_);
            if( files.end() == files.find(fileid) ){
                return false;
            }else{
                StringList& path_list = same_files_[fileid];
                return find(path_list.begin(), path_list.end(), top_path_ + relative_path + filename ) != path_list.end() ;
            }
        }

        bool LocalFileInfo::has_file(const string& fileid){
            FastMutex::ScopedLock lock(mutex_);
            return files.end() != files.find(fileid);
        }

        map<string, StringList>& LocalFileInfo::same_files_map(){
            return this->same_files_;
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
                fileMap_[file.checksum()].push_back( TorrentFileInfoPtr(new TorrentFileInfo(file) ) );
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
        
        //const TorrentFileInfoPtr& TorrentInfo::get_file(const string& fileid, const string& relative_path, const string& filename){
        //}

        const TorrentFileInfoPtr& TorrentInfo::get_one_file_of_same_fileid(const string& fileid){
            file_map_t::iterator iter = fileMap_.find(fileid);
            poco_assert( iter != fileMap_.end() );
            poco_assert( iter->second.size() >= 1 );
            return iter->second.at(0);
        }

        string TorrentInfo::tracker_address() const{
            return torrent_.trackeraddress();
        }

        string TorrentInfo::torrentid() const{
            return torrent_.torrentid();
        }

        //DownloadInfo
        DownloadInfo::DownloadInfo()
        :is_job_removed(false),
        is_stopped(true),
        is_download_paused(true),
        is_upload_paused(true),
        //is_download_finished(false),
        bytes_upload_this_second(0),
        bytes_download_this_second(0),
        upload_speed_limit(1<<30),
        download_speed_limit(1<<30),
        time_to_next_report(DownloadInfo::report_period)    //report progress to tracker every 30 seconds
        {
        }

        //JobInfo
        JobInfo::JobInfo(const Torrent::Torrent& torrent, const string& top_path, const FileIdentityInfoList& needs)
        :app_(dynamic_cast<CoolClient&>(Application::instance()) ),
         logger_(app_.logger()),
         localFileInfo(top_path),
         torrentInfo(torrent)
        {
            fileidlist_.reserve( torrent.file().size() );
            for(int pos = 0; pos != torrent.file().size(); ++pos){
                const Torrent::File& file = torrent.file().Get(pos);
                int chunk_size = file.chunk().size();
                string fileid( file.checksum() );
                string relative_path( file.relativepath() );
                string filename( file.filename() );

                FileIdentityInfo id_info(relative_path, filename);
                if( needs.end() == find( needs.begin(), needs.end(), id_info ) ){
                    //we donot need this file
                    continue;
                }

                Int64 filesize( file.size() );

                poco_debug_f2(logger_, "add file to Job, fileid : '%s', name : '%s'", fileid, filename );
                downloadInfo.percentage_map[fileid] = 0;
                downloadInfo.bitmap_map[fileid] = new file_bitmap_t( chunk_size, 0 );
                localFileInfo.add_file(fileid, relative_path, filename, filesize);
                fileidlist_.push_back(fileid);
            }
        }

        JobInfo::~JobInfo(){
        }

        string JobInfo::clientid() const{
            return app_.clientid();
        }

        const StringList& JobInfo::fileidlist() const{
            return this->fileidlist_;
        }

        StringList JobInfo::UniqueFileidList() const{
            set<string> unique_fileids(this->fileidlist_.begin(), this->fileidlist_.end());
            return StringList(unique_fileids.begin(), unique_fileids.end());
        }

    }
}
