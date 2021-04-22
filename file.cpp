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