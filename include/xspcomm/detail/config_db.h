#ifndef XSPCOMM_DETAIL_CONFIG_DB_H
#define XSPCOMM_DETAIL_CONFIG_DB_H

// XSCFG v1: a read-only, single-file configuration database. All integers on
// disk are explicitly little endian; no native pointers or C++ structs are
// serialized. The sorted hash index is directly mapped, while prefix-coded
// names and tagged values are compressed in independently checksummed blocks.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <zlib.h>
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace xspcomm::detail::cfgdb {
constexpr uint32_t format_version = 1;
constexpr size_t header_size = 128, index_size = 8, directory_size = 24;
constexpr size_t block_target = 65536, block_entries = 256;
constexpr size_t max_block_size = 256 * 1024 * 1024;
constexpr char magic[8] = {'X','S','C','F','G','D','B','\0'};

inline void require(bool good, const char *message) {
    if (!good) throw std::invalid_argument(std::string("XSCFG: ") + message);
}
inline uint64_t read_le(const char *p, unsigned bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) value |= uint64_t(static_cast<unsigned char>(p[i])) << (8 * i);
    return value;
}
inline void put_le(std::string &out, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) out.push_back(char(value >> (8 * i)));
}
inline void set_le(std::string &out, size_t at, uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) out.at(at + i) = char(value >> (8 * i));
}
inline void put_uint(std::string &out, uint64_t value) {
    while (value >= 128) { out.push_back(char((value & 127) | 128)); value >>= 7; }
    out.push_back(char(value));
}
struct Cursor {
    std::string_view bytes;
    size_t pos = 0;
    uint32_t nodes = 0;
    uint64_t uint() {
        uint64_t result = 0;
        for (unsigned shift = 0; shift <= 63; shift += 7) {
            require(pos < bytes.size(), "truncated integer");
            const auto c = uint8_t(bytes[pos++]);
            require(shift != 63 || c <= 1, "integer overflow");
            result |= uint64_t(c & 127) << shift;
            if (!(c & 128)) return result;
        }
        throw std::invalid_argument("XSCFG: invalid integer");
    }
    std::string_view take(uint64_t size) {
        require(size <= bytes.size() - pos, "truncated value");
        auto result = bytes.substr(pos, size); pos += size; return result;
    }
    std::string string() { const auto size = uint(); return std::string(take(size)); }
};

// A value is materialized only for a requested record, never for the whole
// database. This also lets Picker and xcomm share a format without coupling
// their JSON/YAML libraries or linking the Picker executable to libxspcomm.
struct Value {
    enum Kind : uint8_t { Null, False, True, Unsigned, Signed, Real, String, Array, Object } kind = Null;
    uint64_t number = 0;
    std::string text;
    std::vector<Value> array;
    std::map<std::string, Value> object;
    static Value str(std::string text) { Value v; v.kind = String; v.text = std::move(text); return v; }
    static Value uint(uint64_t n) { Value v; v.kind = Unsigned; v.number = n; return v; }
    static Value sint(int64_t n) { Value v; v.kind = Signed; v.number = uint64_t(n); return v; }
    void encode(std::string &out, unsigned depth = 0, uint32_t *visited = nullptr) const {
        uint32_t local = 0; if (!visited) visited = &local;
        require(depth < 128 && ++*visited <= 1000000, "value complexity limit");
        out.push_back(char(kind));
        switch (kind) {
        case Unsigned: put_uint(out, number); break;
        case Signed: put_uint(out, (number << 1) ^ (uint64_t(0) - (number >> 63))); break;
        case Real: put_le(out, number, 8); break;
        case String: put_uint(out, text.size()); out += text; break;
        case Array:
            put_uint(out, array.size()); for (const auto &v : array) v.encode(out, depth + 1, visited); break;
        case Object:
            put_uint(out, object.size());
            for (const auto &[key, v] : object) {
                put_uint(out, key.size()); out += key; v.encode(out, depth + 1, visited);
            }
            break;
        default: require(kind <= True, "unknown value tag");
        }
    }
    static Value decode(Cursor &in, unsigned depth = 0) {
        require(depth < 128 && ++in.nodes <= 1000000, "value complexity limit");
        Value v; v.kind = Kind(uint8_t(in.take(1)[0]));
        switch (v.kind) {
        case Unsigned: v.number = in.uint(); break;
        case Signed: { auto n = in.uint(); v.number = (n >> 1) ^ (uint64_t(0) - (n & 1)); break; }
        case Real: v.number = read_le(in.take(8).data(), 8); break;
        case String: v.text = in.string(); break;
        case Array: {
            auto count = in.uint(); require(count <= in.bytes.size() - in.pos, "invalid array count");
            for (uint64_t i = 0; i < count; ++i) v.array.push_back(decode(in, depth + 1));
            break;
        }
        case Object: {
            auto count = in.uint(); require(count <= (in.bytes.size() - in.pos) / 2, "invalid object count");
            for (uint64_t i = 0; i < count; ++i) {
                auto key = in.string(); auto child = decode(in, depth + 1);
                require(v.object.emplace(std::move(key), std::move(child)).second, "duplicate object key");
            }
            break;
        }
        default: require(v.kind <= True, "unknown value tag");
        }
        return v;
    }
};

inline std::string key(char category, std::string_view name) {
    require(name.find('\0') == std::string_view::npos, "NUL in record name");
    return std::string(1, category) + ':' + std::string(name);
}
inline uint64_t hash(std::string_view value) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : value) { h ^= c; h *= 1099511628211ull; }
    return h;
}

class Database {
    const char *data_ = nullptr;
    size_t size_ = 0;
    bool mapped_ = false;
    std::string owned_;
    uint64_t count_ = 0, blocks_ = 0, index_ = 0, directory_ = 0;
    std::string model_;
    struct Cached { uint32_t block; std::shared_ptr<const std::string> bytes; };
    mutable std::list<Cached> cache_;
    mutable size_t cache_bytes_ = 0;
    static constexpr size_t cache_limit = 2 * 1024 * 1024;
    void range(uint64_t offset, uint64_t length) const {
        require(offset <= size_ && length <= size_ - offset, "file interval out of bounds");
    }
    std::shared_ptr<const std::string> block(uint32_t id) const {
        require(id < blocks_, "invalid block id");
        for (auto it = cache_.begin(); it != cache_.end(); ++it) if (it->block == id) {
            auto bytes = it->bytes; cache_.splice(cache_.begin(), cache_, it); return bytes;
        }
        const char *d = data_ + directory_ + uint64_t(id) * directory_size;
        auto offset = read_le(d, 8), length = read_le(d + 8, 4), raw_size = read_le(d + 12, 4);
        auto checksum = read_le(d + 16, 4), codec = read_le(d + 20, 4);
        range(offset, length);
        require(offset >= read_le(data_ + 56, 8) && raw_size >= 4 && raw_size <= max_block_size, "invalid block size/offset");
        auto out = std::make_shared<std::string>(raw_size, '\0');
        if (codec == 0) {
            require(length == raw_size, "invalid uncompressed block");
            std::memcpy(out->data(), data_ + offset, raw_size);
        } else {
            require(codec == 1, "unknown block codec");
            uLongf n = raw_size;
            require(uncompress(reinterpret_cast<Bytef *>(out->data()), &n,
                               reinterpret_cast<const Bytef *>(data_ + offset), length) == Z_OK && n == raw_size,
                    "invalid compressed block");
        }
        require(crc32(0, reinterpret_cast<const Bytef *>(out->data()), out->size()) == checksum, "block checksum mismatch");
        auto n = read_le(out->data(), 4);
        require(n > 0 && n <= block_entries && 4 + n * 4 <= out->size(), "invalid block directory");
        if (out->size() <= cache_limit) {
            while (cache_bytes_ + out->size() > cache_limit && !cache_.empty()) {
                cache_bytes_ -= cache_.back().bytes->size(); cache_.pop_back();
            }
            cache_.push_front({id, out}); cache_bytes_ += out->size();
        }
        return out;
    }
    static std::string_view next_entry(const std::string &block, uint32_t slot, std::string &name) {
        auto n = read_le(block.data(), 4);
        require(slot < n, "invalid block slot");
        auto begin = read_le(block.data() + 4 + slot * 4, 4);
        auto end = slot + 1 < n ? read_le(block.data() + 8 + slot * 4, 4) : block.size();
        require(begin >= 4 + n * 4 && begin <= end && end <= block.size(), "invalid record interval");
        Cursor c{std::string_view(block).substr(begin, end - begin)};
        auto prefix = c.uint(); require(prefix <= name.size(), "invalid name prefix");
        name.resize(prefix); name += c.string();
        auto size = c.uint(); auto payload = c.take(size);
        require(c.pos == c.bytes.size(), "trailing record bytes");
        return payload;
    }
    static std::pair<std::string, std::string_view> entry(const std::string &block, uint32_t slot) {
        std::string name; std::string_view payload;
        require(slot < read_le(block.data(), 4), "invalid block slot");
        for (uint32_t i = 0; i <= slot; ++i) {
            payload = next_entry(block, i, name);
        }
        return {std::move(name), payload};
    }
public:
    explicit Database(const std::string &path, const std::string &expected_model = "") {
        try {
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
            int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
            if (fd < 0) throw std::invalid_argument("XSCFG: cannot open " + path);
            struct stat st{};
            if (fstat(fd, &st) || st.st_size < int64_t(header_size) || uint64_t(st.st_size) > SIZE_MAX) {
                ::close(fd); throw std::invalid_argument("XSCFG: invalid file size");
            }
            size_ = size_t(st.st_size);
            void *p = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0); ::close(fd);
            if (p == MAP_FAILED) throw std::invalid_argument("XSCFG: mmap failed");
            data_ = static_cast<const char *>(p); mapped_ = true;
#else
            std::ifstream in(path, std::ios::binary);
            require(bool(in), "cannot open database");
            owned_ = std::string(std::istreambuf_iterator<char>(in), {});
            data_ = owned_.data(); size_ = owned_.size();
#endif
            require(size_ >= header_size && std::memcmp(data_, magic, 8) == 0, "invalid database magic");
            require(read_le(data_ + 8, 4) == format_version, "unsupported database version");
            require(read_le(data_ + 12, 4) == header_size && read_le(data_ + 16, 8) == size_, "invalid header size");
            require(read_le(data_ + 120, 4) == crc32(0, reinterpret_cast<const Bytef *>(data_), 120), "header checksum mismatch");
            count_ = read_le(data_ + 24, 8); blocks_ = read_le(data_ + 32, 8);
            index_ = read_le(data_ + 40, 8); directory_ = read_le(data_ + 48, 8);
            require(count_ <= UINT32_MAX && blocks_ <= 0x1000000, "database exceeds format limits");
            require(index_ == header_size && directory_ == index_ + count_ * index_size &&
                    read_le(data_ + 56, 8) == directory_ + blocks_ * directory_size, "invalid section directory");
            range(index_, count_ * index_size); range(directory_, blocks_ * directory_size);
            auto model_size = read_le(data_ + 64, 4); require(model_size <= 52, "invalid model identifier");
            model_ = std::string(data_ + 68, model_size);
            require(expected_model.empty() || model_ == expected_model, "database belongs to a different model build");
        } catch (...) { close(); throw; }
    }
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    ~Database() { close(); }
    void close() {
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
        if (mapped_) munmap(const_cast<char *>(data_), size_);
#endif
        mapped_ = false; data_ = nullptr;
    }
    const std::string &model() const { return model_; }
    uint64_t size() const { return size_; }
    uint64_t records() const { return count_; }
    size_t cached_bytes() const { return cache_bytes_; }
    std::optional<Value> get(char category, std::string_view name) const {
        auto wanted = key(category, name); auto h = uint32_t(hash(wanted));
        uint64_t low = 0, high = count_;
        while (low < high) {
            auto middle = low + (high - low) / 2;
            if (read_le(data_ + index_ + middle * index_size, 4) < h) low = middle + 1;
            else high = middle;
        }
        for (; low < count_; ++low) {
            const char *p = data_ + index_ + low * index_size;
            if (read_le(p, 4) != h) break;
            auto location = uint32_t(read_le(p + 4, 4));
            auto bytes = block(location >> 8);
            auto [found, payload] = entry(*bytes, location & 255);
            if (found != wanted) continue; // Hash collisions must never alias storage.
            Cursor c{payload}; auto v = Value::decode(c);
            require(c.pos == payload.size(), "trailing value bytes"); return v;
        }
        return {};
    }
    void each(char category, const std::function<void(const std::string &, const Value &)> &visit) const {
        for (uint64_t id = 0; id < blocks_; ++id) {
            auto bytes = block(uint32_t(id)); auto n = read_le(bytes->data(), 4);
            std::string name;
            for (uint32_t i = 0; i < n; ++i) {
                auto payload = next_entry(*bytes, i, name);
                require(name.size() >= 2 && name[1] == ':', "invalid record name");
                if (name[0] != category) continue;
                Cursor c{payload}; auto value = Value::decode(c);
                require(c.pos == payload.size(), "trailing value bytes");
                visit(name.substr(2), value);
            }
        }
    }
    std::vector<std::string> names(std::string_view categories, const std::string &pattern = "") const {
        std::vector<std::string> out;
        // Do not decode values or populate a per-signal map during enumeration.
        for (uint64_t id = 0; id < blocks_; ++id) {
            auto bytes = block(uint32_t(id)); auto n = read_le(bytes->data(), 4);
            std::string name;
            for (uint32_t i = 0; i < n; ++i) {
                next_entry(*bytes, i, name);
                if (name.size() >= 2 && categories.find(name[0]) != std::string_view::npos && name[1] == ':' &&
                    (pattern.empty() || name.find(pattern, 2) != std::string::npos)) out.push_back(name.substr(2));
            }
        }
        return out;
    }
    std::vector<std::string> names(char category, const std::string &pattern = "") const {
        return names(std::string_view(&category, 1), pattern);
    }
};

class Writer {
    std::map<std::string, std::string> entries_;
    std::string model_;
public:
    explicit Writer(std::string model = "") : model_(std::move(model)) { require(model_.size() <= 52, "model identifier too long"); }
    void add(char category, const std::string &name, const Value &value) {
        std::string payload; value.encode(payload); entries_[key(category, name)] = std::move(payload);
    }
    void copy(const Database &db, const std::string &categories) {
        for (char category : categories) db.each(category, [&](const std::string &name, const Value &v) { add(category, name, v); });
    }
    void write(const std::string &path) const {
        require(entries_.size() <= UINT32_MAX, "too many records");
        struct Index { uint32_t hash, block, slot; };
        struct Block { std::string bytes; uint32_t raw, crc, codec; };
        std::vector<Index> indices; std::vector<Block> blocks;
        std::vector<std::string> pending; size_t pending_size = 0; std::string previous;
        auto flush = [&]() {
            if (pending.empty()) return;
            std::string raw; put_le(raw, pending.size(), 4);
            uint64_t offset = 4 + pending.size() * 4;
            for (const auto &p : pending) { put_le(raw, offset, 4); offset += p.size(); }
            require(offset <= max_block_size, "record exceeds block size limit");
            for (const auto &p : pending) raw += p;
            std::string compressed(compressBound(raw.size()), '\0'); uLongf size = compressed.size();
            require(compress2(reinterpret_cast<Bytef *>(compressed.data()), &size,
                              reinterpret_cast<const Bytef *>(raw.data()), raw.size(), 6) == Z_OK, "compression failed");
            compressed.resize(size);
            uint32_t crc = crc32(0, reinterpret_cast<const Bytef *>(raw.data()), raw.size());
            if (compressed.size() < raw.size()) blocks.push_back({std::move(compressed), uint32_t(raw.size()), crc, 1});
            else blocks.push_back({std::move(raw), uint32_t(offset), crc, 0});
            pending.clear(); pending_size = 0; previous.clear();
        };
        for (const auto &[name, payload] : entries_) {
            if (pending.size() == block_entries || (!pending.empty() && pending_size + payload.size() + name.size() >= block_target)) flush();
            size_t prefix = 0;
            while (prefix < std::min(previous.size(), name.size()) && previous[prefix] == name[prefix]) ++prefix;
            std::string record; put_uint(record, prefix); put_uint(record, name.size() - prefix);
            record.append(name, prefix, name.size() - prefix); put_uint(record, payload.size()); record += payload;
            require(record.size() + 8 <= max_block_size && blocks.size() < 0x1000000, "record exceeds format limits");
            indices.push_back({uint32_t(hash(name)), uint32_t(blocks.size()), uint32_t(pending.size())});
            pending_size += record.size(); pending.push_back(std::move(record)); previous = name;
        }
        flush();
        std::sort(indices.begin(), indices.end(), [](const Index &a, const Index &b) {
            return a.hash < b.hash || (a.hash == b.hash &&
                (a.block < b.block || (a.block == b.block && a.slot < b.slot)));
        });
        std::string header(header_size, '\0'); std::memcpy(header.data(), magic, 8);
        set_le(header, 8, format_version, 4); set_le(header, 12, header_size, 4);
        set_le(header, 24, indices.size(), 8); set_le(header, 32, blocks.size(), 8);
        set_le(header, 40, header_size, 8);
        const uint64_t directory = header_size + indices.size() * index_size;
        uint64_t data_offset = directory + blocks.size() * directory_size;
        set_le(header, 48, directory, 8); set_le(header, 56, data_offset, 8);
        set_le(header, 64, model_.size(), 4); header.replace(68, model_.size(), model_);
        std::string index, dirs;
        for (const auto &i : indices) { put_le(index, i.hash, 4); put_le(index, (i.block << 8) | i.slot, 4); }
        for (const auto &b : blocks) {
            put_le(dirs, data_offset, 8); put_le(dirs, b.bytes.size(), 4); put_le(dirs, b.raw, 4);
            put_le(dirs, b.crc, 4); put_le(dirs, b.codec, 4); data_offset += b.bytes.size();
        }
        set_le(header, 16, data_offset, 8);
        set_le(header, 120, crc32(0, reinterpret_cast<const Bytef *>(header.data()), 120), 4);
        // Atomic replacement never truncates an already mapped database. No
        // journal, sidecar index or persistent cache is created.
        std::string temporary = path + ".tmp.XXXXXX";
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
        int fd = mkstemp(temporary.data()); require(fd >= 0, "cannot create temporary database"); ::close(fd);
#else
        temporary = path + ".tmp";
#endif
        try {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            require(bool(out), "cannot create database");
            out.write(header.data(), header.size()); out.write(index.data(), index.size()); out.write(dirs.data(), dirs.size());
            for (const auto &b : blocks) out.write(b.bytes.data(), b.bytes.size());
            out.flush(); require(bool(out), "database write failed"); out.close();
            std::filesystem::rename(temporary, path);
        } catch (...) { std::filesystem::remove(temporary); throw; }
    }
};
} // namespace xspcomm::detail::cfgdb
#endif
