#ifndef CLIENT_H
#define CLIENT_H

#include "error_code.h"
#include "local_sock_manager.h"
#include "net_task_manager.h"
#include "job_info.h"
#include <vector>
#include <string>
#include <Poco/Util/Application.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Path.h>
#include <Poco/File.h>
#include <Poco/Types.h>
#include <Poco/ThreadPool.h>
#include <google/protobuf/message.h>
using google::protobuf::Message;

using std::vector;
using std::string;
using Poco::Util::Application;
using Poco::Util::ServerApplication;
using Poco::Path;
using Poco::File;
using Poco::Int32;
using Poco::Int64;
using Poco::TaskManager;
using Poco::ThreadPool;

namespace Torrent{
    class Torrent;
}

namespace ClientProto{
    class ShakeHand;
}

namespace CoolDown{
    namespace Client{

            class Job;
            class CoolClient : public ServerApplication{
                public:

                    CoolClient();
                    enum {
                        TRACKER_PORT = 9977,
                    };
                    typedef vector<string> StringList;
                    typedef StringList ClientIdCollection;
                    typedef vector<File> FileList;
                    typedef LocalSockManager::LocalSockManagerPtr LocalSockManagerPtr;
                    typedef int make_torrent_progress_callback_t;
                    typedef SharedPtr<Job> JobPtr;

                    //Application operations
                    void initialize(Application& self);
                    void uninitialize();
                    int main(const vector<string>& args);

                    //NetTaskManager& download_manager();
                    NetTaskManager& upload_manager();

                    //communicate with tracker
                    retcode_t login_tracker(const string& tracker_address, int port = TRACKER_PORT);
                    retcode_t logout_tracker(const string& tracker_address, int port = TRACKER_PORT);
                    retcode_t publish_resource_to_tracker(const string& tracker_address, 
                            const string& torrentid, const StringList& fileids);
                    retcode_t report_progress(const string& tracker_address, const string& fileid, int percentage);
                    retcode_t request_clients(const string& tracker_address, const string& fileid, int currentPercentage, 
                                          int needCount, const ClientIdCollection& clientids, FileOwnerInfoPtrList* pInfoList);

                    //communicate with client
                    retcode_t shake_hand(const ClientProto::ShakeHand& self, ClientProto::ShakeHand& peer);

                    //job control
                    retcode_t add_job(const Torrent::Torrent& torrent, const string& top_path, int* internal_handle);
                    retcode_t start_job(int handle);
                    retcode_t pause_download(int handle);
                    retcode_t resume_download(int handle);
                    JobPtr get_job(const string& fileid);

                    



                    //torrent operations
                    retcode_t parse_torrent(const Path& torrent_file_path, Torrent::Torrent* pTorrent);
                    retcode_t make_torrent(const Path& path, const Path& torrent_file_path, 
                            Int32 chunk_size, Int32 type, const string& tracker_address);

                    //self identity
                    string clientid() const;

                    string current_time() const;

                    void set_make_torrent_progress_callback(make_torrent_progress_callback_t callback);

                private:

                    template<typename ReplyMessageType>
                    retcode_t handle_reply_message(LocalSockManager::SockPtr& sock, 
                    const Message& msg, int payload_type, SharedPtr<ReplyMessageType>* out);

                    void list_dir_recursive(const File& file, FileList* pList);

                    int LOCAL_PORT;

                    bool init_error_;
                    string clientid_;
                    LocalSockManagerPtr sockManager_;

                    int job_index_;
                    typedef map<int, JobPtr> JobMap;
                    JobMap jobs_;
                    ThreadPool jobThreads_;

                    //NetTaskManager downloadManager_;
                    NetTaskManager uploadManager_;

                    make_torrent_progress_callback_t make_torrent_progress_callback_;

            };
    }
}

#endif
