#include "client.h"
#include <iostream>
#include <Poco/Exception.h>

using std::cerr;
using std::endl;
using Poco::Exception;

int main(int argc, char* argv[]){
    try{
        Client c;
        return c.run();
    }catch(Exception& e){
        cerr << e.message() << endl;
    }
}
