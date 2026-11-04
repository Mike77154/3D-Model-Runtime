#include "re4c3d/re4_model.h"

const char *re4_model_kind_name(re4_model_kind kind)
{
    switch (kind) {
    case RE4_MODEL_PMD: return "PMD";
    case RE4_MODEL_BIN: return "BIN";
    case RE4_MODEL_SMD_ROOM: return "SMD_ROOM";
    case RE4_MODEL_SMX: return "SMX";
    default: return "UNKNOWN";
    }
}
