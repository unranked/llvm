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

#define VECSIZE 50
#define VECTYPE int // double

typedef struct {
    VECTYPE* values;
    char* null;
} Vector;


llvm::Function* createAddvFunction(llvm::Module* module) {
    /* Builds the following function:
    # clang task.c -S -emit-llvm
    void addv(Vector *vec1, Vector *vec2, Vector *result) {
        for (int i = 0; i < VECSIZE; i++) {
            if (!vec1->null[i] && !vec2->null[i]) {
                result->values[i] = vec1->values[i] + vec2->values[i];
                result->null[i] = 0;
            } else {
                result->null[i] = 1;
            }
        }
    }

    compile with:
    # clang++-8 `llvm-config-8 --cxxflags --ldflags --libs` -std=c++17 solve.cpp -o exec.out
    */
    llvm::LLVMContext &context = module->getContext();
    llvm::IRBuilder<> builder(context);
    llvm::Type *struct_values = llvm::PointerType::get(builder.getInt32Ty(), 0);
    llvm::Type *struct_null = llvm::PointerType::get(builder.getInt8Ty(), 0);
    llvm::StructType *struct_Ty = llvm::StructType::create(context, "Vector");
    struct_Ty->setBody({struct_values, struct_null});
    std::vector<llvm::Type*> ArgTypes = {struct_Ty->getPointerTo(0), struct_Ty->getPointerTo(0), struct_Ty->getPointerTo(0)};
    auto *funcType = llvm::FunctionType::get(builder.getVoidTy(), ArgTypes, false);
    auto *fooFunc = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, "addv", module
    );
    std::vector<llvm::Value*> Args;
    for (auto& arg : fooFunc->args()) {
        Args.push_back(&arg);
    }
    auto *l3 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l8 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l11 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l20 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l29 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l57 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l64 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l65 = llvm::BasicBlock::Create(context, "", fooFunc);
    auto *l68 = llvm::BasicBlock::Create(context, "", fooFunc);
    builder.SetInsertPoint(l3);
    auto *p4 = builder.CreateAlloca(struct_Ty->getPointerTo(0), 0, nullptr, "");
    auto *p5 = builder.CreateAlloca(struct_Ty->getPointerTo(0), 0, nullptr, "");
    auto *p6 = builder.CreateAlloca(struct_Ty->getPointerTo(0), 0, nullptr, "");
    auto *p7 = builder.CreateAlloca(builder.getInt32Ty(), 0, nullptr, "");
    builder.CreateStore(Args[0], p4);
    builder.CreateStore(Args[1], p5);
    builder.CreateStore(Args[2], p6);
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt32Ty(), 0), p7);
    builder.CreateBr(l8);
    builder.SetInsertPoint(l8);
    auto *p9 = builder.CreateLoad(p7);
    auto *p10 = builder.CreateICmpSLT(p9, llvm::ConstantInt::get(builder.getInt32Ty(), VECSIZE));
    builder.CreateCondBr(p10, l11, l68);
    builder.SetInsertPoint(l11);
    auto *p12 = builder.CreateLoad(p4);
    auto *p13 = builder.CreateInBoundsGEP(p12, 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 1)
        });
    auto *p14 = builder.CreateLoad(p13);
    auto *p15 = builder.CreateLoad(p7);
    auto *p16 = builder.CreateSExt(p15, builder.getInt64Ty());
    auto *p17 = builder.CreateInBoundsGEP(p14, p16);
    auto *p18 = builder.CreateLoad(p17);
    auto *p19 = builder.CreateICmpNE(p18, llvm::ConstantInt::get(builder.getInt8Ty(), 0));
    builder.CreateCondBr(p19, l57, l20);
    builder.SetInsertPoint(l20);
    auto *p21 = builder.CreateLoad(p5);
    auto *p22 = builder.CreateInBoundsGEP(p21, 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 1)
        });
    auto *p23 = builder.CreateLoad(p22);
    auto *p24 = builder.CreateLoad(p7);
    auto *p25 = builder.CreateSExt(p24, builder.getInt64Ty());
    auto *p26 = builder.CreateInBoundsGEP(p23, p25);
    auto *p27 = builder.CreateLoad(p26);
    auto *p28 = builder.CreateICmpNE(p27, llvm::ConstantInt::get(builder.getInt8Ty(), 0));
    builder.CreateCondBr(p28, l57, l29);
    builder.SetInsertPoint(l29);

    auto *p30 = builder.CreateLoad(p4);
    auto *p31 = builder.CreateInBoundsGEP(p30, 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 0)
        });
    auto *p32 = builder.CreateLoad(p31);
    auto *p33 = builder.CreateLoad(p7);
    auto *p34 = builder.CreateSExt(p33, builder.getInt64Ty());
    auto *p35 = builder.CreateInBoundsGEP(p32, p34);
    auto *p36 = builder.CreateLoad(p35);
    auto *p37 = builder.CreateLoad(p5);
    auto *p38 = builder.CreateInBoundsGEP(p37, 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 0)
        });
    auto *p39 = builder.CreateLoad(p38);
    auto *p40 = builder.CreateLoad(p7);
    auto *p41 = builder.CreateSExt(p40, builder.getInt64Ty());
    auto *p42 = builder.CreateInBoundsGEP(p39, p41);
    auto *p43 = builder.CreateLoad(p42);
    auto *p44 = builder.CreateAdd(p36, p43);
    auto *p45 = builder.CreateLoad(p6);
    auto *p46 = builder.CreateInBoundsGEP(p45, 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 0)
        });
    auto *p47 = builder.CreateLoad(p46);
    auto *p48 = builder.CreateLoad(p7);
    auto *p49 = builder.CreateSExt(p48, builder.getInt64Ty());
    auto *p50 = builder.CreateInBoundsGEP(p47, p49);
    builder.CreateStore(p44, p50);
    auto *p51 = builder.CreateLoad(p6);
    auto *p52 = builder.CreateInBoundsGEP(p51, 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 1)
        });
    auto *p53 = builder.CreateLoad(p52);
    auto *p54 = builder.CreateLoad(p7);
    auto *p55 = builder.CreateSExt(p54, builder.getInt64Ty());
    auto *p56 = builder.CreateInBoundsGEP(p53, p55);
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt8Ty(), 0), p56);
    builder.CreateBr(l64);
    builder.SetInsertPoint(l57);
    auto *p58 = builder.CreateLoad(p6);
    auto *p59 = builder.CreateInBoundsGEP(p58, 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 1)
        });
    auto *p60 = builder.CreateLoad(p59);
    auto *p61 = builder.CreateLoad(p7);
    auto *p62 = builder.CreateSExt(p61, builder.getInt64Ty());
    auto *p63 = builder.CreateInBoundsGEP(p60, p62);
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt8Ty(), 1), p63);
    builder.CreateBr(l64);
    builder.SetInsertPoint(l64);
    builder.CreateBr(l65);
    builder.SetInsertPoint(l65);
    auto *p66 = builder.CreateLoad(p7);
    auto *p67 = builder.CreateAdd(p66, llvm::ConstantInt::get(builder.getInt8Ty(), 1));
    builder.CreateStore(p67, p7);
    builder.CreateBr(l8);
    builder.SetInsertPoint(l68);
    builder.CreateRet(llvm::UndefValue::get(llvm::Type::getVoidTy(context)));
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

    auto* func = createAddvFunction(module);

    llvm::outs() << "We just constructed this LLVM module:\n\n" << *module;
    llvm::outs() << "\n\nRunning foo: ";
    llvm::outs().flush();

    auto* raw_ptr = executionEngine->getPointerToFunction(func);
    auto* func_ptr = (void(*)(Vector*, Vector*, Vector*))raw_ptr;
    executionEngine->finalizeObject();

    // Execute
    Vector arg1 = (Vector){.values = (VECTYPE*)std::calloc(VECSIZE, sizeof(VECTYPE)), .null = (char*)std::calloc(VECSIZE, sizeof(char))};
    Vector arg2 = (Vector){.values = (VECTYPE*)std::calloc(VECSIZE, sizeof(VECTYPE)), .null = (char*)std::calloc(VECSIZE, sizeof(char))};
    Vector res0 = (Vector){.values = (VECTYPE*)std::calloc(VECSIZE, sizeof(VECTYPE)), .null = (char*)std::calloc(VECSIZE, sizeof(char))};
    std::srand(123);
    for (int i = 0; i < VECSIZE; ++i) {
        arg1.values[i] = std::rand() % 100;
        arg2.values[i] = std::rand() % 100;
        arg1.null[i] = std::rand() % 2 ? 1 : 0;
        arg2.null[i] = std::rand() % 2 ? 1 : 0;
    }
    func_ptr(&arg1, &arg2, &res0);
    for (int i = 0; i < VECSIZE; ++i) {
        std::cout << "(" << arg1.values[i] << ", " << (int)arg1.null[i] << ") & ";
        std::cout << "(" << arg2.values[i] << ", " << (int)arg2.null[i] << ") = ";
        std::cout << "(" << res0.values[i] << ", " << (int)res0.null[i] << ")\n";
    }
    
    return 0;
}