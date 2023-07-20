#pragma once
#include "types.h"
#include "note.h"
#include "str_util.h"
#include <vector>

namespace DAW::PythonNoteProcessor {
    struct python_script_ctxt_t {
        std::vector<note_t> notes;
        int32_t seed = 42;
        std::vector<float> params;
    };

    struct python_func_param_t {
        enum param_type {
            param_type_int,
            param_type_float
        };
        String name;
        param_type type = param_type_int;
        float rangeMin  = 0;
        float rangeMax  = 0;
        float defValue  = 0;
    };

    struct python_note_processor_t {
        String descriptiveName;
        String processorName;
        std::vector<python_func_param_t> params;
    };

    void Init();
    std::vector<note_t> RunPythonNoteProcessor(const String& processorName, python_script_ctxt_t& ctxt);
    std::vector<python_note_processor_t> GetNoteProcessors();
} // namespace DAW::PythonNoteProcessor