#include "catch.hpp"
#include "xspcomm/xcomm.h"
#include "xspcomm/detail/config_db.h"
#include <chrono>
#include <filesystem>
#include <fstream>

namespace db = xspcomm::detail::cfgdb;
namespace {
struct Fixture {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
        ("xscfg-unit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { std::filesystem::create_directories(dir); }
    ~Fixture() { for (auto &e : std::filesystem::directory_iterator(dir)) std::filesystem::remove(e.path()); std::filesystem::remove(dir); }
    std::string path(const std::string &name = "model.xscfg") { return (dir / name).string(); }
};
db::Value object(std::map<std::string, db::Value> fields) { db::Value v; v.kind = db::Value::Object; v.object = std::move(fields); return v; }
std::string read(const std::string &path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
void write(const std::string &path, const std::string &data) { std::ofstream out(path, std::ios::binary); out.write(data.data(), data.size()); }
}

TEST_CASE("single-file database round trips typed values and compressed names lazily", "[xscfg]") {
    Fixture f;
    db::Writer writer("build-a");
    for (int i = 0; i < 4000; ++i) writer.add('v', "Top.deep.repeated.module.signal_" + std::to_string(i),
        object({{"offset", db::Value::uint(i * 4)}, {"rtl_width", db::Value::uint(32)},
                {"mem_bytes", db::Value::uint(4)}, {"type", db::Value::str("IData")}}));
    writer.add('m', "empty", db::Value::str(""));
    writer.add('m', "unicode", db::Value::str("字段\n\"quoted\""));
    writer.add('m', "negative", db::Value::sint(INT64_MIN));
    writer.add('m', "unsigned", db::Value::uint(UINT64_MAX));
    writer.write(f.path());
    db::Database database(f.path(), "build-a");
    REQUIRE(database.records() == 4004);
    REQUIRE(database.cached_bytes() == 0);
    REQUIRE(database.get('m', "empty")->text.empty());
    REQUIRE(database.get('m', "unicode")->text == "字段\n\"quoted\"");
    REQUIRE(int64_t(database.get('m', "negative")->number) == INT64_MIN);
    REQUIRE(database.get('m', "unsigned")->number == UINT64_MAX);
    REQUIRE(database.get('v', "Top.deep.repeated.module.signal_3999")->object.at("offset").number == 15996);
    REQUIRE_FALSE(database.get('v', "missing"));
    auto names = database.names('v', "signal_399");
    REQUIRE(names.size() == 11);
    REQUIRE(std::is_sorted(names.begin(), names.end()));
    REQUIRE(database.size() < 4000 * 50);
    REQUIRE(database.cached_bytes() <= 2 * 1024 * 1024);
    REQUIRE(std::distance(std::filesystem::directory_iterator(f.dir), std::filesystem::directory_iterator{}) == 1);
    REQUIRE_THROWS_AS(db::Database(f.path(), "different-build"), std::invalid_argument);
    // Replacing the file is atomic; readers keep the original mapped inode.
    db::Writer replacement("build-b"); replacement.add('m', "empty", db::Value::str("new")); replacement.write(f.path());
    REQUIRE(database.get('m', "empty")->text.empty());
    REQUIRE(db::Database(f.path(), "build-b").get('m', "empty")->text == "new");
}

TEST_CASE("database validates headers, block boundaries, checksums and value encoding", "[xscfg]") {
    Fixture f;
    db::Writer writer("test"); writer.add('v', "a", db::Value::str(std::string(4096, 'x'))); writer.write(f.path());
    auto original = read(f.path());
    for (size_t length : {size_t(0), size_t(16), original.size() - 1}) {
        write(f.path("bad.xscfg"), original.substr(0, length));
        REQUIRE_THROWS_AS(db::Database(f.path("bad.xscfg")), std::invalid_argument);
    }
    auto corrupt = original; corrupt[8] = 42; write(f.path("bad.xscfg"), corrupt);
    REQUIRE_THROWS_AS(db::Database(f.path("bad.xscfg")), std::invalid_argument);
    corrupt = original; corrupt.back() ^= 0x40; write(f.path("bad.xscfg"), corrupt);
    db::Database bad_block(f.path("bad.xscfg"));
    REQUIRE_THROWS_AS(bad_block.get('v', "a"), std::invalid_argument);
    corrupt = original;
    const auto directory = db::read_le(corrupt.data() + 48, 8);
    db::set_le(corrupt, directory, UINT64_MAX, 8); write(f.path("bad-range.xscfg"), corrupt);
    db::Database bad_range(f.path("bad-range.xscfg"));
    REQUIRE_THROWS_AS(bad_range.get('v', "a"), std::invalid_argument);
    db::Cursor invalid{std::string_view("\x03\x80", 2)};
    REQUIRE_THROWS_AS(db::Value::decode(invalid), std::invalid_argument);
    db::Cursor overflow{std::string_view("\x03\xff\xff\xff\xff\xff\xff\xff\xff\xff\x02", 11)};
    REQUIRE_THROWS_AS(db::Value::decode(overflow), std::invalid_argument);
}

TEST_CASE("database hash collisions never alias distinct signal addresses", "[xscfg]") {
    Fixture f;
    const std::string a = "Top.s_12c300fc843ccc01", b = "Top.s_6e0338135e8e4467";
    REQUIRE(uint32_t(db::hash(db::key('v', a))) == uint32_t(db::hash(db::key('v', b))));
    db::Writer writer;
    writer.add('v', a, db::Value::uint(123)); writer.add('v', b, db::Value::uint(456)); writer.write(f.path());
    db::Database database(f.path());
    REQUIRE(database.get('v', a)->number == 123);
    REQUIRE(database.get('v', b)->number == 456);
}

TEST_CASE("a corrupt derived override cannot expose a native fallback on retry", "[xscfg]") {
    Fixture f;
    db::Writer writer("model");
    writer.add('v', "x", object({{"offset", db::Value::uint(0)}, {"mem_bytes", db::Value::uint(4)}, {"rtl_width", db::Value::uint(32)}}));
    writer.add('s', "x", object({{"kind", db::Value::str("unavailable")}, {"padding", db::Value::str(std::string(70000, 'x'))}}));
    writer.write(f.path());
    auto bytes = read(f.path());
    const auto directory = db::read_le(bytes.data() + 48, 8);
    const auto block = db::read_le(bytes.data() + directory, 8);
    const auto size = db::read_le(bytes.data() + directory + 8, 4);
    bytes.at(block + size - 1) ^= 0x40; write(f.path("bad.xscfg"), bytes);
    uint32_t storage = 0;
    xspcomm::XSignalCFG cfg(f.path("bad.xscfg"), reinterpret_cast<uint64_t>(&storage), "model");
    REQUIRE_THROWS_AS(cfg.NewXView("x"), std::invalid_argument);
    REQUIRE_THROWS_AS(cfg.NewXView("x"), std::invalid_argument);
}

TEST_CASE("XSignalCFG reads only requested database records and preserves recursive native types", "[xscfg]") {
    Fixture f;
    db::Writer writer("model");
    writer.add('v', "Top.packet", object({{"offset", db::Value::uint(0)}, {"mem_bytes", db::Value::uint(4)},
        {"rtl_width", db::Value::uint(32)}, {"layout_id", db::Value::str("packet")}}));
    writer.add('t', "byte", object({{"kind", db::Value::str("bits")}, {"packed", db::Value{db::Value::True}}, {"width", db::Value::uint(8)}}));
    writer.add('t', "word", object({{"kind", db::Value::str("bits")}, {"packed", db::Value{db::Value::True}}, {"width", db::Value::uint(24)}}));
    db::Value fields; fields.kind = db::Value::Array;
    fields.array = {object({{"name", db::Value::str("tag")}, {"offset", db::Value::uint(24)}, {"type", db::Value::str("byte")}}),
                    object({{"name", db::Value::str("data")}, {"offset", db::Value::uint(0)}, {"type", db::Value::str("word")}})};
    writer.add('t', "packet", object({{"kind", db::Value::str("struct")}, {"packed", db::Value{db::Value::True}},
        {"width", db::Value::uint(32)}, {"fields", fields}}));
    writer.add('m', "ports", db::Value::str("metadata"));
    writer.add('s', "constant", object({{"kind", db::Value::str("const")}, {"rtl_width", db::Value::uint(8)}, {"value", db::Value::str("42")}}));
    writer.add('s', "temporary", object({{"kind", db::Value::str("unavailable")}}));
    writer.write(f.path());
    uint32_t native = 0x12345678;
    xspcomm::XSignalCFG cfg(f.path(), reinterpret_cast<uint64_t>(&native), "model");
    REQUIRE(cfg.Metadata("ports") == "\"metadata\"");
    auto view = cfg.NewXView("Top.packet");
    REQUIRE(view.Struct()->Data("tag")->U() == 0x12);
    view.Struct()->Data("tag")->Set(0xab);
    REQUIRE(native == 0xab345678);
    REQUIRE(cfg.NewXView("Top.packet.tag").Data() == view.Struct()->Data("tag"));
    REQUIRE(cfg.GetSignalNames() == std::vector<std::string>{"Top.packet", "constant", "temporary"});
    auto constant = std::unique_ptr<xspcomm::XData>(cfg.NewXData("constant"));
    REQUIRE(constant->U() == 42);
    REQUIRE(cfg.NewXData("temporary") == nullptr);
    xspcomm::XSignalCFG wrong(f.path(), reinterpret_cast<uint64_t>(&native), "other");
    REQUIRE_THROWS_AS(wrong.NewXView("Top.packet"), std::invalid_argument);
    REQUIRE_THROWS_AS(wrong.NewXView("Top.packet"), std::invalid_argument);
}

TEST_CASE("configuration associates port types without a native base or replacing leaves", "[xscfg][layout]") {
    Fixture f;
    db::Writer writer("ports");
    const db::Value packed{db::Value::True};
    writer.add('t', "bit", object({{"kind", db::Value::str("bits")}, {"packed", packed}, {"width", db::Value::uint(0)}}));
    writer.add('t', "byte", object({{"kind", db::Value::str("array")}, {"packed", packed}, {"width", db::Value::uint(8)},
        {"left", db::Value::uint(7)}, {"right", db::Value::uint(0)}, {"element", db::Value::str("bit")}}));
    db::Value fields; fields.kind = db::Value::Array;
    fields.array = {object({{"name", db::Value::str("payload")}, {"offset", db::Value::uint(1)}, {"type", db::Value::str("byte")}}),
                    object({{"name", db::Value::str("valid")}, {"offset", db::Value::uint(0)}, {"type", db::Value::str("bit")}})};
    writer.add('t', "packet", object({{"kind", db::Value::str("struct")}, {"packed", packed}, {"width", db::Value::uint(9)}, {"fields", fields}}));
    writer.add('m', "port_types", object({{"p", db::Value::str("packet")}, {"other", db::Value::str("packet")}}));
    writer.write(f.path());
    xspcomm::XData first(9, xspcomm::IOType::Input), second(9, xspcomm::IOType::Input);
    xspcomm::XPort port;
    port.Add("p", first);
    {
        xspcomm::XSignalCFG cfg(f.path(), 0, "ports");
        REQUIRE_THROWS_AS(cfg.BindDataLayouts(port), std::invalid_argument);
        REQUIRE_FALSE(first.HasLayout()); // The entire association is validated first.
        port.Add("other", second);
        cfg.BindDataLayouts(port);
        REQUIRE(first.HasLayout());
        REQUIRE(second.HasLayout());
        REQUIRE(cfg.NewViewFromPort(port, "p").PackedData().get() == &first);
        first.StructView()->Data("payload")->Set(0x5a);
        REQUIRE(first.U() == 0xb4);
        REQUIRE(second.U() == 0);
        REQUIRE(port.PortCount() == 2);
    }
    first.StructView()->Data("valid")->Set(1);
    REQUIRE(first.U() == 0xb5);
    REQUIRE(first.StructView()->Data("payload")->ArrayView()->Left() == 7);
}
