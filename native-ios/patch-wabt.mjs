import fs from 'node:fs';
import assert from 'node:assert/strict';
const file=process.argv[2]+'/src/c-writer.cc';
let source=fs.readFileSync(file,'utf8');
const from=`      case ExprType::AtomicWait:
      case ExprType::AtomicNotify:
      case ExprType::CallRef:`;
const to=`      case ExprType::AtomicWait: {
        const auto inst = cast<AtomicWaitExpr>(&expr);
        Memory* memory = module_->memories[module_->GetMemoryIndex(inst->memidx)];
        assert(memory->page_limits.is_shared);
        const bool is64 = inst->opcode == Opcode::MemoryAtomicWait64;
        Write(StackVar(2, Type::I32), " = native_wait", is64 ? "64" : "32", "(",
              ExternalInstancePtr(ModuleFieldType::Memory, memory->name), ", ");
        WriteMemoryAddress(2, memory, inst->offset);
        Write(", ", StackVar(1), ", (int64_t)", StackVar(0), ");", Newline());
        DropTypes(3);
        PushType(Type::I32);
      } break;
      case ExprType::AtomicNotify: {
        const auto inst = cast<AtomicNotifyExpr>(&expr);
        Memory* memory = module_->memories[module_->GetMemoryIndex(inst->memidx)];
        assert(memory->page_limits.is_shared);
        Write(StackVar(1, Type::I32), " = native_notify(",
              ExternalInstancePtr(ModuleFieldType::Memory, memory->name), ", ");
        WriteMemoryAddress(1, memory, inst->offset);
        Write(", ", StackVar(0), ");", Newline());
        DropTypes(2);
        PushType(Type::I32);
      } break;
      case ExprType::CallRef:`;
assert.equal(source.split(from).length,2,'Pinned compiler source changed');
source=source.replace(from,to);
fs.writeFileSync(file,source);
console.log('Added checked native atomic wait/notify emission.');
