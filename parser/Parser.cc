#include "parser.h"
#include "Builder.h"
#include "core/Loc.h"
#include "core/errors/parser.h"
#include "ruby_parser/driver.hh"
#include <algorithm>

template class std::unique_ptr<sorbet::parser::Node>;

using namespace std;

namespace sorbet {
namespace parser {

extern const char *dclass_strings[];

using namespace std;

class DiagnosticToError {
    static u4 translatePos(size_t pos, u4 max_off) {
        if (pos == 0) {
            return pos;
        }
        return min((u4)(pos), max_off);
    }

public:
    static void run(core::GlobalState &gs, core::FileRef file, ruby_parser::diagnostics_t diagnostics) {
        if (diagnostics.empty()) {
            return;
        }
        u4 max_off = file.data(gs).source().size();

        for (auto &diag : diagnostics) {
            string level = "unknown";
            switch (diag.level()) {
                case ruby_parser::dlevel::NOTE:
                case ruby_parser::dlevel::WARNING:
                    continue;
                case ruby_parser::dlevel::ERROR:
                    level = "Error";
                    break;
                case ruby_parser::dlevel::FATAL:
                    level = "Fatal";
                    break;
                default:
                    Error::notImplemented();
            }
            string msg("Parse {}: ");
            msg.append(dclass_strings[(int)diag.error_class()]);
            Loc loc(file, translatePos(diag.location().beginPos, max_off),
                    translatePos(diag.location().endPos, max_off));
            if (auto e = gs.beginError(loc, core::errors::Parser::ParserError)) {
                e.setHeader(msg, level, diag.data());
            }
        }
    }
};

unique_ptr<Node> Parser::run(sorbet::core::GlobalState &gs, core::FileRef file) {
    Builder builder(gs, file);
    auto source = file.data(gs).source();
    ruby_parser::typedruby24 driver(string(source.begin(), source.end()), Builder::interface);
    auto ast = unique_ptr<Node>(builder.build(&driver));
    DiagnosticToError::run(gs, file, driver.diagnostics);

    if (!ast) {
        core::Loc loc(file, 0, 0);
        NodeVec empty;
        return make_unique<Begin>(loc, move(empty));
    }

    return ast;
}

unique_ptr<Node> Parser::run(sorbet::core::GlobalState &gs, const string &path, const string &src) {
    core::FileRef file = gs.enterFile(path, src);
    return run(gs, file);
}

}; // namespace parser
}; // namespace sorbet
