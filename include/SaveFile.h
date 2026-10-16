#pragma once
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
namespace SaveFile {
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
    Checkpoint(const Checkpoint&)=delete;
    Checkpoint& operator=(const Checkpoint&)=delete;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
bool recover(const std::string& journal);
int interruptionFixture(const std::string& directory);
bool selfTest(const std::string& directory,std::string& report);
}
