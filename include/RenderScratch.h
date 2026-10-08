#pragma once

#include <cstddef>
#include <vector>

// Scope-local ownership of a thread-private reusable vector. Large stress-test
// capacities are released after use; normal capacities survive the next frame.
template <class T>
class RenderScratch {
public:
    RenderScratch(std::vector<T>& storage, std::size_t maximumBytes)
        : storage_(storage), maximumBytes_(maximumBytes) {
        storage_.clear();
    }
    ~RenderScratch() {
        storage_.clear();
        if (storage_.capacity() > maximumBytes_ / sizeof(T))
            std::vector<T>().swap(storage_);
    }
    RenderScratch(const RenderScratch&) = delete;
    RenderScratch& operator=(const RenderScratch&) = delete;
    std::vector<T>& get() { return storage_; }

private:
    std::vector<T>& storage_;
    std::size_t maximumBytes_;
};
