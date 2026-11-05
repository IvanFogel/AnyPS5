#pragma once

#include "prx/libharfbuzz/include/FontFunctions.hpp"

struct hb_blob_t;
struct hb_buffer_t;
struct hb_feature_t;

namespace HarfBuzz {

using ReferenceTable = hb_blob_t* (APS5_VABI *)(hb_face_t*, std::uint32_t, void*);

}

extern "C" {

hb_blob_t* APS5_VABI hb_blob_create_nid_postfix(const char* data, unsigned length, std::uint32_t mode, void* userData, HarfBuzz::Destroy destroy);
hb_face_t* APS5_VABI hb_face_create_for_tables_nid_postfix(HarfBuzz::ReferenceTable referenceTable, void* userData, HarfBuzz::Destroy destroy);
void APS5_VABI hb_face_destroy_nid_postfix(hb_face_t* face);
unsigned APS5_VABI hb_ot_layout_table_get_script_tags_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned startOffset, unsigned* count, std::uint32_t* tags);
unsigned APS5_VABI hb_ot_layout_script_get_language_tags_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned scriptIndex, unsigned startOffset, unsigned* count, std::uint32_t* tags);
int APS5_VABI hb_ot_layout_language_find_feature_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned scriptIndex, unsigned languageIndex, std::uint32_t featureTag, unsigned* featureIndex);
void APS5_VABI hb_shape_nid_postfix(hb_font_t* font, hb_buffer_t* buffer, const hb_feature_t* features, unsigned featureCount);

}
