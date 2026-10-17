#pragma once
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <string>
namespace SaveFile {
struct CommitStatus {
    bool permanentFailure = false;
    std::string reason;
};
// Lifetime reservation, acquired before recovery/loading and held until unload.
class Session {
public:
    explicit Session(const std::string& directory);
    ~Session();
    bool acquired() const;
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
// Serialized writers never share a temporary file. False always means uncommitted.
bool writeImpl(const std::string&, std::ios::openmode, const std::function<bool(std::ofstream&)>&);
template<class Writer> bool write(const std::string& path,std::ios::openmode mode,Writer writer) {
    return writeImpl(path,mode,writer);
}
// Stages a coherent set of files, with a rollback journal for process interruption.
// No retry sleeps: the game schedules bounded retries on subsequent frames.
class Checkpoint {
public:
    explicit Checkpoint(const std::string& journal);
    ~Checkpoint();
    bool commit();
    bool ready() const;
    bool permanentFailure() const;
    const std::string& failureReason() const;
    // All live state has already serialized into private staged files. Only the
    // immutable bytes and commit metadata cross the worker boundary.
    std::future<bool> commitAsync(std::shared_ptr<CommitStatus> status = {});
    Checkpoint(const Checkpoint&)=delete;
    Checkpoint& operator=(const Checkpoint&)=delete;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
bool recover(const std::string& journal);
bool verifyCheckpoint(const std::string& directory, std::string* reason = nullptr);
bool stagingCheckpoint();
// Explicit recovery creates a separate verified world; original bytes stay intact.
bool copyPrevious(const std::string& directory,const std::string& destination);
int interruptionFixture(const std::string& directory);
bool selfTest(const std::string& directory,std::string& report);
}
