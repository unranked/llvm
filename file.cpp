#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

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
    std::vector<std::string> ArgNames = {"arg1", "arg2", "result"};
    auto *funcType = llvm::FunctionType::get(builder.getVoidTy(), ArgTypes, false);
    auto *fooFunc = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, "addv", module
    );
    std::unordered_map<std::string, llvm::Value*> Args;
    for (auto& arg : fooFunc->args()) {
        arg.setName(ArgNames[arg.getArgNo()]);
        Args[ArgNames[arg.getArgNo()]] = &arg;
    }
    auto *entry = llvm::BasicBlock::Create(context, "entry", fooFunc);
    auto *loop_check = llvm::BasicBlock::Create(context, "loop_check", fooFunc);
    auto *loop = llvm::BasicBlock::Create(context, "loop", fooFunc);
    auto *afterloop = llvm::BasicBlock::Create(context, "afterloop", fooFunc);
    auto *increment = llvm::BasicBlock::Create(context, "increment", fooFunc);
    auto *second_cond_label = llvm::BasicBlock::Create(context, "second_cond_label", fooFunc);
    auto *if_label = llvm::BasicBlock::Create(context, "if_label", fooFunc);
    auto *else_label = llvm::BasicBlock::Create(context, "else_label", fooFunc);
    builder.SetInsertPoint(entry);
    auto *iptr = builder.CreateAlloca(builder.getInt32Ty(), 0, nullptr, "iptr");
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt32Ty(), 0), iptr);

    builder.CreateBr(loop_check);
    builder.SetInsertPoint(loop_check);

    auto *i = builder.CreateLoad(iptr, "i");
    auto *loop_cond = builder.CreateICmpSLT(i, llvm::ConstantInt::get(builder.getInt32Ty(), VECSIZE), "loop_cond");
    builder.CreateCondBr(loop_cond, loop, afterloop);
    builder.SetInsertPoint(loop);


    auto *result_null_ptr = builder.CreateInBoundsGEP(Args["result"], 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 1)
        });
    auto *result_null = builder.CreateLoad(result_null_ptr);
    auto *result_null_i_ptr = builder.CreateInBoundsGEP(result_null, i, "result_null_i_ptr");

    auto *arg1_null_ptr = builder.CreateInBoundsGEP(Args["arg1"], 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 1)
        });
    auto *arg1_null = builder.CreateLoad(arg1_null_ptr);
    auto *arg1_null_i_ptr = builder.CreateInBoundsGEP(arg1_null, i);
    auto *arg1_null_i = builder.CreateLoad(arg1_null_i_ptr, "arg1_null_i");
    
    auto *first_cond = builder.CreateICmpNE(arg1_null_i, llvm::ConstantInt::get(builder.getInt8Ty(), 0), "first_cond");
    builder.CreateCondBr(first_cond, else_label, second_cond_label);
    builder.SetInsertPoint(second_cond_label);
    auto *arg2_null_ptr = builder.CreateInBoundsGEP(Args["arg2"], 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 1)
        });
    auto *arg2_null = builder.CreateLoad(arg2_null_ptr);
    auto *arg2_null_i_ptr = builder.CreateInBoundsGEP(arg2_null, i);
    auto *arg2_null_i = builder.CreateLoad(arg2_null_i_ptr, "arg2_null_i");
    
    auto *second_cond = builder.CreateICmpNE(arg2_null_i, llvm::ConstantInt::get(builder.getInt8Ty(), 0), "second_cond");

    builder.CreateCondBr(second_cond, else_label, if_label);
    builder.SetInsertPoint(if_label);

    builder.CreateStore(llvm::ConstantInt::get(builder.getInt8Ty(), 0), result_null_i_ptr);


    auto *arg1_values_ptr = builder.CreateInBoundsGEP(Args["arg1"], 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 0)
        });
    auto *arg1_values = builder.CreateLoad(arg1_values_ptr);
    auto *arg1_values_i_ptr = builder.CreateInBoundsGEP(arg1_values, i);
    auto *arg1_values_i = builder.CreateLoad(arg1_values_i_ptr, "arg1_values_i");

    auto *arg2_values_ptr = builder.CreateInBoundsGEP(Args["arg2"], 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 0)
        });
    auto *arg2_values = builder.CreateLoad(arg2_values_ptr);
    auto *arg2_values_i_ptr = builder.CreateInBoundsGEP(arg2_values, i);
    auto *arg2_values_i = builder.CreateLoad(arg2_values_i_ptr, "arg2_values_i");

    auto *result_values_ptr = builder.CreateInBoundsGEP(Args["result"], 
        {
            llvm::ConstantInt::get(builder.getInt32Ty(), 0), 
            llvm::ConstantInt::get(builder.getInt32Ty(), 0)
        });
    auto *result_values = builder.CreateLoad(result_values_ptr);
    auto *result_values_i_ptr = builder.CreateInBoundsGEP(result_values, i);
    auto *sum = builder.CreateAdd(arg1_values_i, arg2_values_i, "sum");
    builder.CreateStore(sum, result_values_i_ptr);




    builder.CreateBr(increment);
    builder.SetInsertPoint(else_label);
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt8Ty(), 1), result_null_i_ptr);







/*
    builder.CreateStore(Args["arg1"], p4);
    builder.CreateStore(Args["arg2"], p5);
    builder.CreateStore(Args["result"], p6);
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
    */

    builder.CreateBr(increment);
    builder.SetInsertPoint(increment);
    auto *inc = builder.CreateAdd(i, llvm::ConstantInt::get(builder.getInt32Ty(), 1), "inc");
    builder.CreateStore(inc, iptr);
    builder.CreateBr(loop_check);
    builder.SetInsertPoint(afterloop);
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