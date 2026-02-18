#pragma once

#include "zym/zym.h"

ZymValue marshal_reconstruct_value(ZymVM* caller_vm, ZymVM* source_vm, ZymVM* target_vm, ZymValue value);
ZymValue marshal_reconstruct_list(ZymVM* caller_vm, ZymVM* source_vm, ZymVM* target_vm, ZymValue source_list);
ZymValue marshal_reconstruct_map(ZymVM* caller_vm, ZymVM* source_vm, ZymVM* target_vm, ZymValue source_map);
ZymValue marshal_reconstruct_buffer(ZymVM* source_vm, ZymVM* target_vm, ZymValue source_buffer);
