#pragma once
#include "Block.h"
#include <vector>
#include <algorithm>
#include <array>

// Ordinary generated terrain retains one byte/cell and no per-cell metadata.
// Extended chunks use an 8-bit local palette; only >256 distinct states widen.
class CompactBlocks {
    std::uint8_t mode_=0;
    std::vector<std::uint8_t> bytes_;
    std::vector<Block> palette_;
    std::vector<Block> wide_;
    void palette() {
        if(!palette_.empty())return;
        std::vector<Block> values;values.reserve(32);
        std::array<std::int16_t,198> remap;remap.fill(-1);
        for(auto& cell:bytes_){if(remap[cell]<0){remap[cell]=std::int16_t(values.size());values.push_back(Block(cell));}cell=std::uint8_t(remap[cell]);}
        palette_=std::move(values);mode_=1;
    }
public:
    CompactBlocks()=default;
    CompactBlocks(std::vector<Block> values) { resize(values.size(),Block::Air);for(std::size_t i=0;i<values.size();++i)set(i,values[i]); }
    std::size_t size() const {return mode_==2?wide_.size():bytes_.size();}
    bool empty() const {return size()==0;}
    std::size_t storageBytes() const {return bytes_.capacity()+palette_.capacity()*sizeof(Block)+wide_.capacity()*sizeof(Block);}
    bool wide() const {return mode_==2;}
    Block operator[](std::size_t i) const {return mode_==0?Block(bytes_[i]):mode_==1?palette_[bytes_[i]]:wide_[i];}
    void set(std::size_t i,Block b) {
        if(mode_==2){wide_[i]=b;return;}
        if(mode_==0 && std::uint32_t(b)<198){bytes_[i]=std::uint8_t(b);return;}
        palette();
        auto it=std::find(palette_.begin(),palette_.end(),b);
        if(it==palette_.end()) {
            if(palette_.size()==256) {
                wide_.resize(bytes_.size());for(std::size_t j=0;j<bytes_.size();++j)wide_[j]=palette_[bytes_[j]];
                std::vector<std::uint8_t>().swap(bytes_);std::vector<Block>().swap(palette_);mode_=2;wide_[i]=b;return;
            }
            palette_.push_back(b);it=palette_.end()-1;
        }
        bytes_[i]=std::uint8_t(it-palette_.begin());
    }
    struct Reference {
        CompactBlocks& owner;std::size_t index;
        operator Block() const {return static_cast<const CompactBlocks&>(owner)[index];}
        Reference& operator=(Block value){owner.set(index,value);return *this;}
        Reference& operator=(const Reference& value){return *this=Block(value);}
    };
    Reference operator[](std::size_t i){return {*this,i};}
    void assign(std::size_t n,Block value) {*this=CompactBlocks(std::vector<Block>(n,value));}
    void resize(std::size_t n,Block value=Block::Air) {
        const auto previous=size();if(wide()){wide_.resize(n,value);return;}
        bytes_.resize(n);for(std::size_t i=previous;i<n;++i)set(i,value);
    }
    operator std::vector<Block>() const {std::vector<Block> out(size());for(std::size_t i=0;i<size();++i)out[i]=(*this)[i];return out;}
};
