#include "client.h"
#include "verification.h"
#include <iostream>
#include <Poco/Exception.h>

using std::cout;
using std::cerr;
using std::endl;
using Poco::Exception;
using CoolDown::Client::Verification;
using CoolDown::Client::CoolClient;

int main(int argc, char* argv[]){
    try{
        CoolClient c;
        return c.run(argc, argv);
    }catch(Exception& e){
        cerr << e.message() << endl;
    }
}
