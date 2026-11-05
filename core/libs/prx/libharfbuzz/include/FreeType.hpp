#pragma once

#include "SceTypes.hpp"

struct hb_face_t;

namespace FreeType {

struct GuestFaceRec;

}

extern "C" {

hb_face_t* APS5_VABI hb_ft_face_create_cached_nid_postfix(FreeType::GuestFaceRec* face);

}
