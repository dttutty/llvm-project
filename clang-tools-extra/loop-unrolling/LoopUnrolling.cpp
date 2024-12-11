#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/CompilerInstance.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// 定义命令行选项
static cl::OptionCategory MyToolCategory("I can unroll loops by given factor");
static cl::opt<unsigned> UnrollFactor("unroll-factor", cl::desc("Specify unroll factor"), cl::value_desc("factor"), cl::init(2));
static cl::opt<std::string> OutputFilePath("output", cl::desc("Specify output file path"), cl::value_desc("filename"), cl::init(""));

class LoopUnrollingVisitor : public RecursiveASTVisitor<LoopUnrollingVisitor> {
public:
    explicit LoopUnrollingVisitor(ASTContext* Context, Rewriter& R) : Context(Context), TheRewriter(R) {}

    bool VisitForStmt(ForStmt* For) {
        if (!isValidForLoop(For)) {
            return true; // 继续遍历
        }

        llvm::outs() << "Found a loop to unroll at line " << Context->getSourceManager().getSpellingLineNumber(For->getForLoc()) << "\n";

        std::string UnrolledCode = generateUnrolledCode(For);
        SourceRange Range = For->getSourceRange();
        TheRewriter.ReplaceText(Range, UnrolledCode);

        return true;
    }

private:
    ASTContext* Context;
    Rewriter& TheRewriter;

    bool isValidForLoop(ForStmt* For) {
        const Expr* Cond = For->getCond();
        const Expr* Inc = For->getInc();
        if (!Cond || !Inc) {
            return false;
        }

        if (const BinaryOperator* BinOp = dyn_cast<BinaryOperator>(Cond)) {
            if (BinOp->getOpcode() == BO_LT) {
                if (const IntegerLiteral* End = dyn_cast<IntegerLiteral>(BinOp->getRHS())) {
                    unsigned IterCount = End->getValue().getLimitedValue();
                    return IterCount % UnrollFactor == 0;
                }
            }
        }

        return false;
    }

    std::string generateUnrolledCode(ForStmt* For) {
        std::string UnrolledCode;
        llvm::raw_string_ostream Stream(UnrolledCode);

        const VarDecl* LoopVar = nullptr;
        if (DeclStmt* Init = dyn_cast<DeclStmt>(For->getInit())) {
            if (VarDecl* VD = dyn_cast<VarDecl>(Init->getSingleDecl())) {
                LoopVar = VD;
            }
        }

        if (!LoopVar) {
            llvm::errs() << "Error: Unable to extract loop variable.\n";
            return "";
        }

        std::string LoopVarName = LoopVar->getNameAsString();
        const BinaryOperator* Cond = dyn_cast<BinaryOperator>(For->getCond());
        const IntegerLiteral* End = dyn_cast<IntegerLiteral>(Cond->getRHS());
        unsigned IterCount = End->getValue().getLimitedValue();
        const Stmt* Body = For->getBody();

        for (unsigned i = 0; i < IterCount; i += UnrollFactor) {
            for (unsigned j = 0; j < UnrollFactor; ++j) {
                Stream << LoopVarName << " = " << (i + j) << ";\n";
                Stream << TheRewriter.getRewrittenText(Body->getSourceRange()) << "\n";
            }
        }

        return Stream.str();
    }
};

class LoopUnrollingConsumer : public ASTConsumer {
public:
    explicit LoopUnrollingConsumer(ASTContext* Context, Rewriter& R) : Visitor(Context, R) {}

    void HandleTranslationUnit(ASTContext& Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    LoopUnrollingVisitor Visitor;
};

class LoopUnrollingAction : public ASTFrontendAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef InFile) override {
        RewriterForTool.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<LoopUnrollingConsumer>(&CI.getASTContext(), RewriterForTool);
    }

    void EndSourceFileAction() override {
        std::error_code EC;
        llvm::raw_fd_ostream OutFile(OutputFilePath.empty() ? "-" : OutputFilePath, EC, llvm::sys::fs::OF_None);

        if (EC) {
            llvm::errs() << "Error writing to file: " << EC.message() << "\n";
            return;
        }

        RewriterForTool.getEditBuffer(RewriterForTool.getSourceMgr().getMainFileID()).write(OutFile);
        if (OutputFilePath.empty()) {
            llvm::outs() << "Transformed Code:\n";
        }
    }

private:
    Rewriter RewriterForTool;
};

int main(int argc, const char** argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
    if (!ExpectedParser) {
        llvm::errs() << "Error:" << ExpectedParser.takeError() << "\n";
        return 1;
    }
    CommonOptionsParser& op = *ExpectedParser;

    ClangTool Tool(op.getCompilations(), op.getSourcePathList());

    return Tool.run(newFrontendActionFactory<LoopUnrollingAction>().get());
}
