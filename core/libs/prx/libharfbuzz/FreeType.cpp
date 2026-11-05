#include "prx/libharfbuzz/include/FreeType.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeType/include/NativeFace.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <hb.h>
#include <hb-ft.h>

namespace {

static void APS5_VABI FinalizeCachedFace_nid_no_patch(void* opaque) {
    const auto* face = static_cast<FreeType::GuestFaceRec*>(opaque);
    hb_face_destroy(static_cast<hb_face_t*>(face->generic.data));
}

}

extern "C" {

hb_face_t* APS5_VABI hb_ft_face_create_cached_nid_postfix(FreeType::GuestFaceRec* face) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    const auto nativeFace = static_cast<FT_Face>(FreeTypeBorrowFace_nid_no_patch(face));
    if (!nativeFace) NotImplemented_nid_no_patch(__func__);
    auto& cache = face->generic;
    if (!cache.data || cache.finalizer != FinalizeCachedFace_nid_no_patch) {
        if (cache.finalizer) cache.finalizer(face);
        auto* native = hb_ft_face_create(nativeFace, nullptr);
        GuestArena::HostWrite write(&cache, sizeof(cache));
        cache = {native, FinalizeCachedFace_nid_no_patch};
    }
    return hb_face_reference(static_cast<hb_face_t*>(cache.data));
}

}
