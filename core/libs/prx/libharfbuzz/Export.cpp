#include "prx/libharfbuzz/include/InventoryExports.hpp"
#include "prx/libharfbuzz/include/Shaping.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

hb_blob_t* APS5_VABI hb_blob_create_nid_postfix(const char* data, unsigned length, std::uint32_t mode, void* userData, HarfBuzz::Destroy destroy) {
    NotImplemented_nid_no_patch(__func__);
}

hb_face_t* APS5_VABI hb_face_create_for_tables_nid_postfix(HarfBuzz::ReferenceTable referenceTable, void* userData, HarfBuzz::Destroy destroy) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI hb_face_destroy_nid_postfix(hb_face_t* face) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI hb_ft_face_create_cached_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI hb_icu_script_to_script_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI hb_ot_layout_language_find_feature_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned scriptIndex, unsigned languageIndex, std::uint32_t featureTag, unsigned* featureIndex) {
    NotImplemented_nid_no_patch(__func__);
}

unsigned APS5_VABI hb_ot_layout_script_get_language_tags_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned scriptIndex, unsigned startOffset, unsigned* count, std::uint32_t* tags) {
    NotImplemented_nid_no_patch(__func__);
}

unsigned APS5_VABI hb_ot_layout_table_get_script_tags_nid_postfix(hb_face_t* face, std::uint32_t tableTag, unsigned startOffset, unsigned* count, std::uint32_t* tags) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI hb_ot_tag_to_script_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI hb_shape_nid_postfix(hb_font_t* font, hb_buffer_t* buffer, const hb_feature_t* features, unsigned featureCount) {
    NotImplemented_nid_no_patch(__func__);
}

}
