#include "SaveFile.h"
#include <thread>
#include <condition_variable>
#include <mutex>
#include <iostream>
int main(){
    std::mutex m;std::condition_variable cv;bool aOpen=false,bDone=false,aOk=false,bOk=false;
    SaveFile::write("race.bin",std::ios::binary,[](std::ofstream& f){f<<"previous";return bool(f);});
    std::thread a([&]{aOk=SaveFile::write("race.bin",std::ios::binary,[&](std::ofstream& f){
        f<<std::string(128,'A');f.flush();{std::lock_guard<std::mutex> l(m);aOpen=true;}cv.notify_all();
        {std::unique_lock<std::mutex> l(m);cv.wait(l,[&]{return bDone;});}f<<std::string(128,'A');return bool(f);
    });});
    {std::unique_lock<std::mutex> l(m);cv.wait(l,[&]{return aOpen;});}
    bOk=SaveFile::write("race.bin",std::ios::binary,[](std::ofstream& f){f<<std::string(128,'B');return bool(f);});
    {std::lock_guard<std::mutex> l(m);bDone=true;}cv.notify_all();a.join();
    std::ifstream f("race.bin",std::ios::binary);const std::string bytes(std::istreambuf_iterator<char>(f),{});
    std::cout<<"Historical shared-temp reproduction: A success="<<aOk<<" B success="<<bOk<<" bytes="<<bytes.size()<<" first="<<(bytes.empty()?'?':bytes.front())<<" last="<<(bytes.empty()?'?':bytes.back())<<" expected A=256 identical bytes\n";
    return aOk&&bytes!=std::string(256,'A')?0:2;
}
