// Copyright (c) 2021-present, Trail of Bits, Inc.

#include "vast/Dialect/HighLevel/Passes.hpp"

VAST_RELAX_WARNINGS
#include <mlir/Dialect/ControlFlow/IR/ControlFlowOps.h>
#include <mlir/Dialect/Func/IR/FuncOps.h>

#include <mlir/Rewrite/FrozenRewritePatternSet.h>
#include <mlir/Transforms/DialectConversion.h>
#include <mlir/Transforms/GreedyPatternRewriteDriver.h>
VAST_UNRELAX_WARNINGS

#include <gap/core/memoize.hpp>
#include <gap/core/generator.hpp>

#include <vast/Interfaces/AggregateTypeDefinitionInterface.hpp>

#include <vast/Conversion/Common/Passes.hpp>
#include <vast/Util/TypeUtils.hpp>

#include "PassesDetails.hpp"

namespace vast::hl {

#if !defined(NDEBUG)
    constexpr bool debug_ude_pass = false;
    #define VAST_UDE_DEBUG(...) VAST_REPORT_WITH_PREFIX_IF(debug_ude_pass, "[UDE] ", __VA_ARGS__)
#else
    #define VAST_UDE_DEBUG(...)
#endif

    template< typename ... args_t >
    bool is_one_of( operation op ) { return ( mlir::isa< args_t >( op ) || ... ); }

    constexpr auto keep_only_if_used = [] (auto, auto) { /* do not keep by default */ return false; };

    struct UDE : UDEBase< UDE >
    {
        using base         = UDEBase< UDE >;
        using operations_t = std::vector< operation >;

        operations_t unused;

        using walk_result = mlir::WalkResult;

        static inline auto keep_operation = walk_result::interrupt();
        static inline auto drop_operation = walk_result::advance();

        using aggregate = AggregateTypeDefinitionInterface;

        //
        // aggregate unused definition elimination
        //
        bool keep(aggregate op, auto scope) const { return keep_only_if_used(op, scope); }

        walk_result users(aggregate agg, auto scope, auto &&yield) const {
            auto accept = [&] (mlir_type ty) {
                if (auto rec = mlir::dyn_cast< RecordType >(ty))
                    return rec.getName() == agg.getDefinedName();
                return false;
            };

            return scope.walk([&](operation op) {
                if (mlir::isa< vast_module >(op)) {
                    // skip module as it always contains value use
                    return drop_operation;
                }

                if (has_type_somewhere(op, accept)) {
                    return yield(op);
                }

                return drop_operation;
            });
        }

        // keep field if its parent is kept
        bool keep(hl::FieldDeclOp op, auto scope) const {
            return keep(op.getParentAggregate(), scope);
        }

        walk_result users(hl::FieldDeclOp decl, auto scope, auto &&yield) const {
            return users(decl.getParentAggregate(), scope, std::forward< decltype(yield) >(yield));
        }

        void process(operation op, vast_module mod) {
            // we keep the operation if it is resolved to be kept or any of its
            // users is marked as to be kept, otherwise we mark it as unused and
            // erase it
            auto keep = [this, mod](auto &self, operation op) {
                auto dispatch = [this, mod, &self] (auto op) {
                    VAST_UDE_DEBUG("processing: {0}", *op);
                    if (this->keep(op, mod))
                        return true;

                    auto result = users(op, mod, [&](auto user) -> walk_result {
                        auto keep_user = self(user);
                        VAST_UDE_DEBUG("user: {0} : {1}", *user, keep_user ? "keep" : "drop");
                        // if any user is to be kept, keep the operation
                        return keep_user ? keep_operation : drop_operation;
                    });

                    return result == keep_operation;
                };

                return llvm::TypeSwitch< operation, bool >(op)
                    .Case([&](aggregate op)       { return dispatch(op); })
                    .Case([&](hl::FieldDeclOp op) { return dispatch(op); })
                    .Default([&](operation) { return true; });
            };

            auto to_keep = gap::recursive_memoize<bool(operation)>(keep);

            if (!to_keep(op)) {
                VAST_UDE_DEBUG("unused: {0}", *op);
                unused.push_back(op);
            }
        }

        void process(vast_module mod) {
            for (auto &op : mod.getOps()) {
                process(&op, mod);
            }
        }

        void runOnOperation() override {
            process(getOperation());
            for (auto &op : unused) {
                op->erase();
            }
        }
    };

    std::unique_ptr< mlir::Pass > createUDEPass() { return std::make_unique< UDE >(); }
} // namespace vast::hl