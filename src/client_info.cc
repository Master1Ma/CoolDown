#include "client_info.h"

ClientInfo::ClientInfo(const string& clientId, const string& ip, UInt16 messagePort = 9025, UInt16 dataPort = 9026)
    :clientId_(clientId),
    ip_(ip),
    messagePort_(messagePort),
    dataPort_ (dataPort)
{
}

ClientInfo::~ClientInfo(){
}
