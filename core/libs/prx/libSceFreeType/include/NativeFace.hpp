#pragma once

namespace FreeType {

struct GuestFaceRec;

}

extern "C" {

void* FreeTypeBorrowFace_nid_no_patch(const FreeType::GuestFaceRec* face);

}
