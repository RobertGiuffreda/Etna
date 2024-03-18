#pragma once
#include "importer_types.h"

import_payload import_files(u32 file_count, const char* const* paths);

void import_payload_destroy(import_payload* payload);