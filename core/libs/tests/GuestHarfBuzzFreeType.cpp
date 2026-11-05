#include "prx/libharfbuzz/libharfbuzz.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"

#include <hb.h>
#include <hb-ft.h>

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
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
        std::fprintf(stderr, "HarfBuzz FreeType check failed at line %u\n", location.line());
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
    decltype(&hb_ft_face_create_cached_nid_postfix) cached;
    decltype(&hb_face_destroy_nid_postfix) destroyFace;
    decltype(&hb_font_create_nid_postfix) font;
    decltype(&hb_font_destroy_nid_postfix) destroyFont;
    decltype(&hb_font_set_scale_nid_postfix) scale;
    decltype(&hb_shape_nid_postfix) shape;
    decltype(&hb_buffer_create_nid_postfix) buffer;
    decltype(&hb_buffer_destroy_nid_postfix) destroyBuffer;
    decltype(&hb_buffer_add_utf16_nid_postfix) text;
    decltype(&hb_buffer_guess_segment_properties_nid_postfix) guess;
    decltype(&hb_buffer_get_glyph_infos_nid_postfix) infos;
    decltype(&hb_buffer_get_glyph_positions_nid_postfix) positions;
    decltype(&FT_Init_FreeType_nid_postfix) init;
    decltype(&FT_Done_FreeType_nid_postfix) doneLibrary;
    decltype(&FT_New_Memory_Face_nid_postfix) memory;
    decltype(&FT_Done_Face_nid_postfix) doneFace;
    decltype(&FT_Reference_Face_nid_postfix) referenceFace;
    decltype(&FT_Set_Pixel_Sizes_nid_postfix) size;
    decltype(&FT_Get_Char_Index_nid_postfix) charIndex;
    decltype(&FT_Load_Glyph_nid_postfix) glyph;
};

struct Library {
    const Api& api;
    FreeType::GuestLibrary* guest = nullptr;
    FT_Library native = nullptr;

    explicit Library(const Api& functions) : api(functions) {
        Require(api.init(&guest) == 0 && FT_Init_FreeType(&native) == 0);
    }
    ~Library() {
        if (guest) Require(api.doneLibrary(guest) == 0);
        Require(FT_Done_FreeType(native) == 0);
    }
    FreeType::GuestFaceRec* Face(const std::vector<unsigned char>& bytes, std::int64_t index = 0) {
        FreeType::GuestFaceRec* face = nullptr;
        Require(api.memory(guest, bytes.data(), static_cast<std::int64_t>(bytes.size()), index, &face) == 0);
        Require(face != nullptr);
        return face;
    }
    FT_Face Native(const std::vector<unsigned char>& bytes, FT_Long index = 0) {
        FT_Face face = nullptr;
        Require(FT_New_Memory_Face(native, bytes.data(), static_cast<FT_Long>(bytes.size()), index, &face) == 0);
        return face;
    }
};

static std::vector<unsigned char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open());
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    Require(!file.bad() && !bytes.empty());
    return bytes;
}

static void Increment(void* opaque) {
    ++*static_cast<unsigned*>(opaque);
}

static void CountDestruction(hb_face_t* face, hb_user_data_key_t& key, unsigned& destroyed) {
    Require(hb_face_set_user_data(face, &key, &destroyed, Increment, false));
}

static void CheckCache(const Api& api, const std::filesystem::path& root) {
    const auto bytes = Read(root / "test/subset/data/fonts/Roboto-Regular.ttf");
    Library library(api);
    auto* face = library.Face(bytes);
    auto* cached = api.cached(face);
    Require(cached != nullptr && face->generic.data == cached && face->generic.finalizer != nullptr);
    const auto finalizer = face->generic.finalizer;
    hb_user_data_key_t key{};
    unsigned destroyed = 0;
    CountDestruction(cached, key, destroyed);
    for (unsigned i = 0; i < 32; ++i) {
        auto* another = api.cached(face);
        Require(another == cached && face->generic.finalizer == finalizer);
        api.destroyFace(another);
        Require(destroyed == 0);
    }
    api.destroyFace(cached);
    Require(destroyed == 0);
    cached = api.cached(face);
    Require(cached == face->generic.data && destroyed == 0);
    Require(api.size(face, 0, 16) == 0);
    const auto glyph = api.charIndex(face, 'A');
    Require(glyph != 0 && api.glyph(face, glyph, 0) == 0);
    Require(face->glyph != nullptr && face->glyph->face == face);
    Require(api.referenceFace(face) == 0);
    api.destroyFace(cached);
    Require(api.doneFace(face) == 0 && destroyed == 0);
    Require(api.cached(face) == cached);
    api.destroyFace(cached);
    Require(api.doneFace(face) == 0 && destroyed == 1);

    auto* first = library.Face(bytes);
    auto* second = library.Face(bytes);
    auto* firstCache = api.cached(first);
    auto* secondCache = api.cached(second);
    Require(firstCache != secondCache);
    unsigned firstDestroyed = 0, secondDestroyed = 0;
    hb_user_data_key_t firstKey{}, secondKey{};
    CountDestruction(firstCache, firstKey, firstDestroyed);
    CountDestruction(secondCache, secondKey, secondDestroyed);
    api.destroyFace(firstCache);
    api.destroyFace(secondCache);
    Require(api.doneFace(first) == 0 && firstDestroyed == 1 && secondDestroyed == 0);
    Require(api.cached(second) == secondCache);
    api.destroyFace(secondCache);
    Require(api.doneLibrary(library.guest) == 0);
    library.guest = nullptr;
    Require(secondDestroyed == 1);
}

struct Previous {
    FreeType::GuestFaceRec* face;
    unsigned calls = 0;
};

static void APS5_VABI PreviousFinalizer(void* opaque) {
    auto* face = static_cast<FreeType::GuestFaceRec*>(opaque);
    auto* previous = static_cast<Previous*>(face->generic.data);
    Require(previous != nullptr && previous->face == face);
    ++previous->calls;
}

static unsigned nullFinalizerCalls = 0;

static void APS5_VABI NullFinalizer(void* opaque) {
    Require(static_cast<FreeType::GuestFaceRec*>(opaque)->generic.data == nullptr);
    ++nullFinalizerCalls;
}

static void CheckGeneric(const Api& api, const std::filesystem::path& root) {
    const auto bytes = Read(root / "test/subset/data/fonts/Roboto-Regular.ttf");
    Library library(api);
    auto* face = library.Face(bytes);
    Previous previous{face};
    face->generic = {&previous, PreviousFinalizer};
    auto* first = api.cached(face);
    Require(previous.calls == 1 && face->generic.data == first);
    auto* again = api.cached(face);
    Require(again == first && previous.calls == 1);
    api.destroyFace(again);
    hb_user_data_key_t key{};
    unsigned firstDestroyed = 0;
    CountDestruction(first, key, firstDestroyed);
    const auto finalizer = face->generic.finalizer;
    finalizer(face);
    Require(firstDestroyed == 0);
    face->generic.data = nullptr;
    auto* second = api.cached(face);
    Require(second != first && face->generic.data == second && face->generic.finalizer == finalizer);
    api.destroyFace(first);
    Require(firstDestroyed == 1 && previous.calls == 1);
    api.destroyFace(second);
    Require(api.doneFace(face) == 0 && previous.calls == 1);

    face = library.Face(bytes);
    nullFinalizerCalls = 0;
    face->generic = {nullptr, NullFinalizer};
    first = api.cached(face);
    Require(nullFinalizerCalls == 1);
    api.destroyFace(first);
    Require(api.doneFace(face) == 0 && nullFinalizerCalls == 1);

    face = library.Face(bytes);
    face->generic = {&previous, nullptr};
    first = api.cached(face);
    Require(previous.calls == 1 && face->generic.data == first);
    api.destroyFace(first);
    Require(api.doneFace(face) == 0 && previous.calls == 1);
}

static void CompareFace(hb_face_t* actual, hb_face_t* expected) {
    Require(hb_face_get_index(actual) == hb_face_get_index(expected));
    Require(hb_face_get_upem(actual) == hb_face_get_upem(expected));
    Require(hb_face_get_glyph_count(actual) == hb_face_get_glyph_count(expected));
    for (auto tag : {HB_TAG_NONE, HB_TAG('h', 'e', 'a', 'd'), HB_TAG('c', 'm', 'a', 'p'), HB_TAG('G', 'S', 'U', 'B'), HB_TAG('n', 'o', 'n', 'e')}) {
        auto* actualBlob = hb_face_reference_table(actual, tag);
        auto* expectedBlob = hb_face_reference_table(expected, tag);
        unsigned actualLength = 0, expectedLength = 0;
        const auto* actualData = hb_blob_get_data(actualBlob, &actualLength);
        const auto* expectedData = hb_blob_get_data(expectedBlob, &expectedLength);
        Require(actualLength == expectedLength);
        Require(actualLength == 0 || std::memcmp(actualData, expectedData, actualLength) == 0);
        hb_blob_destroy(actualBlob);
        hb_blob_destroy(expectedBlob);
    }
}

static void CheckFormats(const Api& api, const std::filesystem::path& root) {
    Library library(api);
    for (const auto* file : {"test/subset/data/fonts/Roboto-Regular.ttf", "test/api/fonts/cmunrm.otf", "test/shape/data/in-house/fonts/TTC.ttc", "test/subset/data/fonts/Roboto-Variable.ttf"}) {
        const auto bytes = Read(root / file);
        auto* probe = library.Native(bytes);
        const auto count = probe->num_faces;
        const auto instances = static_cast<unsigned long>(probe->style_flags) >> 16;
        Require(FT_Done_Face(probe) == 0);
        std::vector<FT_Long> indexes;
        for (FT_Long i = 0; i < count; ++i) indexes.push_back(i);
        if (instances != 0) indexes.push_back(1 << 16);
        for (auto index : indexes) {
            auto* face = library.Face(bytes, index);
            auto* native = library.Native(bytes, index);
            auto* actual = api.cached(face);
            auto* expected = hb_ft_face_create_cached(native);
            CompareFace(actual, expected);
            hb_face_destroy(expected);
            api.destroyFace(actual);
            Require(FT_Done_Face(native) == 0 && api.doneFace(face) == 0);
        }
    }
    static const std::string bdf = "STARTFONT 2.1\nFONT -AnyPS5-Test-Medium-R-Normal--8-80-75-75-C-80-ISO10646-1\nSIZE 8 75 75\nFONTBOUNDINGBOX 8 8 0 0\nSTARTPROPERTIES 2\nFONT_ASCENT 8\nFONT_DESCENT 0\nENDPROPERTIES\nCHARS 1\nSTARTCHAR A\nENCODING 65\nSWIDTH 1000 0\nDWIDTH 8 0\nBBX 8 8 0 0\nBITMAP\n18\n24\n42\n7E\n42\n42\n42\n00\nENDCHAR\nENDFONT\n";
    const std::vector<unsigned char> bytes(bdf.begin(), bdf.end());
    auto* face = library.Face(bytes);
    auto* native = library.Native(bytes);
    auto* actual = api.cached(face);
    auto* expected = hb_ft_face_create_cached(native);
    CompareFace(actual, expected);
    Require(api.charIndex(face, 'A') != 0);
    hb_face_destroy(expected);
    api.destroyFace(actual);
    Require(FT_Done_Face(native) == 0 && api.doneFace(face) == 0);
}

static unsigned Shape(const Api& api, hb_font_t* actualFont, hb_font_t* expectedFont, const std::u16string& text, bool ligatures) {
    auto* actual = api.buffer();
    auto* expected = hb_buffer_create();
    const auto* data = reinterpret_cast<const std::uint16_t*>(text.data());
    api.text(actual, data, static_cast<int>(text.size()), 0, -1);
    hb_buffer_add_utf16(expected, data, static_cast<int>(text.size()), 0, -1);
    api.guess(actual);
    hb_buffer_guess_segment_properties(expected);
    const hb_feature_t feature{HB_TAG('l', 'i', 'g', 'a'), 0, 0, std::numeric_limits<unsigned>::max()};
    api.shape(actualFont, actual, ligatures ? nullptr : &feature, ligatures ? 0 : 1);
    hb_shape(expectedFont, expected, ligatures ? nullptr : &feature, ligatures ? 0 : 1);
    unsigned actualCount = 0, expectedCount = 0;
    const auto* infos = api.infos(actual, &actualCount);
    const auto* expectedInfos = hb_buffer_get_glyph_infos(expected, &expectedCount);
    const auto* positions = api.positions(actual, nullptr);
    const auto* expectedPositions = hb_buffer_get_glyph_positions(expected, nullptr);
    Require(actualCount == expectedCount);
    for (unsigned i = 0; i < actualCount; ++i) {
        Require(infos[i].codepoint == expectedInfos[i].codepoint && infos[i].cluster == expectedInfos[i].cluster);
        Require(positions[i].x_advance == expectedPositions[i].x_advance && positions[i].y_advance == expectedPositions[i].y_advance);
        Require(positions[i].x_offset == expectedPositions[i].x_offset && positions[i].y_offset == expectedPositions[i].y_offset);
    }
    api.destroyBuffer(actual);
    hb_buffer_destroy(expected);
    return actualCount;
}

static void CheckShape(const Api& api, const std::filesystem::path& root) {
    Library library(api);
    for (const auto* file : {"test/subset/data/fonts/Roboto-Regular.ttf", "test/shape/data/in-house/fonts/872d2955d326bd6676a06f66b8238ebbaabc212f.ttf"}) {
        const auto bytes = Read(root / file);
        auto* face = library.Face(bytes);
        auto* native = library.Native(bytes);
        auto* actualFace = api.cached(face);
        auto* expectedFace = hb_ft_face_create_cached(native);
        auto* actual = api.font(actualFace);
        auto* expected = hb_font_create(expectedFace);
        api.destroyFace(actualFace);
        hb_face_destroy(expectedFace);
        api.scale(actual, 1000, 1000);
        hb_font_set_scale(expected, 1000, 1000);
        for (const auto& text : {u"office affinity", u"AVATAR", u"a\u0301", u"\u0628\u0622", u"\u0628\u0628\u0628", u"\U0001F600 A"}) {
            Shape(api, actual, expected, text, true);
            Shape(api, actual, expected, text, false);
        }
        if (std::string(file).find("Roboto") != std::string::npos) {
            Require(Shape(api, actual, expected, u"fi", true) == 1);
            Require(Shape(api, actual, expected, u"fi", false) == 2);
        }
        api.destroyFont(actual);
        hb_font_destroy(expected);
        Require(api.doneFace(face) == 0 && FT_Done_Face(native) == 0);
    }
}

int main(int argc, char** argv) {
    Require(argc == 6);
    const bool patched = std::strcmp(argv[3], "patched") == 0;
    Require(patched || std::strcmp(argv[3], "unpatched") == 0);
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
    auto ftModule = LoadLibraryW(std::filesystem::path(argv[2]).c_str());
#else
    auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
    auto ftModule = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!ftModule) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr && ftModule != nullptr);
    Api api{};
#define LoadExport(handle, field, name, nid) api.field = Import<decltype(api.field)>(handle, patched ? nid : #name "_nid_postfix")
    LoadExport(module, cached, hb_ft_face_create_cached, "RTxFQhlbF+U");
    LoadExport(module, destroyFace, hb_face_destroy, "ULNHQXciJ3s");
    LoadExport(module, font, hb_font_create, "FVhYusaWqA4");
    LoadExport(module, destroyFont, hb_font_destroy, "-8t2aEtFws4");
    LoadExport(module, scale, hb_font_set_scale, "eG5EI3x6twg");
    LoadExport(module, shape, hb_shape, "dM3hP4y99rM");
    LoadExport(module, buffer, hb_buffer_create, "7f13XmbjTZo");
    LoadExport(module, destroyBuffer, hb_buffer_destroy, "K-y1p7Z9THQ");
    LoadExport(module, text, hb_buffer_add_utf16, "P+hgu937lFs");
    LoadExport(module, guess, hb_buffer_guess_segment_properties, "DcrJr531A4w");
    LoadExport(module, infos, hb_buffer_get_glyph_infos, "ENsHmJCNG4Q");
    LoadExport(module, positions, hb_buffer_get_glyph_positions, "43mnfTQPCbw");
    LoadExport(ftModule, init, FT_Init_FreeType, "GNsiLzm4SH8");
    LoadExport(ftModule, doneLibrary, FT_Done_FreeType, "dH02CpEEcYk");
    LoadExport(ftModule, memory, FT_New_Memory_Face, "40Wa2neSLM0");
    LoadExport(ftModule, doneFace, FT_Done_Face, "OgFczHko0w8");
    LoadExport(ftModule, referenceFace, FT_Reference_Face, "HkSJXCHCQCU");
    LoadExport(ftModule, size, FT_Set_Pixel_Sizes, "lJxMIwj81SQ");
    LoadExport(ftModule, charIndex, FT_Get_Char_Index, "obnSzeb-KXk");
    LoadExport(ftModule, glyph, FT_Load_Glyph, "M156RuYoUbU");
#undef LoadExport
    const std::filesystem::path root(argv[5]);
    const std::string mode(argv[4]);
    try {
        if (mode == "cache") CheckCache(api, root);
        else if (mode == "generic") CheckGeneric(api, root);
        else if (mode == "formats") CheckFormats(api, root);
        else if (mode == "shape") CheckShape(api, root);
        else Require(false);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: %s\n", mode.c_str(), error.what());
        return 1;
    }
#ifdef _WIN32
    Require(FreeLibrary(module) != 0 && FreeLibrary(ftModule) != 0);
#else
    Require(dlclose(module) == 0 && dlclose(ftModule) == 0);
#endif
}
