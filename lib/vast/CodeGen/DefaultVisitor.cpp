// Copyright (c) 2024-present, Trail of Bits, Inc.

#include "vast/CodeGen/DefaultVisitor.hpp"

#include "vast/CodeGen/Util.hpp"

namespace vast::cg
{
    operation default_visitor::visit_with_attrs(const clang_decl *decl, scope_context &scope) {
        default_decl_visitor visitor(bld, self, scope);
        if (auto op = visitor.visit(decl)) {
            return visit_decl_attrs(op, decl, scope);
        }

        return {};
    }

    using excluded_attr_list = util::type_list<
          clang::WeakAttr
        , clang::SelectAnyAttr
        , clang::CUDAGlobalAttr
    >;

    operation default_visitor::visit_decl_attrs(operation op, const clang_decl *decl, scope_context &scope) {
        if (decl->hasAttrs()) {
            mlir::NamedAttrList attrs = op->getAttrs();
            for (auto attr : exclude_attrs< excluded_attr_list >(decl->getAttrs())) {
                auto visited = self.visit(attr, scope);

                auto spelling = attr->getSpelling();
                // Bultin attr doesn't have spelling because it can not be written in code
                if (auto builtin = clang::dyn_cast< clang::BuiltinAttr >(attr)) {
                    spelling = "builtin";
                }

                if (auto prev = attrs.getNamed(spelling)) {
                    VAST_CHECK(visited == prev.value().getValue(), "Conflicting redefinition of attribute {0}", spelling);
                }

                attrs.set(spelling, visited);
            }
            op->setAttrs(attrs);
        }

        return op;
    }

    operation default_visitor::visit(const clang_decl *decl, scope_context &scope) {
        return visit_with_attrs(decl, scope);
    }

    operation default_visitor::visit(const clang_stmt *stmt, scope_context &scope) {
        default_stmt_visitor visitor(bld, self, scope);
        return visitor.visit(stmt);
    }

    mlir_type default_visitor::visit(const clang_type *type, scope_context &scope) {
        if (auto value = cache.lookup(type)) {
            return value;
        }

        default_type_visitor visitor(bld, self, scope);
        auto result = visitor.visit(type);
        cache.try_emplace(type, result);
        return result;
    }

    mlir_type default_visitor::visit(clang_qual_type type, scope_context &scope) {
        if (auto value = qual_cache.lookup(type)) {
            return value;
        }

        default_type_visitor visitor(bld, self, scope);
        auto result = visitor.visit(type);
        qual_cache.try_emplace(type, result);
        return result;
    }

    mlir_attr default_visitor::visit(const clang_attr *attr, scope_context &scope) {
        default_attr_visitor visitor(bld, self, scope);
        return visitor.visit(attr);
    }

    operation default_visitor::visit_prototype(const clang_function *decl, scope_context &scope) {
        default_decl_visitor visitor(bld, self, scope);
        return visitor.visit_prototype(decl);
    }

} // namespace vast::cg