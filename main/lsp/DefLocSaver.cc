#include "DefLocSaver.h"
#include "ast/Helpers.h"
#include "core/lsp/Query.h"
#include "core/lsp/QueryResponse.h"

using namespace std;
namespace sorbet::realmain::lsp {

unique_ptr<ast::MethodDef> DefLocSaver::postTransformMethodDef(core::Context ctx,
                                                               unique_ptr<ast::MethodDef> methodDef) {
    const core::lsp::Query &lspQuery = ctx.state.lspQuery;
    bool lspQueryMatch = lspQuery.matchesLoc(methodDef->declLoc) || lspQuery.matchesSymbol(methodDef->symbol);

    if (lspQueryMatch) {
        // Query matches against the method definition as a whole.
        auto &symbolData = methodDef->symbol.data(ctx);
        auto &argTypes = symbolData->arguments();
        core::TypeAndOrigins tp;

        // Check if it matches against a specific argument. If it does, send that instead;
        // it's more specific.
        const int numArgs = methodDef->args.size();

        ENFORCE(numArgs == argTypes.size());
        for (int i = 0; i < numArgs; i++) {
            auto &arg = methodDef->args[i];
            auto &argType = argTypes[i];
            auto *localExp = ast::MK::arg2Local(arg.get());
            // localExp should never be null, but guard against the possibility.
            if (localExp && lspQuery.matchesLoc(localExp->loc)) {
                tp.type = argType.type;
                tp.origins.emplace_back(localExp->loc);
                core::lsp::QueryResponse::pushQueryResponse(
                    ctx, core::lsp::IdentResponse(methodDef->symbol, localExp->loc, localExp->localVariable, tp));
                return methodDef;
            }
        }

        tp.type = symbolData->resultType;
        tp.origins.emplace_back(methodDef->declLoc);
        core::lsp::QueryResponse::pushQueryResponse(
            ctx, core::lsp::DefinitionResponse(methodDef->symbol, methodDef->declLoc, methodDef->name, tp));
    }

    return methodDef;
}

unique_ptr<ast::UnresolvedIdent> DefLocSaver::postTransformUnresolvedIdent(core::Context ctx,
                                                                           unique_ptr<ast::UnresolvedIdent> id) {
    if (id->kind == ast::UnresolvedIdent::Instance || id->kind == ast::UnresolvedIdent::Class) {
        core::SymbolRef klass;
        // Logic cargo culted from `global2Local` in `walker_build.cc`.
        if (id->kind == ast::UnresolvedIdent::Instance) {
            ENFORCE(ctx.owner.data(ctx)->isMethod());
            klass = ctx.owner.data(ctx)->owner;
        } else {
            // Class var.
            klass = ctx.owner.data(ctx)->enclosingClass(ctx);
            while (klass.data(ctx)->attachedClass(ctx).exists()) {
                klass = klass.data(ctx)->attachedClass(ctx);
            }
        }

        auto sym = klass.data(ctx)->findMemberTransitive(ctx, id->name);
        const core::lsp::Query &lspQuery = ctx.state.lspQuery;
        if (sym.exists() && (lspQuery.matchesSymbol(sym) || lspQuery.matchesLoc(id->loc))) {
            core::TypeAndOrigins tp;
            tp.type = sym.data(ctx.state)->resultType;
            tp.origins.emplace_back(sym.data(ctx.state)->loc());
            core::lsp::QueryResponse::pushQueryResponse(
                ctx, core::lsp::ConstantResponse(klass, sym, id->loc, id->name, tp, tp));
        }
    }
    return id;
}

} // namespace sorbet::realmain::lsp
