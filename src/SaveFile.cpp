#include "SaveFile.h"
#include <system_error>
#include <mutex>
#include <vector>
#include <iomanip>
#include <iostream>
#include <cerrno>
#include <cstdlib>
#include <cwctype>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <winternl.h>
#endif
namespace SaveFile {
namespace {
std::recursive_mutex mutex;
struct Entry {std::string path; bool existed=false;};
thread_local std::vector<Entry>* staged=nullptr;
thread_local int interruptAfter=-1;
void error(const std::string& path,const char* stage,unsigned code,bool native=true) {
    std::cerr << "Save failure stage=" << stage << " path=" << std::quoted(path)
              << " domain=" << (native?"system":"errno") << " code=" << code << " message=" << (code?(native?std::system_category().message(int(code)):std::generic_category().message(int(code))):"no OS error supplied (see stage)")
              << " thread=" <<
#ifdef _WIN32
              GetCurrentThreadId()
#else
              0
#endif
              << " previous checkpoint retained; temporary data not reported as saved\n" << std::flush;
}
// Same-profile processes must not serialize into each other's .tmp or journal.
// A zero-timeout named mutex refuses contention instead of stalling a frame.
struct WriterLock {
    bool acquired=true;
#ifdef _WIN32
    HANDLE handle=nullptr;
    explicit WriterLock(const std::string& path){
        auto profile=std::filesystem::current_path().wstring();
        std::transform(profile.begin(),profile.end(),profile.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
        const auto name=L"Local\\VoxelFrontierSave-"+std::to_wstring(std::hash<std::wstring>{}(profile));
        handle=CreateMutexW(nullptr,FALSE,name.c_str());
        const auto state=handle?WaitForSingleObject(handle,0):WAIT_FAILED;
        acquired=state==WAIT_OBJECT_0||state==WAIT_ABANDONED;
        if(!acquired)error(path,"writer-busy",handle?ERROR_BUSY:GetLastError());
    }
    ~WriterLock(){if(acquired&&handle)ReleaseMutex(handle);if(handle)CloseHandle(handle);}
#else
    explicit WriterLock(const std::string&){}
#endif
};
#ifdef _WIN32
void lockOwners(const std::string& path) {
    const auto wide=std::filesystem::path(path).wstring();
    std::cerr<<"Save lock diagnostic path="<<std::quoted(path)<<" attributes="<<GetFileAttributesW(wide.c_str());
    HANDLE file=CreateFileW(wide.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){
        using Query=LONG (NTAPI*)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,ULONG);
        const auto query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationFile"));
        struct Owners {ULONG count;ULONG_PTR pid[64];} owners{};IO_STATUS_BLOCK status{};
        if(query && query(file,&status,&owners,sizeof(owners),47)>=0){
            std::cerr<<" owners=";
            for(ULONG i=0;i<std::min<ULONG>(owners.count,64);++i){std::cerr<<owners.pid[i]<<':' ;
                HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(owners.pid[i]));wchar_t executable[1024]{};DWORD size=1024;
                if(process){if(QueryFullProcessImageNameW(process,0,executable,&size))std::cerr<<std::filesystem::path(executable).filename().string();CloseHandle(process);}std::cerr<<',';
            }
        }else std::cerr<<" owners=unavailable";
        CloseHandle(file);
    }else std::cerr<<" handle-query-error="<<GetLastError();
    std::cerr<<'\n'<<std::flush;
}
#endif
bool replaceFile(const std::string& path,const std::string& temporary) {
#ifdef _WIN32
    const auto destination=std::filesystem::path(path).wstring(),source=std::filesystem::path(temporary).wstring();
    const auto backup=std::filesystem::path(path+".replace-old").wstring();
    // Supplying a backup also preserves the old file for ReplaceFile's documented
    // partial-failure states (1176/1177). Never delete the destination to retry.
    std::error_code cleanup;
    if(std::filesystem::exists(backup)){
        if(!std::filesystem::exists(destination)){
            if(!MoveFileExW(backup.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)){error(path,"restore-replacement-backup",GetLastError());return false;}
        }else {std::filesystem::remove(backup,cleanup);if(cleanup){error(path,"clear-replacement-backup",cleanup.value());return false;}}
    }
    if(ReplaceFileW(destination.c_str(),source.c_str(),backup.c_str(),0,nullptr,nullptr)){
        std::filesystem::remove(backup,cleanup);return true;
    }
    auto code=GetLastError();
    if(code==ERROR_FILE_NOT_FOUND && GetFileAttributesW(destination.c_str())==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND){
        if(MoveFileExW(source.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))return true;
        code=GetLastError();
    }
    error(path,"replace",code);
    if(code==ERROR_ACCESS_DENIED||code==ERROR_SHARING_VIOLATION||code==1175||code==1176||code==1177){lockOwners(path);lockOwners(temporary);}
    if(code==1177 && !MoveFileExW(backup.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))error(path,"restore-partial-replacement",GetLastError());
    return false;
#else
    std::error_code ec;std::filesystem::rename(temporary,path,ec);
    if(ec)error(path,"replace",ec.value());return !ec;
#endif
}
bool serialize(const std::string& path,std::ios::openmode mode,const std::function<bool(std::ofstream&)>& writer) {
    errno=0;std::ofstream output(path+".tmp",mode|std::ios::trunc);
    if(!output){error(path,"open",errno,false);return false;}
    bool written=false;
    try {written=writer(output);} catch(...) {output.close();throw;}
    output.flush();bool complete=written&&output.good();output.close();complete=complete&&!output.fail();
    if(!complete)error(path,written?"flush/close":"serializer-rejected",written?errno:0,false);
    return complete;
}
bool rollback(const std::string& journal,const std::vector<Entry>& entries) {
    for(const auto& e:entries) {
        std::error_code ec;
        if(e.existed){
            if(std::filesystem::exists(e.path,ec)&&std::filesystem::equivalent(e.path,e.path+".checkpoint-old",ec)&&!ec)continue;
            ec.clear();
            std::filesystem::copy_file(e.path+".checkpoint-old",e.path+".rollback",std::filesystem::copy_options::overwrite_existing,ec);
            if(ec){error(e.path,"restore-copy",ec.value());return false;}
            if(!replaceFile(e.path,e.path+".rollback"))return false;
        }else {std::filesystem::remove(e.path,ec);if(ec){error(e.path,"restore-remove",ec.value());return false;}}
    }
    std::error_code ec;std::filesystem::remove(journal,ec);
    if(ec){error(journal,"clear-journal",ec.value());return false;}
    for(const auto& e:entries)std::filesystem::remove(e.path+".checkpoint-old",ec);
    return true;
}
}
bool recover(const std::string& journal) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    WriterLock writer(journal);if(!writer.acquired)return false;
    std::error_code ec;
#ifdef _WIN32
    const auto parent=std::filesystem::absolute(journal).parent_path();
    for(const auto& entry:std::filesystem::directory_iterator(parent,ec)){
        const auto name=entry.path().filename().string();const std::string suffix=".replace-old";
        if(name.size()<=suffix.size()||name.compare(name.size()-suffix.size(),suffix.size(),suffix))continue;
        const auto target=parent/name.substr(0,name.size()-suffix.size());
        if(!std::filesystem::exists(target) && !MoveFileExW(entry.path().c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)){error(target.string(),"recover-partial-replacement",GetLastError());return false;}
    }
    if(ec){error(journal,"checkpoint-directory",ec.value());return false;}
#endif
    if(!std::filesystem::exists(journal,ec))return !ec;
    std::ifstream f(journal);std::string magic;unsigned count=0;f>>magic>>count;
    if(magic!="VXCHECK1"||count>16||!count){error(journal,"invalid-journal",0);return false;}
    std::vector<Entry> entries;
    for(unsigned i=0;i<count;++i){Entry e;if(!(f>>std::quoted(e.path)>>e.existed)){error(journal,"truncated-journal",0);return false;}
        // A world checkpoint cannot overwrite files outside its own directory.
        const auto journalParent=std::filesystem::absolute(journal).parent_path().lexically_normal();
        const auto target=std::filesystem::absolute(e.path).lexically_normal();
        if(target.parent_path()!=journalParent){error(journal,"invalid-journal-path",0);return false;}
        entries.push_back(e);
    }
    f.close();const bool ok=rollback(journal,entries);
    if(ok)std::cerr<<"Recovered interrupted checkpoint "<<std::quoted(journal)<<'\n';
    return ok;
}
struct Checkpoint::Impl {
    std::unique_lock<std::recursive_mutex> lock{mutex};
    std::string journal;WriterLock writer;std::vector<Entry> entries;bool ready=false,committed=false;
    explicit Impl(std::string j):journal(std::move(j)),writer(journal){if(staged)throw std::logic_error("Nested save checkpoint");ready=writer.acquired&&recover(journal);staged=&entries;}
};
Checkpoint::Checkpoint(const std::string& journal):impl_(new Impl(journal)){}
Checkpoint::~Checkpoint(){staged=nullptr;}
bool writeImpl(const std::string& path,std::ios::openmode mode,const std::function<bool(std::ofstream&)>& writer) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    WriterLock ownership(path);if(!ownership.acquired)return false;
    const std::string absolute=std::filesystem::absolute(path).lexically_normal().string();
    if(!serialize(absolute,mode,writer))return false;
    if(staged){for(const auto& e:*staged)if(e.path==absolute)return true;staged->push_back({absolute,false});return true;}
    return replaceFile(absolute,absolute+".tmp");
}
bool Checkpoint::commit() {
    auto& s=*impl_;if(!s.ready||s.committed)return false;staged=nullptr;
    for(auto& e:s.entries){std::error_code ec;e.existed=std::filesystem::exists(e.path,ec);
        if(!ec&&e.existed){
            std::filesystem::remove(e.path+".checkpoint-old",ec);
            if(!ec)std::filesystem::create_hard_link(e.path,e.path+".checkpoint-old",ec);
            // Atomic replacement leaves the previous inode intact; a hard link
            // avoids duplicating an unchanged multi-MiB world/map checkpoint.
            if(ec){ec.clear();std::filesystem::copy_file(e.path,e.path+".checkpoint-old",std::filesystem::copy_options::overwrite_existing,ec);}
        }
        if(ec){error(e.path,"checkpoint-backup",ec.value());return false;}}
    if(!write(s.journal,std::ios::out,[&](std::ofstream& f){f<<"VXCHECK1 "<<s.entries.size()<<'\n';for(const auto& e:s.entries)f<<std::quoted(e.path)<<' '<<e.existed<<'\n';return bool(f);}))return false;
    for(const auto& e:s.entries){if(!replaceFile(e.path,e.path+".tmp")){rollback(s.journal,s.entries);return false;}
        if(interruptAfter>0 && --interruptAfter==0)std::_Exit(77); // Opt-in child-process interruption fixture only.
    }
    std::error_code ec;std::filesystem::remove(s.journal,ec);
    if(ec){error(s.journal,"commit-clear-journal",ec.value());rollback(s.journal,s.entries);return false;}
    s.committed=true;for(const auto& e:s.entries)std::filesystem::remove(e.path+".checkpoint-old",ec);return true;
}
int interruptionFixture(const std::string& directory){
    std::filesystem::create_directories(directory);
    const auto root=std::filesystem::absolute(directory);
    auto put=[](const std::string& p,const char* data){return write(p,std::ios::binary,[&](std::ofstream& f){f<<data;return bool(f);});};
    const auto a=(root/"inventory.test").string(),b=(root/"navigation.test").string();
    if(!put(a,"old")||!put(b,"old"))return 1;
    Checkpoint checkpoint((root/"save.checkpoint").string());
    if(!put(a,"new")||!put(b,"new"))return 1;
    interruptAfter=1;checkpoint.commit();return 1;
}
}
