#include <iostream>
#include <memory>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/Support/TargetSelect.h"

// Optimizations
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"


llvm::Function* createMulFunction(llvm::Module* module) {
    /* Builds the following function:
    
    int mul(int a. int b) {
        int result = 0;
        for (int i = 0; i < b; ++i) {
            result += a;
        }
        return result;
    }
    */
    llvm::LLVMContext &context = module->getContext();
    llvm::IRBuilder<> builder(context);
    std::vector<std::string> ArgNames = {"a", "b"};
    std::vector<llvm::Type*> Integers(ArgNames.size(), builder.getInt32Ty());
    auto *funcType = llvm::FunctionType::get(builder.getInt32Ty(), Integers, false);
    auto *fooFunc = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, "mul", module
    );
    std::vector<llvm::Value*> Args;
    for (auto& arg : fooFunc->args()) {
        arg.setName(ArgNames[arg.getArgNo()]);
        Args.push_back(&arg);
    }
    auto *entry = llvm::BasicBlock::Create(context, "entry", fooFunc);
    auto *loop = llvm::BasicBlock::Create(context, "loop", fooFunc);
    auto *afterloop = llvm::BasicBlock::Create(context, "afterloop", fooFunc);
    builder.SetInsertPoint(entry);
    llvm::Value *ResultVal = builder.CreateAlloca(builder.getInt32Ty(), 0, builder.getInt32(1), "result");
    builder.CreateStore(builder.getInt32(0), ResultVal);
    llvm::Instruction* br = builder.CreateBr(loop);
    builder.SetInsertPoint(loop);
    llvm::Value *StartVal = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
    llvm::Value *StepVal = llvm::ConstantInt::get(builder.getInt32Ty(), 1);
    llvm::PHINode *Variable = builder.CreatePHI(llvm::Type::getInt32Ty(context), 0, "i");
    Variable->addIncoming(StartVal, entry);
    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(ResultVal), Args[0]), ResultVal);
    llvm::Value *NextVar = builder.CreateAdd(StepVal, Variable, "nextvar");
    llvm::Value *cond = builder.CreateICmpULT(Variable, Args[1], "cmptmp");
    builder.CreateCondBr(cond, loop, afterloop);
    builder.SetInsertPoint(afterloop);
    Variable->addIncoming(NextVar, loop);
    builder.CreateRet(builder.CreateLoad(ResultVal));
    llvm::verifyFunction(*fooFunc);
    return fooFunc;
};

int main(int argc, char* argv[]) {
    llvm::TargetOptions Opts;
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::LLVMContext context;
    auto myModule = std::make_unique<llvm::Module>("My First JIT", context);
    auto* module = myModule.get();

    std::unique_ptr<llvm::RTDyldMemoryManager> MemMgr(new llvm::SectionMemoryManager());

    llvm::EngineBuilder factory(std::move(myModule));
    factory.setEngineKind(llvm::EngineKind::JIT);
    factory.setTargetOptions(Opts);
    factory.setMCJITMemoryManager(std::move(MemMgr));
    auto executionEngine = std::unique_ptr<llvm::ExecutionEngine>(factory.create());
    module->setDataLayout(executionEngine->getDataLayout());

    // Create optimizations, not necessary, whole block can be ommited.
    // auto fpm = llvm::make_unique<legacy::FunctionPassManager>(module);
    // fpm->add(llvm::createBasicAAWrapperPass());
    // fpm->add(llvm::createPromoteMemoryToRegisterPass());
    // fpm->add(llvm::createInstructionCombiningPass());
    // fpm->add(llvm::createReassociatePass());
    // fpm->add(llvm::createNewGVNPass());
    // fpm->add(llvm::createCFGSimplificationPass());
    // fpm->doInitialization();

    auto* func = createMulFunction(module);
    executionEngine->finalizeObject();

    auto* raw_ptr = executionEngine->getPointerToFunction(func);
    auto* func_ptr = (int(*)(int, int))raw_ptr;
    llvm::outs() << "We just constructed this LLVM module:\n\n" << *module;
    llvm::outs() << "\n\nRunning foo: ";
    llvm::outs().flush();

    // Execute
    int arg1 = 123;
    int arg2 = 100;
    int result = func_ptr(arg1, arg2);
    std::cout << arg1 << " * " << arg2 << " = " << result << std::endl;

    return 0;
}