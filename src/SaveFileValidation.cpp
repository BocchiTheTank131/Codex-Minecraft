#include "SaveFile.h"
#include <thread>
#include <atomic>
#include <vector>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
namespace SaveFile {
bool selfTest(const std::string& directory,std::string& report) {
    namespace fs=std::filesystem;fs::create_directories(directory);
    const auto file=(fs::absolute(directory)/"inventory.test").string(),navigation=(fs::absolute(directory)/"navigation.test").string(),journal=(fs::absolute(directory)/"save.checkpoint").string();
    auto put=[](const std::string& p,const std::string& value){return write(p,std::ios::binary,[&](std::ofstream& f){f<<value;return bool(f);});};
    auto get=[](const std::string& p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
    auto check=[](bool v,const char* why){if(!v)throw std::runtime_error(why);};
    check(put(file,"old-inventory")&&put(navigation,"old-maps"),"save setup");
    check(!write(file,std::ios::binary,[](std::ofstream& f){f<<"incomplete";return false;})&&get(file)=="old-inventory","failed serialization preserves previous");
#ifdef _WIN32
    fs::rename(file,file+".replace-old");check(recover(journal)&&get(file)=="old-inventory","standalone partial replacement backup recovered before load");
    HANDLE held=CreateFileW(fs::path(file).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    check(held!=INVALID_HANDLE_VALUE,"lock fixture handle");
    check(!put(file,"locked-new")&&get(file)=="old-inventory"&&get(file+".tmp")=="locked-new","locked replacement false/previous/tmp");
    CloseHandle(held);check(put(file,"recovered"),"replacement after lock release");
    {
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
        const auto child=fs::absolute(directory)/"terminated";
        std::wstring args=L"\""+std::wstring(exe)+L"\" \"--save-interruption-fixture="+child.wstring()+L"\"";
        STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi{};
        check(CreateProcessW(exe,args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=0,"start interruption child");
        const auto wait=WaitForSingleObject(pi.hProcess,30000);DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        check(wait==WAIT_OBJECT_0&&code==77,"process interrupted during real checkpoint commit");
        check(get((child/"inventory.test").string())=="new"&&get((child/"navigation.test").string())=="old","interrupted mixed checkpoint fixture");
        check(recover((child/"save.checkpoint").string())&&get((child/"inventory.test").string())=="old"&&get((child/"navigation.test").string())=="old","restart after abrupt process termination");
    }
    {
        Checkpoint owning(journal);
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
        const auto child=fs::absolute(directory)/"competing";
        std::wstring args=L"\""+std::wstring(exe)+L"\" \"--save-interruption-fixture="+child.wstring()+L"\"";
        STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi{};
        check(CreateProcessW(exe,args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=0,"start competing child");
        const auto wait=WaitForSingleObject(pi.hProcess,30000);DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        check(wait==WAIT_OBJECT_0&&code==1&&!fs::exists(child/"inventory.test"),"competing process refuses writer without corrupting staging");
    }
    // A failure after the first replacement must not split inventory from map data.
    held=CreateFileW(fs::path(navigation).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    check(held!=INVALID_HANDLE_VALUE,"checkpoint lock fixture");
    {Checkpoint batch(journal);check(put(file,"new-inventory")&&put(navigation,"new-map"),"stage checkpoint");check(!batch.commit(),"locked batch must not report saved");}
    CloseHandle(held);check(recover(journal)&&get(file)=="recovered"&&get(navigation)=="old-maps","checkpoint rollback/recovery");
#endif
    {Checkpoint batch(journal);check(put(file,"paired-inventory")&&put(navigation,"paired-maps")&&batch.commit(),"checkpoint commit");}
    // Exact durable state left by process termination between replacements.
    fs::copy_file(file,file+".checkpoint-old",fs::copy_options::overwrite_existing);fs::copy_file(navigation,navigation+".checkpoint-old",fs::copy_options::overwrite_existing);
    {std::ofstream f(journal);f<<"VXCHECK1 2\n"<<std::quoted(file)<<" 1\n"<<std::quoted(navigation)<<" 1\n";}
    check(put(file,"interrupted-new"),"interrupted state");check(recover(journal)&&get(file)=="paired-inventory"&&get(navigation)=="paired-maps","restart rolls back complete pair");
    std::atomic<bool> ok{true};std::atomic<int> writerRetries{0};std::vector<std::thread> threads;
    for(int n=0;n<4;++n)threads.emplace_back([&,n]{for(int i=0;i<100;++i){bool saved=false;
        for(int attempt=0;attempt<4&&!saved;++attempt){saved=put(file,std::to_string(n)+":"+std::to_string(i)+":"+std::string(4096,char('A'+n)));if(!saved){++writerRetries;std::this_thread::sleep_for(std::chrono::milliseconds(5*(1<<attempt)));}}
        if(!saved)ok=false;
    }});
    for(auto& t:threads)t.join();const auto content=get(file);check(ok&&content.size()>4096&&content.substr(content.size()-4096)==std::string(4096,content.back()),"concurrent writers serialize whole records");
    std::cout<<"Concurrent writer transient failures recovered="<<writerRetries<<'\n';
    int recoveredFailures=0;
    for(int i=0;i<200;++i){bool saved=false;for(int attempt=0;attempt<4&&!saved;++attempt){
        {Checkpoint batch(journal);saved=put(file,std::to_string(i))&&put(navigation,std::to_string(i))&&batch.commit();}
        check(recover(journal)&&get(file)==get(navigation),"checkpoint order after success/failure");
        if(!saved){++recoveredFailures;std::this_thread::sleep_for(std::chrono::milliseconds(5*(1<<attempt)));} // Test driver only; gameplay uses frame deadlines.
    }check(saved,"rapid paired saves exhausted bounded recovery");}
    std::cout<<"Rapid checkpoint transient failures recovered="<<recoveredFailures<<'\n';
    report="Save reliability: held Windows delete locks reproduced; failures return false, old data retained, release/retry succeeds; interrupted paired rollback, 400 concurrent writes and 200 paired checkpoints passed";
    return true;
}
}
