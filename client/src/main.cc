#include "client.h"
#include "verification.h"
#include <iostream>
#include <Poco/Exception.h>

using std::cout;
using std::cerr;
using std::endl;
using Poco::Exception;
using CoolDown::Client::Verification;

int main(int argc, char* argv[]){
    try{
        Verification v;
        cout << v.get_file_verification_code(argv[1]) << endl;
        /*
        Client c;
        return c.run();
        */
    }catch(Exception& e){
        cerr << e.message() << endl;
    }
}
