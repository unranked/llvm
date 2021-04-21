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
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ADT/APInt.h"

// Optimizations
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"


llvm::Function* createSumFunction(llvm::Module* module) {
    /* Builds the following function:
    # clang sum.c -S -emit-llvm
    int sum(int* a, int n) {
        int result = 0;
        for (int i = 0; i < n; ++i) {
            result += a[i];
        }
        return result;
    }

    compile with:
    # clang++-8 `llvm-config-8 --cxxflags --ldflags --libs` -std=c++17 array.cpp -o exec.out
    */
    llvm::LLVMContext &context = module->getContext();
    llvm::IRBuilder<> builder(context);
    std::vector<llvm::Type*> ArgTypes = {builder.getInt32Ty()->getPointerTo(0), builder.getInt32Ty()};
    auto *funcType = llvm::FunctionType::get(builder.getInt32Ty(), ArgTypes, false);
    auto *fooFunc = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, "sum", module
    );
    std::vector<llvm::Value*> Args;
    for (auto& arg : fooFunc->args()) {
        Args.push_back(&arg);
    }
    auto *l2 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l7 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l11 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l19 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l22 = llvm::BasicBlock::Create(context, "", fooFunc);
    builder.SetInsertPoint(l2);
    llvm::Value *p3 = builder.CreateAlloca(builder.getInt32Ty()->getPointerTo(0), 0, nullptr, "");
    llvm::Value *p4 = builder.CreateAlloca(builder.getInt32Ty(), 0, nullptr, "");
    llvm::Value *p5 = builder.CreateAlloca(builder.getInt32Ty(), 0, nullptr, "");
    llvm::Value *p6 = builder.CreateAlloca(builder.getInt32Ty(), 0, nullptr, "");
    builder.CreateStore(Args[0], p3);
    builder.CreateStore(Args[1], p4);
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt32Ty(), 0), p5);
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt32Ty(), 0), p6);
    builder.CreateBr(l7);
    builder.SetInsertPoint(l7);
    llvm::Value *p8 = builder.CreateLoad(p6);
    llvm::Value *p9 = builder.CreateLoad(p4);
    llvm::Value *p10 = builder.CreateICmpSLT(p8, p9);
    builder.CreateCondBr(p10, l11, l22);
    builder.SetInsertPoint(l11);
    llvm::Value *p12 = builder.CreateLoad(p3);
    llvm::Value *p13 = builder.CreateLoad(p6);
    llvm::Value *p14 = builder.CreateSExt(p13, builder.getInt64Ty());
    llvm::Value *p15 = builder.CreateInBoundsGEP(p12, p14);
    llvm::Value *p16 = builder.CreateLoad(p15);
    llvm::Value *p17 = builder.CreateLoad(p5);
    llvm::Value *p18 = builder.CreateAdd(p17, p16);
    builder.CreateStore(p18, p5);
    builder.CreateBr(l19);
    builder.SetInsertPoint(l19);
    llvm::Value *p20 = builder.CreateLoad(p6);
    llvm::Value *p21 = builder.CreateAdd(p20, llvm::ConstantInt::get(builder.getInt32Ty(), 1));
    builder.CreateStore(p21, p6);
    builder.CreateBr(l7);
    builder.SetInsertPoint(l22);
    llvm::Value *p23 = builder.CreateLoad(p5);
    builder.CreateRet(p23);
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

    auto* func = createSumFunction(module);

    llvm::outs() << "We just constructed this LLVM module:\n\n" << *module;
    llvm::outs() << "\n\nRunning foo: ";
    llvm::outs().flush();

    auto* raw_ptr = executionEngine->getPointerToFunction(func);
    auto* func_ptr = (int(*)(int*, int))raw_ptr;
    executionEngine->finalizeObject();

    // Execute
    int arg1[5] = {1, 2, 3, 4, 5};
    int arg2 = 5;
    int result = func_ptr(arg1, arg2);
    for (int i = 0; i < arg2 - 1; ++i) {
        std::cout << arg1[i] << " + ";
    }
    std::cout << arg1[arg2 - 1] << " = " << result << std::endl;
    return 0;
}