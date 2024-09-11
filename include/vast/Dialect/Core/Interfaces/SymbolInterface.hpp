// Copyright (c) 2022-present, Trail of Bits, Inc.

#pragma once

#include "vast/Util/Warnings.hpp"

VAST_RELAX_WARNINGS
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/IR/Dialect.h>
#include <mlir/IR/OperationSupport.h>
#include <llvm/ADT/StringRef.h>
VAST_RELAX_WARNINGS

#include "vast/Util/TypeList.hpp"

namespace vast::core {

    // Use TypeID of Interfaces that defines the symbol
    using symbol_kind = mlir::TypeID;

} // namespace vast::core

/// Include the generated interface declarations.
#include "vast/Dialect/Core/Interfaces/SymbolInterface.h.inc"

namespace vast::core {

    using symbol = SymbolOpInterface;
    using func_symbol = FuncSymbolOpInterface;

    template< typename interface >
    concept symbol_op_interface = requires (interface i) {
        static_cast< symbol >(i);
    };

    template< symbol_op_interface interface >
    static symbol_kind get_symbol_kind = mlir::TypeID::get< interface >();

    template< symbol_op_interface interface >
    bool is_symbol_kind(symbol_kind kind) {
        return kind == get_symbol_kind< interface >;
    }

} // namespace vast::core