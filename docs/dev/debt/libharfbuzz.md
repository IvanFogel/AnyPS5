# libharfbuzz technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libharfbuzz/Export.cpp) - The remaining 3 throwing callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libharfbuzz.c) and the [selected export metadata](https://gist.github.com/kyon-eth/510aa3fcaf09cbe8b52048b44e4e67c0/b909f17c99ecd6323c085f70064548789062d57e) from an older `libharfbuzz` image and a later `libSceVshHarfBuzz` image (SHA-256 `25af97e02c1fa8d318ff3697eb8a79057ee9b8c5e5e4ec321f25114aceb05628`). The later image has a different module and library identity; matching NIDs do not establish forwarding or equivalent behavior. The version labels are not authenticated. Their prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `hb_ft_face_create_cached` | `RTxFQhlbF+U` |
| `hb_icu_script_to_script` | `u0E8Xi0easM` |
| `hb_ot_tag_to_script` | `uKjh3hI1npw` |

### Functional

- [The eleven buffer exports](../../../core/libs/prx/libharfbuzz/Buffer.cpp) have no console or title verification. Their signatures, 20-byte glyph records, Unicode properties, UTF-16 decoding and buffer behavior follow the pinned [HarfBuzz 10.0.1](https://github.com/harfbuzz/harfbuzz/tree/a1d9bfe62818ef0fa9cf63b6e6d51436b1c93cbc) public API and backend; the console revision and its Unicode data are unknown. Buffers are opaque host allocations, not console object layouts. Returned arrays belong to that backend and remain valid only until the buffer contents change. Segment-property guessing uses the host process's default language, not a console language setting. Handles from another HarfBuzz instance cannot be mixed with these buffers. Null buffer handles throw except for destruction, which accepts null as the public API does; console null-handle behavior is unverified.

- [Blob, face, layout and shaping adapters](../../../core/libs/prx/libharfbuzz/Shaping.cpp) have no console or title verification. Their signatures, 16-byte feature records, memory modes, reference counting, font-table callbacks, OpenType queries and shaping follow the pinned HarfBuzz 10.0.1 public API and source; the console revision, Unicode data and allocation-failure behavior remain unknown. Blob, face, font and buffer handles must belong to this backend. Bridge allocation failure throws before transferring ownership of guest data. Callbacks use the upstream C callback contract; throwing through backend callbacks is unverified. Null face layout queries and null font/buffer shaping throw because console behavior is unverified. OpenType output writes are bracketed with guest host-write guards. No game compatibility or performance improvement is established.

### Font callbacks

- [Font functions](../../../core/libs/prx/libharfbuzz/FontFunctions.cpp): Console ABI, HarfBuzz version and behavior have not been measured. Fourteen font lifecycle, callback-table, immutability, scale and ppem APIs follow source-built [HarfBuzz 10.0.1](https://github.com/harfbuzz/harfbuzz/blob/a1d9bfe62818ef0fa9cf63b6e6d51436b1c93cbc/src/hb-font.h). They use backend-owned opaque handles and bridge native callbacks and destruction to guest System V callbacks. Callback data and font data are distinct; replacement and final release follow the backend's reference counting. Foreign object layouts and forwarding from `libSceVshHarfBuzz` are not established. Bridge allocation failure throws `std::bad_alloc`; no console allocation-failure equivalence is claimed.
- [Font callbacks](../../../core/libs/prx/libharfbuzz/FontFunctions.cpp) are exercised by runtime tests using actual exports before and after NID patching and through native shaping with guest callbacks. They establish adapter behavior only. Null destruction and null-face construction follow the backend; other null font/function-table operations throw because no console contract is established. No title compatibility is claimed.

- [Legacy glyph setter](../../../core/libs/prx/libharfbuzz/FontFunctions.cpp): A null callback throws before changing the table or taking ownership of caller data. The pinned deprecated backend setter installs trampolines that later invoke that null pointer; this host-safety rejection has not been measured on a console. Modern nominal/variation null setters still reset their callbacks through the backend.
