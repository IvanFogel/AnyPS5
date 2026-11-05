#include "prx/libharfbuzz/libharfbuzz.hpp"

#include <hb.h>
#include <hb-ot.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <source_location>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HarfBuzz shaping check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name) {
#ifdef _WIN32
    const auto address = GetProcAddress(module, name);
#else
    const auto address = dlsym(module, name);
#endif
    if (!address) std::fprintf(stderr, "Missing symbol: %s\n", name);
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct Api {
    decltype(&hb_blob_create_nid_postfix) blob;
    decltype(&hb_face_create_for_tables_nid_postfix) face;
    decltype(&hb_face_destroy_nid_postfix) destroyFace;
    decltype(&hb_ot_layout_table_get_script_tags_nid_postfix) scripts;
    decltype(&hb_ot_layout_script_get_language_tags_nid_postfix) languages;
    decltype(&hb_ot_layout_language_find_feature_nid_postfix) feature;
    decltype(&hb_shape_nid_postfix) shape;
    decltype(&hb_font_create_nid_postfix) font;
    decltype(&hb_font_destroy_nid_postfix) destroyFont;
    decltype(&hb_font_set_scale_nid_postfix) scale;
    decltype(&hb_font_funcs_create_nid_postfix) functions;
    decltype(&hb_font_funcs_set_nominal_glyph_func_nid_postfix) nominal;
    decltype(&hb_font_funcs_set_glyph_h_advance_func_nid_postfix) advance;
    decltype(&hb_font_set_funcs_nid_postfix) setFunctions;
    decltype(&hb_buffer_create_nid_postfix) buffer;
    decltype(&hb_buffer_destroy_nid_postfix) destroyBuffer;
    decltype(&hb_buffer_reset_nid_postfix) reset;
    decltype(&hb_buffer_add_utf16_nid_postfix) text;
    decltype(&hb_buffer_guess_segment_properties_nid_postfix) guess;
    decltype(&hb_buffer_get_glyph_infos_nid_postfix) infos;
    decltype(&hb_buffer_get_glyph_positions_nid_postfix) positions;
};

static void APS5_VABI Increment(void* opaque) {
    ++*static_cast<unsigned*>(opaque);
}

static void CheckBlobs(const Api& api) {
    for (std::uint32_t mode = 0; mode != 4; ++mode) {
        std::array<char, 5> bytes{'a', '\0', 'b', 'c', 'd'};
        unsigned destroyed = 0;
        auto* blob = api.blob(bytes.data(), bytes.size(), mode, &destroyed, Increment);
        Require(blob != nullptr && hb_blob_get_length(blob) == bytes.size());
        unsigned length = 0;
        const auto* data = hb_blob_get_data(blob, &length);
        Require(length == bytes.size() && std::memcmp(data, bytes.data(), bytes.size()) == 0);
        if (mode == HB_MEMORY_MODE_DUPLICATE) {
            Require(data != bytes.data() && destroyed == 1);
            bytes[0] = 'z';
            Require(data[0] == 'a');
        } else {
            Require(data == bytes.data() && destroyed == 0);
        }
        auto* retained = hb_blob_reference(blob);
        hb_blob_destroy(blob);
        Require(destroyed == (mode == HB_MEMORY_MODE_DUPLICATE ? 1u : 0u));
        hb_blob_destroy(retained);
        Require(destroyed == 1);
    }
    for (unsigned length : {0u, 0x80000000u, std::numeric_limits<unsigned>::max()}) {
        unsigned destroyed = 0;
        auto* blob = api.blob(nullptr, length, HB_MEMORY_MODE_READONLY, &destroyed, Increment);
        Require(blob != nullptr && hb_blob_get_length(blob) == 0 && destroyed == 1);
        hb_blob_destroy(blob);
        Require(destroyed == 1);
    }
    auto* blob = api.blob("abc", 3, HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    auto* writable = hb_blob_get_data_writable(blob, nullptr);
    Require(writable != nullptr && std::memcmp(writable, "abc", 3) == 0);
    writable[0] = 'z';
    Require(hb_blob_get_data(blob, nullptr)[0] == 'z');
    hb_blob_destroy(blob);
}

struct TableBlob {
    hb_blob_t* source;
    unsigned* destroyed;
};

struct Tables {
    const Api* api;
    hb_face_t* source;
    hb_face_t* face = nullptr;
    unsigned requested = 0;
    unsigned created = 0;
    unsigned released = 0;
    unsigned destroyed = 0;
};

static void APS5_VABI ReleaseTable(void* opaque) {
    auto* table = static_cast<TableBlob*>(opaque);
    ++*table->destroyed;
    hb_blob_destroy(table->source);
    delete table;
}

static hb_blob_t* APS5_VABI ReferenceTable(hb_face_t* face, std::uint32_t tag, void* opaque) {
    auto& tables = *static_cast<Tables*>(opaque);
    Require(tables.destroyed == 0 && (!tables.face || tables.face == face));
    ++tables.requested;
    auto* source = hb_face_reference_table(tables.source, tag);
    unsigned length = 0;
    const auto* bytes = hb_blob_get_data(source, &length);
    auto* owner = new TableBlob{source, &tables.released};
    ++tables.created;
    return tables.api->blob(bytes, length, HB_MEMORY_MODE_READONLY, owner, ReleaseTable);
}

static void APS5_VABI DestroyTables(void* opaque) {
    auto& tables = *static_cast<Tables*>(opaque);
    Require(tables.destroyed == 0);
    ++tables.destroyed;
    hb_face_destroy(tables.source);
}

static hb_face_t* LoadFace(const std::filesystem::path& path) {
    auto* blob = hb_blob_create_from_file_or_fail(path.string().c_str());
    Require(blob != nullptr && hb_blob_get_length(blob) != 0);
    auto* face = hb_face_create(blob, 0);
    hb_blob_destroy(blob);
    Require(hb_face_get_glyph_count(face) != 0);
    return face;
}

static void CheckFace(const Api& api, const std::filesystem::path& fontPath) {
    unsigned destroyed = 0;
    auto* empty = api.face(nullptr, &destroyed, Increment);
    Require(empty != nullptr && destroyed == 1 && hb_face_get_glyph_count(empty) == 0);
    api.destroyFace(empty);
    api.destroyFace(nullptr);
    for (unsigned iteration = 0; iteration != 8; ++iteration) {
        Tables tables{&api, LoadFace(fontPath)};
        tables.face = api.face(ReferenceTable, &tables, DestroyTables);
        Require(tables.face != nullptr && tables.destroyed == 0);
        auto* font = api.font(tables.face);
        Require(hb_face_get_glyph_count(tables.face) == hb_face_get_glyph_count(tables.source));
        Require(hb_face_get_upem(tables.face) == hb_face_get_upem(tables.source));
        auto* missing = hb_face_reference_table(tables.face, HB_TAG('n', 'o', 'n', 'e'));
        Require(hb_blob_get_length(missing) == 0);
        hb_blob_destroy(missing);
        api.destroyFace(tables.face);
        Require(tables.destroyed == 0);
        api.destroyFont(font);
        Require(tables.destroyed == 1 && tables.requested != 0);
        Require(tables.created == tables.released);
    }
}

static void CheckDestroy(const Api& api) {
    auto* face = hb_face_create_for_tables(nullptr, nullptr, nullptr);
    api.destroyFace(face);
    api.destroyFace(nullptr);
    unsigned destroyed = 0;
    auto* owner = hb_face_create_for_tables([](hb_face_t*, hb_tag_t, void*) { return hb_blob_get_empty(); }, &destroyed, [](void* p) { ++*static_cast<unsigned*>(p); });
    auto* retained = hb_face_reference(owner);
    api.destroyFace(owner);
    Require(destroyed == 0);
    api.destroyFace(retained);
    Require(destroyed == 1);
}

static void CheckScripts(const Api& api, hb_face_t* face) {
    for (auto tag : {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS, HB_TAG('n', 'o', 'n', 'e')}) {
        const auto total = hb_ot_layout_table_get_script_tags(face, tag, 0, nullptr, nullptr);
        Require(api.scripts(face, tag, 0, nullptr, nullptr) == total);
        for (unsigned start : {0u, 1u, total, total + 1, std::numeric_limits<unsigned>::max()}) {
            for (unsigned capacity : {0u, 1u, 3u, 32u}) {
                std::array<std::uint32_t, 34> actual, expected;
                actual.fill(0xDEADBEEFu);
                expected = actual;
                unsigned count = capacity, expectedCount = capacity;
                const auto result = api.scripts(face, tag, start, &count, actual.data() + 1);
                const auto reference = hb_ot_layout_table_get_script_tags(face, tag, start, &expectedCount, expected.data() + 1);
                Require(result == reference && count == expectedCount && actual == expected);
            }
        }
    }
    unsigned count = 32;
    std::array<hb_tag_t, 32> tags{};
    api.scripts(face, HB_OT_TAG_GSUB, 0, &count, tags.data());
    Require(count != 0 && std::find(tags.begin(), tags.begin() + count, HB_TAG('l', 'a', 't', 'n')) != tags.begin() + count);
}

static void CheckLanguages(const Api& api, hb_face_t* face) {
    for (auto tag : {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS, HB_TAG('n', 'o', 'n', 'e')}) {
        const auto scripts = hb_ot_layout_table_get_script_tags(face, tag, 0, nullptr, nullptr);
        for (unsigned script = 0; script <= scripts + 1; ++script) {
            const auto total = hb_ot_layout_script_get_language_tags(face, tag, script, 0, nullptr, nullptr);
            Require(api.languages(face, tag, script, 0, nullptr, nullptr) == total);
            for (unsigned start : {0u, 1u, total, std::numeric_limits<unsigned>::max()}) {
                for (unsigned capacity : {0u, 1u, 3u, 32u}) {
                    std::array<std::uint32_t, 34> actual, expected;
                    actual.fill(0xCAFEBABEu);
                    expected = actual;
                    unsigned count = capacity, expectedCount = capacity;
                    const auto result = api.languages(face, tag, script, start, &count, actual.data() + 1);
                    const auto reference = hb_ot_layout_script_get_language_tags(face, tag, script, start, &expectedCount, expected.data() + 1);
                    Require(result == reference && count == expectedCount && actual == expected);
                }
            }
        }
    }
}

static void CheckFeatures(const Api& api, hb_face_t* face) {
    bool foundLiga = false;
    for (auto tag : {HB_OT_TAG_GSUB, HB_OT_TAG_GPOS, HB_TAG('n', 'o', 'n', 'e')}) {
        const auto scripts = hb_ot_layout_table_get_script_tags(face, tag, 0, nullptr, nullptr);
        for (unsigned script = 0; script <= scripts; ++script) {
            const auto languages = hb_ot_layout_script_get_language_tags(face, tag, script, 0, nullptr, nullptr);
            for (unsigned language = 0; language <= languages + 1; ++language) {
                const auto index = language == languages + 1 ? HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX : language;
                for (auto feature : {HB_TAG('l', 'i', 'g', 'a'), HB_TAG('k', 'e', 'r', 'n'), HB_TAG('n', 'o', 'n', 'e')}) {
                    unsigned actual = 0x12345678, expected = actual;
                    const auto result = api.feature(face, tag, script, index, feature, &actual);
                    const auto reference = hb_ot_layout_language_find_feature(face, tag, script, index, feature, &expected);
                    Require(result == reference && actual == expected);
                    Require(api.feature(face, tag, script, index, feature, nullptr) == reference);
                    if (!result) Require(actual == HB_OT_LAYOUT_NO_FEATURE_INDEX);
                    if (result && feature == HB_TAG('l', 'i', 'g', 'a')) foundLiga = true;
                }
            }
        }
    }
    Require(foundLiga);
}

static unsigned Shape(const Api& api, hb_font_t* font, hb_font_t* referenceFont, hb_buffer_t* actual, const std::u16string& text, const std::vector<hb_feature_t>& features) {
    api.reset(actual);
    api.text(actual, reinterpret_cast<const std::uint16_t*>(text.data()), static_cast<int>(text.size()), 0, -1);
    api.guess(actual);
    auto* expected = hb_buffer_create();
    hb_buffer_add_utf16(expected, reinterpret_cast<const std::uint16_t*>(text.data()), static_cast<int>(text.size()), 0, -1);
    hb_buffer_guess_segment_properties(expected);
    api.shape(font, actual, features.data(), static_cast<unsigned>(features.size()));
    hb_shape(referenceFont, expected, features.data(), static_cast<unsigned>(features.size()));
    unsigned actualCount = 0, expectedCount = 0;
    const auto* infos = api.infos(actual, &actualCount);
    const auto* referenceInfos = hb_buffer_get_glyph_infos(expected, &expectedCount);
    Require(actualCount == expectedCount);
    unsigned positionCount = 0;
    const auto* positions = api.positions(actual, &positionCount);
    const auto* referencePositions = hb_buffer_get_glyph_positions(expected, nullptr);
    Require(positionCount == actualCount);
    for (unsigned i = 0; i < actualCount; ++i) {
        Require(infos[i].codepoint == referenceInfos[i].codepoint && infos[i].cluster == referenceInfos[i].cluster && infos[i].mask == referenceInfos[i].mask);
        Require(positions[i].x_advance == referencePositions[i].x_advance && positions[i].y_advance == referencePositions[i].y_advance);
        Require(positions[i].x_offset == referencePositions[i].x_offset && positions[i].y_offset == referencePositions[i].y_offset);
    }
    hb_buffer_destroy(expected);
    return actualCount;
}

static int APS5_VABI Nominal(hb_font_t*, void* data, std::uint32_t unicode, std::uint32_t* glyph, void* callbackData) {
    Require(data == callbackData);
    ++*static_cast<unsigned*>(data);
    *glyph = unicode + 1;
    return 1;
}

static std::int32_t APS5_VABI Advance(hb_font_t*, void* data, std::uint32_t glyph, void* callbackData) {
    Require(data == callbackData && glyph != 0);
    ++*static_cast<unsigned*>(data);
    return 100 + static_cast<std::int32_t>(glyph);
}

static void CheckShaping(const Api& api, const std::filesystem::path& root) {
    auto* emptyFont = hb_font_create(nullptr);
    auto* emptyBuffer = hb_buffer_create();
    api.shape(emptyFont, emptyBuffer, nullptr, 0);
    Require(hb_buffer_get_length(emptyBuffer) == 0);
    hb_buffer_destroy(emptyBuffer);
    hb_font_destroy(emptyFont);
    for (const auto& file : {"test/subset/data/fonts/Roboto-Regular.ttf", "test/shape/data/in-house/fonts/872d2955d326bd6676a06f66b8238ebbaabc212f.ttf"}) {
        Tables tables{&api, LoadFace(root / file)};
        tables.face = api.face(ReferenceTable, &tables, DestroyTables);
        auto* font = api.font(tables.face);
        auto* referenceFont = hb_font_create(tables.source);
        auto* buffer = api.buffer();
        api.scale(font, 1000, 1000);
        hb_font_set_scale(referenceFont, 1000, 1000);
        const std::vector<std::u16string> texts{u"", u"office affinity", u"AVATAR", u"a\u0301", u"\u0628\u0622", u"\u0628\u0627\u0653", u"\u0628\u0628\u0628", u"\U0001F600 A", std::u16string{u'A', 0, u'B'}};
        for (const auto& text : texts) {
            Shape(api, font, referenceFont, buffer, text, {});
            Shape(api, font, referenceFont, buffer, text, {{HB_TAG('l', 'i', 'g', 'a'), 0, 0, std::numeric_limits<unsigned>::max()}});
            Shape(api, font, referenceFont, buffer, text, {{HB_TAG('k', 'e', 'r', 'n'), 0, 0, std::numeric_limits<unsigned>::max()}, {HB_TAG('l', 'i', 'g', 'a'), 0, 1, 3}});
        }
        if (std::string(file).find("Roboto") != std::string::npos) {
            Require(Shape(api, font, referenceFont, buffer, u"fi", {}) == 1);
            Require(Shape(api, font, referenceFont, buffer, u"fi", {{HB_TAG('l', 'i', 'g', 'a'), 0, 0, std::numeric_limits<unsigned>::max()}}) == 2);
        } else {
            Require(Shape(api, font, referenceFont, buffer, u"\u0628\u0628\u0628", {}) == 3);
            const auto* infos = api.infos(buffer, nullptr);
            Require(infos[0].cluster == 2 && infos[2].cluster == 0);
            Require(infos[0].codepoint != infos[1].codepoint && infos[1].codepoint != infos[2].codepoint);
        }
        api.destroyFace(tables.face);
        Require(tables.destroyed == 0);
        Shape(api, font, referenceFont, buffer, u"AV", {});
        api.destroyBuffer(buffer);
        hb_font_destroy(referenceFont);
        api.destroyFont(font);
        Require(tables.destroyed == 1 && tables.created == tables.released);
    }
    auto* face = api.face(nullptr, nullptr, nullptr);
    auto* font = api.font(face);
    api.destroyFace(face);
    auto* functions = api.functions();
    unsigned calls = 0;
    api.nominal(functions, Nominal, &calls, nullptr);
    api.advance(functions, Advance, &calls, nullptr);
    api.setFunctions(font, functions, &calls, nullptr);
    hb_font_funcs_destroy(functions);
    auto* buffer = api.buffer();
    const std::uint16_t text[]{'A', 'B'};
    api.text(buffer, text, 2, 0, -1);
    api.guess(buffer);
    api.shape(font, buffer, nullptr, 0);
    unsigned count = 0;
    const auto* infos = api.infos(buffer, &count);
    const auto* positions = api.positions(buffer, nullptr);
    Require(count == 2 && calls >= 4);
    Require(infos[0].codepoint == 'A' + 1 && infos[1].codepoint == 'B' + 1);
    Require(positions[0].x_advance == 'A' + 101 && positions[1].x_advance == 'B' + 101);
    api.destroyBuffer(buffer);
    api.destroyFont(font);
}

int main(int argc, char** argv) {
    Require(argc == 5);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    Require(patched || std::strcmp(argv[2], "unpatched") == 0);
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    Api api{};
#define LoadExport(field, name, nid) api.field = Import<decltype(api.field)>(module, patched ? nid : #name "_nid_postfix")
    LoadExport(blob, hb_blob_create, "hj1KIIN5D54");
    LoadExport(face, hb_face_create_for_tables, "l7GEYs5PMVU");
    LoadExport(destroyFace, hb_face_destroy, "ULNHQXciJ3s");
    LoadExport(scripts, hb_ot_layout_table_get_script_tags, "wf6CfVJj9PQ");
    LoadExport(languages, hb_ot_layout_script_get_language_tags, "HH-sRSNiSaU");
    LoadExport(feature, hb_ot_layout_language_find_feature, "qos3JIrKrDg");
    LoadExport(shape, hb_shape, "dM3hP4y99rM");
    LoadExport(font, hb_font_create, "FVhYusaWqA4");
    LoadExport(destroyFont, hb_font_destroy, "-8t2aEtFws4");
    LoadExport(scale, hb_font_set_scale, "eG5EI3x6twg");
    LoadExport(functions, hb_font_funcs_create, "8ZddTpSAKIg");
    LoadExport(nominal, hb_font_funcs_set_nominal_glyph_func, "88WaPmXp6x8");
    LoadExport(advance, hb_font_funcs_set_glyph_h_advance_func, "SfmbF51JL1Y");
    LoadExport(setFunctions, hb_font_set_funcs, "WH6phgWx6TI");
    LoadExport(buffer, hb_buffer_create, "7f13XmbjTZo");
    LoadExport(destroyBuffer, hb_buffer_destroy, "K-y1p7Z9THQ");
    LoadExport(reset, hb_buffer_reset, "PEggcTbDlfE");
    LoadExport(text, hb_buffer_add_utf16, "P+hgu937lFs");
    LoadExport(guess, hb_buffer_guess_segment_properties, "DcrJr531A4w");
    LoadExport(infos, hb_buffer_get_glyph_infos, "ENsHmJCNG4Q");
    LoadExport(positions, hb_buffer_get_glyph_positions, "43mnfTQPCbw");
#undef LoadExport
    const std::filesystem::path root(argv[4]);
    const std::string mode(argv[3]);
    try {
        if (mode == "blob") CheckBlobs(api);
        else if (mode == "face") CheckFace(api, root / "test/subset/data/fonts/Roboto-Regular.ttf");
        else if (mode == "destroy") CheckDestroy(api);
        else if (mode == "shape") CheckShaping(api, root);
        else {
            auto* face = LoadFace(root / "test/subset/data/fonts/Roboto-Regular.ttf");
            if (mode == "scripts") CheckScripts(api, face);
            else if (mode == "languages") CheckLanguages(api, face);
            else if (mode == "features") CheckFeatures(api, face);
            else Require(false);
            hb_face_destroy(face);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", mode.c_str(), error.what());
        return 1;
    }
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
}
