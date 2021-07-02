#include "PrimaryDecompiler.h"

using namespace CE;
using namespace CE::Decompiler;

CE::Decompiler::AbstractPrimaryDecompiler::~AbstractPrimaryDecompiler() {
	for (auto& pair : m_decompiledBlocks) {
		auto& decBlockInfo = pair.second;
		delete decBlockInfo.m_execCtx;
	}

	for (auto& pair : m_localVars) {
		auto localVar = pair.first;
		auto& localVarInfo = pair.second;
		if (!localVarInfo.m_used)
			delete localVar;
	}
}

void CE::Decompiler::AbstractPrimaryDecompiler::start() {
	// prepare
	for (auto pcodeBlock : m_decompiledGraph->getFuncGraph()->getBlocks()) {
		DecBlock* newDecBlock;
		if (!pcodeBlock->getNextNearBlock() && !pcodeBlock->getNextFarBlock()) {
			newDecBlock = new EndDecBlock(m_decompiledGraph, pcodeBlock, pcodeBlock->m_level);
		}
		else {
			newDecBlock = new DecBlock(m_decompiledGraph, pcodeBlock, pcodeBlock->m_level);
		}

		DecompiledBlockInfo decompiledBlock;
		decompiledBlock.m_pcodeBlock = pcodeBlock;
		decompiledBlock.m_execCtx = new ExecContext(this, pcodeBlock);
		decompiledBlock.m_decBlock = newDecBlock;
		decompiledBlock.m_decBlock->m_name = Helper::String::NumberToHex(pcodeBlock->ID);

		m_decompiledBlocks[pcodeBlock] = decompiledBlock;
	}
	setAllDecBlocksLinks();

	// start decompiling
	interpreteGraph(m_decompiledGraph->getFuncGraph()->getStartBlock());

	// building decompiled graph
	for (auto& it : m_decompiledBlocks) {
		auto& info = it.second;
		m_decompiledGraph->getDecompiledBlocks().push_back(info.m_decBlock);
	}
	m_decompiledGraph->sortBlocksByLevel();

	// end
	onFinal();
}

AbstractRegisterFactory* CE::Decompiler::AbstractPrimaryDecompiler::getRegisterFactory() {
	return m_registerFactory;
}

// called when a function call appears during decompiling

FunctionCallInfo CE::Decompiler::AbstractPrimaryDecompiler::requestFunctionCallInfo(ExecContext* ctx, PCode::Instruction* instr) {
	int funcOffset = 0;
	auto& constValues = m_decompiledGraph->getFuncGraph()->getConstValues();
	auto it = constValues.find(instr);
	if (it != constValues.end())
		funcOffset = (int)it->second;
	return requestFunctionCallInfo(ctx, instr, funcOffset);
}

void CE::Decompiler::AbstractPrimaryDecompiler::interpreteGraph(PCodeBlock* pcodeBlock, int versionOfDecompiling) {
	auto& blockInfo = m_decompiledBlocks[pcodeBlock];

	// todo: redecompile block because of loops

	blockInfo.m_enterCount++;
	auto refHighBlocksCount = blockInfo.m_decBlock->getRefHighBlocksCount();
	if (blockInfo.m_enterCount >= refHighBlocksCount) {
		// save the register context in the begining
		blockInfo.m_execCtx->m_startRegisterExecCtx.copyFrom(&blockInfo.m_execCtx->m_registerExecCtx);

		// execute the instructions and then change the execution context
		blockInfo.m_decBlock->clearCode();
		PCode::InstructionInterpreter instructionInterpreter(this, blockInfo.m_decBlock, blockInfo.m_execCtx);
		for (auto instr : pcodeBlock->getInstructions()) {
			instructionInterpreter.execute(instr);
		}

		auto hasAlreadyDecompiled = blockInfo.m_isDecompiled;
		blockInfo.m_isDecompiled = true;
		blockInfo.m_versionOfDecompiling = versionOfDecompiling;

		// go to the next blocks to deompile them
		for (auto nextPCodeBlock : pcodeBlock->getNextBlocks()) {
			auto nextDecBlockInfo = m_decompiledBlocks[nextPCodeBlock];

			auto nextVersionOfDecompiling = versionOfDecompiling;
			if (nextPCodeBlock->m_level < pcodeBlock->m_level) {
				// if it is a loop
				if (!hasAlreadyDecompiled)
					nextVersionOfDecompiling = ++m_loopsCount + 1;
			}

			if (nextVersionOfDecompiling <= nextDecBlockInfo.m_versionOfDecompiling)
				continue;

			if (nextDecBlockInfo.m_isDecompiled) {
				nextDecBlockInfo.m_execCtx->m_registerExecCtx.copyFrom(&nextDecBlockInfo.m_execCtx->m_startRegisterExecCtx);
			}
			nextDecBlockInfo.m_execCtx->join(blockInfo.m_execCtx);
			interpreteGraph(nextPCodeBlock, nextVersionOfDecompiling);
		}
	}
}

void CE::Decompiler::AbstractPrimaryDecompiler::setAllDecBlocksLinks() {
	for (const auto& pair : m_decompiledBlocks) {
		auto& decBlockInfo = pair.second;
		if (auto nextPCodeBlock = decBlockInfo.m_pcodeBlock->getNextNearBlock()) {
			decBlockInfo.m_decBlock->setNextNearBlock(m_decompiledBlocks[nextPCodeBlock].m_decBlock);
		}
		if (auto nextPCodeBlock = decBlockInfo.m_pcodeBlock->getNextFarBlock()) {
			decBlockInfo.m_decBlock->setNextFarBlock(m_decompiledBlocks[nextPCodeBlock].m_decBlock);
		}
	}
}

FunctionCallInfo CE::Decompiler::PrimaryDecompiler::requestFunctionCallInfo(ExecContext* ctx, PCode::Instruction* instr, int funcOffset) {
	return m_funcCallInfoCallback(instr, funcOffset);
}
