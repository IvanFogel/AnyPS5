#include "prx/libharfbuzz/include/Shaping.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <hb.h>
#include <hb-ot.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <type_traits>

static_assert(std::is_same_v<hb_bool_t, int>);
static_assert(std::is_same_v<hb_tag_t, std::uint32_t>);
static_assert(sizeof(hb_memory_mode_t) == 4);
static_assert(sizeof(hb_feature_t) == 16 && alignof(hb_feature_t) == 4);
static_assert(offsetof(hb_feature_t, tag) == 0);
static_assert(offsetof(hb_feature_t, value) == 4);
static_assert(offsetof(hb_feature_t, start) == 8);
static_assert(offsetof(hb_feature_t, end) == 12);

namespace {

struct BlobData {
    void* data;
    HarfBuzz::Destroy destroy;
};

struct FaceData {
    HarfBuzz::ReferenceTable referenceTable;
    void* data;
    HarfBuzz::Destroy destroy;
};

template <typename TData>
void DestroyData(void* opaque) {
    const std::unique_ptr<TData> owner(static_cast<TData*>(opaque));
    if (owner->destroy) owner->destroy(owner->data);
}

hb_blob_t* ReferenceTable(hb_face_t* face, hb_tag_t tag, void* opaque) {
    const auto& owner = *static_cast<FaceData*>(opaque);
    return owner.referenceTable(face, tag, owner.data);
}

template <typename TQuery>
unsigned ListTags(unsigned startOffset, unsigned* count, std::uint32_t* tags, TQuery query) {
    GuestArena::HostWrite countWrite(count, count ? sizeof(*count) : 0);
    unsigned writable = 0;
    if (count && tags && *count) {
        const auto total = query(nullptr, nullptr);
        if (startOffset < total) writable = std::min(*count, total - startOffset);
    }
    GuestArena::HostWrite tagsWrite(tags, static_cast<std::size_t>(writable) * sizeof(*tags));
    return query(count, tags);
}

}

extern "C" {

hb_blob_t* APS5_VABI hb_blob_create_nid_postfix(const char* data, unsigned length, std::uint32_t mode, void* userData, HarfBuzz::Destroy destroy) {
    auto owner = std::make_unique<BlobData>(userData, destroy);
    return hb_blob_create(data, length, static_cast<hb_memory_mode_t>(mode), owner.release(), DestroyData<BlobData>);
}

hb_face_t* APS5_VABI hb_face_create_for_tables_nid_postfix(HarfBuzz::ReferenceTable referenceTable, void* userData, HarfBuzz::Destroy destroy) {
    auto owner = std::make_unique<FaceData>(referenceTable, userData, destroy);
    return hb_face_create_for_tables(referenceTable ? ReferenceTable : nullptr, owner.release(), DestroyData<FaceData>);
}

void APS5_VABI hb_face_destroy_nid_postfix(hb_face_t* face) {
    hb_face_destroy(face);
}

unsigned APS5_VABI hb_ot_layout_table_get_script_tags_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned startOffset, unsigned* count, std::uint32_t* tags) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    return ListTags(startOffset, count, tags, [&](unsigned* length, std::uint32_t* output) {
        return hb_ot_layout_table_get_script_tags(face, tableTag, startOffset, length, output);
    });
}

unsigned APS5_VABI hb_ot_layout_script_get_language_tags_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned scriptIndex, unsigned startOffset, unsigned* count, std::uint32_t* tags) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    return ListTags(startOffset, count, tags, [&](unsigned* length, std::uint32_t* output) {
        return hb_ot_layout_script_get_language_tags(face, tableTag, scriptIndex, startOffset, length, output);
    });
}

int APS5_VABI hb_ot_layout_language_find_feature_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned scriptIndex, unsigned languageIndex, std::uint32_t featureTag, unsigned* featureIndex) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite indexWrite(featureIndex, featureIndex ? sizeof(*featureIndex) : 0);
    return hb_ot_layout_language_find_feature(face, tableTag, scriptIndex, languageIndex, featureTag, featureIndex);
}

void APS5_VABI hb_shape_nid_postfix(hb_font_t* font, hb_buffer_t* buffer, const hb_feature_t* features, unsigned featureCount) {
    if (!font || !buffer) NotImplemented_nid_no_patch(__func__);
    hb_shape(font, buffer, features, featureCount);
}

}
