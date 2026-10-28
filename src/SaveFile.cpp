#include "SaveFile.h"
#include <system_error>
#include <mutex>
#include <vector>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <cstdlib>
#include <cwctype>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#ifndef _WIN32
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <winternl.h>
#endif
namespace SaveFile {
namespace {
thread_local SnapshotBudget* snapshotBudget=nullptr;
thread_local CommitStatus* snapshotMetrics=nullptr;
struct ProfileTime {
    double* result;std::chrono::steady_clock::time_point start;
    explicit ProfileTime(double* p):result(p){if(result)start=std::chrono::steady_clock::now();}
    ~ProfileTime(){if(result)*result+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
};
}
SnapshotBudget::SnapshotBudget(std::size_t limit):limit_(limit),previous_(snapshotBudget){snapshotBudget=this;}
SnapshotBudget::~SnapshotBudget(){snapshotBudget=previous_;}
void SnapshotBudget::reserve(std::size_t bytes){
    if(bytes>limit_-used_)throw std::length_error("Background checkpoint snapshot exceeds 256 MiB admission budget; use a manual save");
    used_+=bytes;
}
void reserveSnapshotBytes(std::size_t bytes){if(snapshotBudget)snapshotBudget->reserve(bytes);}
struct Session::Impl {
#ifdef _WIN32
    HANDLE file=INVALID_HANDLE_VALUE;
#else
    int file=-1;
#endif
    bool owns=false;
    explicit Impl(const std::string& directory) {
        const auto path=std::filesystem::absolute(std::filesystem::path(directory)/".session.lock");
#ifdef _WIN32
        file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        owns=file!=INVALID_HANDLE_VALUE;
#else
        file=::open(path.c_str(),O_CREAT|O_RDWR,0600);
        owns=file>=0 && ::flock(file,LOCK_EX|LOCK_NB)==0;
#endif
        if(!owns) {
#ifdef _WIN32
            const auto code=GetLastError();
            std::cerr<<"World session reservation failed: "<<path<<" code="<<code
                     <<" message="<<std::system_category().message(int(code))
                     <<" requester_pid="<<GetCurrentProcessId()
                     <<(code==ERROR_SHARING_VIOLATION?" (already reserved by another handle; same or other process)":" (filesystem/access failure)")<<'\n';
#else
            std::cerr<<"World session reservation failed: "<<path<<" errno="<<errno<<'\n';
#endif
        }
    }
    ~Impl(){
#ifdef _WIN32
        if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
#else
        if(file>=0)::close(file);
#endif
    }
};
Session::Session(const std::string& directory):impl_(new Impl(directory)){}
Session::~Session()=default;
bool Session::acquired() const{return impl_->owns;}
namespace {
std::recursive_mutex mutex;
struct Entry {std::string path; bool existed=false;};
thread_local std::vector<Entry>* staged=nullptr;
thread_local bool stageReady=false;
thread_local std::filesystem::path stageDirectory;
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
              << " save not reported successful; preserve checkpoint/backup files for recovery\n" << std::flush;
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
    ProfileTime timing(snapshotMetrics?&snapshotMetrics->stagingMilliseconds:nullptr);
    errno=0;std::ofstream output(path+".tmp",mode|std::ios::trunc);
    if(!output){error(path,"open",errno,false);return false;}
    bool written=false;
    try {written=writer(output);} catch(...) {output.close();throw;}
    output.flush();bool complete=written&&output.good();output.close();complete=complete&&!output.fail();
    if(!complete)error(path,written?"flush/close":"serializer-rejected",written?errno:0,false);
    return complete;
}
bool signature(const std::string& path,std::uint64_t& bytes,std::uint64_t& hash) {
    ProfileTime timing(snapshotMetrics?&snapshotMetrics->hashingMilliseconds:nullptr);
    std::ifstream input(path,std::ios::binary);if(!input)return false;
    bytes=0;hash=14695981039346656037ULL;char buffer[65536];
    while(input){input.read(buffer,sizeof(buffer));auto size=input.gcount();bytes+=size;
        for(std::streamsize i=0;i<size;++i){hash^=static_cast<unsigned char>(buffer[i]);hash*=1099511628211ULL;}}
    return input.eof()&&!input.bad();
}
bool durable(const std::string& path) {
#ifdef _WIN32
    HANDLE file=CreateFileW(std::filesystem::path(path).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){error(path,"durable-open",GetLastError());return false;}
    bool ok=FlushFileBuffers(file)!=0;if(!ok)error(path,"durable-flush",GetLastError());CloseHandle(file);return ok;
#else
    int file=::open(path.c_str(),O_WRONLY);if(file<0)return false;bool ok=::fsync(file)==0;::close(file);return ok;
#endif
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
    // A failed/interrupted commit must retain its verified recovery generation.
    // The next successful checkpoint may rotate it after validating current state.
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
    if(std::filesystem::file_size(journal,ec)>1048576 || ec){error(journal,"journal-length",0);return false;}
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
bool verifyCheckpoint(const std::string& directory, std::string* reason) {
    if(reason)reason->clear();
    const auto root=std::filesystem::path(directory);
    const auto manifest=root/"checkpoint.manifest";
    if(!std::filesystem::exists(manifest))return true; // Unmodified legacy worlds.
    const auto fail=[&](const std::filesystem::path& component,const std::string& detail){
        if(reason)*reason=component.filename().string()+": "+detail;
        error(component.string(),"checkpoint-integrity",0);
        std::cerr<<"Checkpoint detail: "<<detail<<"; automatic retries stopped; use verified recovery copy\n";
        return false;
    };
    if(std::filesystem::file_size(manifest)>65536)return fail(manifest,"manifest exceeds 64 KiB");
    std::ifstream f(manifest);std::string magic;std::uint64_t generation=0;unsigned count=0;
    if(!(f>>magic>>generation>>count)||magic!="VXMAN1"||!generation||!count||count>16)return fail(manifest,"invalid manifest header");
    std::vector<std::string> seen;
    for(unsigned i=0;i<count;++i){std::string name;std::uint64_t bytes=0,hash=0,actualBytes=0,actualHash=0;
        if(!(f>>std::quoted(name)>>bytes>>hash)||name.empty()||name=="."||name==".."||std::filesystem::path(name).filename()!=std::filesystem::path(name)||std::find(seen.begin(),seen.end(),name)!=seen.end())return fail(manifest,"invalid or duplicate component record");
        seen.push_back(name);
        if(!signature((root/name).string(),actualBytes,actualHash)||actualBytes!=bytes||actualHash!=hash){
            std::ostringstream detail;detail<<"generation="<<generation<<" expected_bytes="<<bytes<<" actual_bytes="<<actualBytes<<" expected_hash="<<hash<<" actual_hash="<<actualHash;
            return fail(root/name,detail.str());}}
    f>>std::ws;if(!f.eof())return fail(manifest,"unexpected trailing records");
    return true;
}
bool stagingCheckpoint(){return staged!=nullptr;}
bool copyPrevious(const std::string& directory,const std::string& destination){
    const auto root=std::filesystem::path(directory),target=std::filesystem::path(destination);
    if(!std::filesystem::exists(root/"checkpoint.manifest.checkpoint-old") || std::filesystem::file_size(root/"checkpoint.manifest.checkpoint-old")>65536)return false;
    std::ifstream f(root/"checkpoint.manifest.checkpoint-old");std::string magic;std::uint64_t generation=0;unsigned count=0;
    if(!(f>>magic>>generation>>count)||magic!="VXMAN1"||!generation||!count||count>16)return false;
    std::vector<std::string> files;
    for(unsigned i=0;i<count;++i){std::string name;std::uint64_t bytes=0,hash=0,actualBytes=0,actualHash=0;
        if(!(f>>std::quoted(name)>>bytes>>hash)||name.empty()||std::filesystem::path(name).filename()!=std::filesystem::path(name)||
            !signature((root/(name+".checkpoint-old")).string(),actualBytes,actualHash)||actualBytes!=bytes||actualHash!=hash)return false;
        files.push_back(name);
    }
    f>>std::ws;if(!f.eof() || !std::filesystem::create_directory(target))return false;
    for(const auto& name:files)std::filesystem::copy_file(root/(name+".checkpoint-old"),target/name);
    std::filesystem::copy_file(root/"checkpoint.manifest.checkpoint-old",target/"checkpoint.manifest");
    return verifyCheckpoint(destination);
}
struct Checkpoint::Impl {
    std::unique_lock<std::recursive_mutex> lock{mutex};
    std::string journal;WriterLock writer;std::vector<Entry> entries;bool ready=false,committed=false,permanent=false;std::string reason;
    explicit Impl(std::string j):journal(std::move(j)),writer(journal){
        if(staged)throw std::logic_error("Nested save checkpoint");
        if(!writer.acquired)reason="Save writer is busy";
        else if(!recover(journal)){reason="save.checkpoint: interrupted checkpoint requires recovery";permanent=true;}
        else if(!verifyCheckpoint(std::filesystem::path(journal).parent_path().string(),&reason))permanent=true;
        else ready=true;
        staged=&entries;stageReady=ready;stageDirectory=std::filesystem::absolute(journal).lexically_normal().parent_path();
    }
};
Checkpoint::Checkpoint(const std::string& journal):impl_(new Impl(journal)){}
Checkpoint::~Checkpoint(){staged=nullptr;stageReady=false;stageDirectory.clear();}
bool Checkpoint::ready()const{return impl_->ready;}
bool Checkpoint::permanentFailure()const{return impl_->permanent;}
const std::string& Checkpoint::failureReason()const{return impl_->reason;}
std::future<bool> Checkpoint::commitAsync(std::shared_ptr<CommitStatus> status){
    if(!impl_->ready||impl_->committed){if(status){status->permanentFailure=impl_->permanent;status->reason=impl_->reason;}std::promise<bool> result;result.set_value(false);return result.get_future();}
    impl_->committed=true;
    return std::async(std::launch::async,[journal=impl_->journal,entries=std::move(impl_->entries),status]()mutable{
        Checkpoint checkpoint(journal);checkpoint.impl_->entries=std::move(entries);
        if(status){status->permanentFailure=checkpoint.permanentFailure();status->reason=checkpoint.failureReason();}
        return checkpoint.commit();
    });
}
std::future<bool> commitSnapshotAsync(std::string journal,std::vector<SnapshotComponent> components,
                                      std::shared_ptr<CommitStatus> status) {
    if(components.empty()||components.size()>15)throw std::invalid_argument("Invalid checkpoint snapshot component count");
    return std::async(std::launch::async,[journal=std::move(journal),components=std::move(components),status]{
        struct MetricsOwner {
            CommitStatus* previous=snapshotMetrics;
            explicit MetricsOwner(CommitStatus* p){snapshotMetrics=p;}
            ~MetricsOwner(){snapshotMetrics=previous;}
        } metrics(status.get());
        const auto start=std::chrono::steady_clock::now();
        // This worker owns the writer lock and TLS staging for the entire job.
        // No live game references are used while verifying, encoding or committing.
        Checkpoint checkpoint(journal);
        if(status){status->verificationMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            status->permanentFailure=checkpoint.permanentFailure();status->reason=checkpoint.failureReason();}
        if(!checkpoint.ready())return false;
        {ProfileTime encode(status?&status->encodingMilliseconds:nullptr);
            for(std::size_t i=0;i<components.size();++i)if(!components[i]()) {
                if(status)status->reason="Snapshot component "+std::to_string(i+1)+" failed validation or staging";
                std::cerr<<"Background checkpoint component "<<i+1<<" failed; previous complete generation retained\n";
                return false;
            }}
        ProfileTime commit(status?&status->commitMilliseconds:nullptr);
        return checkpoint.commit();
    });
}
bool writeImpl(const std::string& path,std::ios::openmode mode,const std::function<bool(std::ofstream&)>& writer) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    WriterLock ownership(path);if(!ownership.acquired)return false;
    const std::string absolute=std::filesystem::absolute(path).lexically_normal().string();
    if(staged && (!stageReady || std::filesystem::path(absolute).parent_path()!=stageDirectory)){
        error(absolute,"checkpoint-not-ready-or-wrong-directory",0);return false;
    }
    if(!staged){
        const auto component=std::filesystem::path(absolute);const auto manifest=component.parent_path()/"checkpoint.manifest";
        if(std::filesystem::exists(manifest)){
            bool managed=false;
            for(const char* name:{"world_seed.txt","world.info","voxel_world.vxw","voxel_player.vps","voxel_inventory.vxi","voxel_mobs.vxm","voxel_travel.vtn","checkpoint.manifest"})managed|=component.filename()==name;
            if(std::filesystem::file_size(manifest)<=65536){std::ifstream f(manifest);std::string magic,name;std::uint64_t generation,bytes,hash;unsigned n=0;
                if(f>>magic>>generation>>n && magic=="VXMAN1" && n<=16)for(unsigned i=0;i<n && (f>>std::quoted(name)>>bytes>>hash);++i)managed|=component.filename()==name;
            }
            if(managed){error(absolute,"checkpoint-transaction-required",0);return false;}
        }
    }
    if(!serialize(absolute,mode,writer))return false;
    if(staged){for(const auto& e:*staged)if(e.path==absolute)return true;staged->push_back({absolute,false});return true;}
    return replaceFile(absolute,absolute+".tmp");
}
bool Checkpoint::commit() {
    auto& s=*impl_;if(!s.ready||s.committed||s.entries.empty()||s.entries.size()>15)return false;staged=nullptr;
    // The manifest participates in the same rollback transaction. Its monotonic
    // generation identifies one complete set; checksums also detect lost sidecars.
    const auto manifest=(std::filesystem::path(s.journal).parent_path()/"checkpoint.manifest").string();
    struct Record {std::string name;std::uint64_t bytes=0,hash=0;};
    std::vector<Record> records;
    std::uint64_t generation=0;{
        std::ifstream old(manifest);std::string magic;unsigned n=0;
        if(old>>magic>>generation>>n)for(unsigned i=0;i<n;++i){Record r;old>>std::quoted(r.name)>>r.bytes>>r.hash;records.push_back(r);}
    }
    if(generation==UINT64_MAX)return false;
    for(const auto& e:s.entries){Record r;r.name=std::filesystem::path(e.path).filename().string();if(!signature(e.path+".tmp",r.bytes,r.hash))return false;
        auto found=std::find_if(records.begin(),records.end(),[&](const auto& old){return old.name==r.name;});
        if(found==records.end())records.push_back(r);else *found=r;}
    if(!serialize(manifest,std::ios::out,[&](std::ofstream& f){
        f<<"VXMAN1 "<<generation+1<<' '<<records.size()<<'\n';
        for(const auto& r:records)f<<std::quoted(r.name)<<' '<<r.bytes<<' '<<r.hash<<'\n';
        return bool(f);
    }))return false;
    s.entries.push_back({manifest,false});
    for(const auto& e:s.entries)if(!durable(e.path+".tmp"))return false;
    auto backups=s.entries;
    for(const auto& r:records){const auto path=(std::filesystem::path(s.journal).parent_path()/r.name).string();
        if(std::none_of(backups.begin(),backups.end(),[&](const auto& e){return e.path==path;}))backups.push_back({path,false});}
    for(auto& e:backups){std::error_code ec;e.existed=std::filesystem::exists(e.path,ec);
        if(!ec&&e.existed){
            std::filesystem::remove(e.path+".checkpoint-old",ec);
            if(!ec)std::filesystem::create_hard_link(e.path,e.path+".checkpoint-old",ec);
            // Atomic replacement leaves the previous inode intact; a hard link
            // avoids duplicating an unchanged multi-MiB world/map checkpoint.
            if(ec){ec.clear();std::filesystem::copy_file(e.path,e.path+".checkpoint-old",std::filesystem::copy_options::overwrite_existing,ec);}
        }
        if(ec){error(e.path,"checkpoint-backup",ec.value());return false;}}
    for(auto& e:s.entries)for(const auto& backup:backups)if(backup.path==e.path)e.existed=backup.existed;
    if(!write(s.journal,std::ios::out,[&](std::ofstream& f){f<<"VXCHECK1 "<<s.entries.size()<<'\n';for(const auto& e:s.entries)f<<std::quoted(e.path)<<' '<<e.existed<<'\n';return bool(f);}))return false;
    if(!durable(s.journal))return false;
    for(const auto& e:s.entries){if(!replaceFile(e.path,e.path+".tmp")){rollback(s.journal,s.entries);return false;}
        if(interruptAfter>0 && --interruptAfter==0)std::_Exit(77); // Opt-in child-process interruption fixture only.
    }
    std::error_code ec;std::filesystem::remove(s.journal,ec);
    if(ec){error(s.journal,"commit-clear-journal",ec.value());rollback(s.journal,s.entries);return false;}
    // Retain the previous complete generation for explicit recovery. The next
    // checkpoint replaces these backups only after this one has been verified.
    s.committed=true;return true;
}
int interruptionFixture(const std::string& directory){
    std::filesystem::create_directories(directory);
    const auto root=std::filesystem::absolute(directory);
    auto put=[](const std::string& p,const char* data){return write(p,std::ios::binary,[&](std::ofstream& f){f<<data;return bool(f);});};
    const auto a=(root/"inventory.test").string(),b=(root/"navigation.test").string(),metadata=(root/"world_seed.txt").string();
    if(!put(a,"old")||!put(b,"old")||!put(metadata,"old-metadata"))return 1;
    {Checkpoint baseline((root/"save.checkpoint").string());if(!put(a,"old")||!put(b,"old")||!put(metadata,"old-metadata")||!baseline.commit())return 1;}
    Checkpoint checkpoint((root/"save.checkpoint").string());
    if(!put(a,"new")||!put(b,"new")||!put(metadata,"new-metadata"))return 1;
    interruptAfter=3;checkpoint.commit();return 1; // Data and metadata replaced; manifest still old.
}
}
