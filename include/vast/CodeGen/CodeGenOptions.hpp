// Copyright (c) 2024, Trail of Bits, Inc.

#pragma once


#include "vast/Dialect/Core/CoreAttributes.hpp"

namespace vast::cg {

    using source_language = core::SourceLanguage;

    struct options_t
    {
        source_language lang;
        unsigned int optimization_level : 2;

        // function emition optionsi
        unsigned int has_strict_return : 1;
    };

} // namespace vast::cg