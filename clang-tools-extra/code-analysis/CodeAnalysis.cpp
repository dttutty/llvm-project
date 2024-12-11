#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/CompilerInstance.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// 定义命令行选项类别
static cl::OptionCategory MyToolCategory("I can extract function information: name, number of arguments, number of statements, number of loops, number of calls");

// 自定义 Visitor 类，用于遍历 AST
class FunctionInfoVisitor : public RecursiveASTVisitor<FunctionInfoVisitor> {
public:
    explicit FunctionInfoVisitor(ASTContext* Context) : Context(Context) {}

    // 处理函数声明
    bool VisitFunctionDecl(FunctionDecl* Func) {
        if (Func->isThisDeclarationADefinition()) {
            // 函数名称
            llvm::outs() << "Function Name: " << Func->getNameAsString() << "\n";

            // 参数个数
            llvm::outs() << "Number of Arguments: " << Func->getNumParams() << "\n";

            // 函数体信息
            if (Stmt* Body = Func->getBody()) {
                FunctionBodyInfo(Body);
            }
            llvm::outs() << "\n";
        }
        return true; // 继续遍历
    }

private:
    ASTContext* Context;

    // 分析函数体内的语句、循环和函数调用
    void FunctionBodyInfo(Stmt* Body) {
        unsigned NumStatements = 0;
        unsigned NumLoops = 0;
        unsigned NumCalls = 0;

        // 遍历函数体中的所有语句
        for (const Stmt* SubStmt : Body->children()) {
            if (!SubStmt)
                continue;

            // 统计语句数
            NumStatements++;

            // 统计循环
            if (isa<ForStmt>(SubStmt) || isa<WhileStmt>(SubStmt) || isa<DoStmt>(SubStmt)) {
                NumLoops++;
            }

            // 统计函数调用
            if (const CallExpr* Call = dyn_cast<CallExpr>(SubStmt)) {
                if (const FunctionDecl* Callee = Call->getDirectCallee()) {
                    llvm::outs() << "  Calls function: " << Callee->getNameAsString() << "\n";
                    NumCalls++;
                }
            }
        }

        llvm::outs() << "Number of Statements: " << NumStatements << "\n";
        llvm::outs() << "Number of Loops: " << NumLoops << "\n";
        llvm::outs() << "Number of Calls: " << NumCalls << "\n";
    }
};

// 自定义 Consumer 类，用于处理 AST
class FunctionInfoConsumer : public ASTConsumer {
public:
    explicit FunctionInfoConsumer(ASTContext* Context) : Visitor(Context) {}

    void HandleTranslationUnit(ASTContext& Context) override {
        // 开始遍历 AST
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    FunctionInfoVisitor Visitor;
};

// 自定义 FrontendAction 类
class FunctionInfoAction : public ASTFrontendAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& Compiler,
        llvm::StringRef InFile) override {
        return std::make_unique<FunctionInfoConsumer>(&Compiler.getASTContext());
    }
};

// 主函数
int main(int argc, const char** argv) {
    // 解析命令行参数
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
    if (!ExpectedParser) {
        llvm::errs() << "Error:" << ExpectedParser.takeError() << "\n";
        return 1;
    }
    CommonOptionsParser& op = *ExpectedParser;

    // 创建 ClangTool 实例
    ClangTool Tool(op.getCompilations(), op.getSourcePathList());

    // 运行工具
    int result = Tool.run(newFrontendActionFactory<FunctionInfoAction>().get());
    return result;
}
