#include "DecGraphLinesExpanding.h"

using namespace CE::Decompiler;

CE::Decompiler::Optimization::GraphLinesExpanding::GraphLinesExpanding(DecompiledCodeGraph* decGraph)
	: GraphModification(decGraph)
{}

void CE::Decompiler::Optimization::GraphLinesExpanding::start() {
	for (auto decBlock : m_decGraph->getDecompiledBlocks()) {
		processBlock(decBlock);
	}
}

void CE::Decompiler::Optimization::GraphLinesExpanding::processBlock(DecBlock* block) const
{
	auto newSeqLines = block->getSymbolParallelAssignmentLines();
	block->getSymbolParallelAssignmentLines().clear();

	for (auto it = newSeqLines.begin(); it != newSeqLines.end(); it++) {
		auto symbolAssignmentLine = *it;
		//let it be {localVar1 = 1}
		auto localVar = symbolAssignmentLine->getDstSymbolLeaf()->m_symbol;

		//determine whether does it need a temp symbol to store value for next lines, and if yes then create it
		Symbol::LocalVariable* localTempVar = nullptr;
		for (auto it2 = std::next(it); it2 != newSeqLines.end(); it2++) {
			auto anotherNextSeqLine = *it2;
			std::list<ExprTree::SymbolLeaf*> symbolLeafs;
			GatherSymbolLeafsFromNode(anotherNextSeqLine->getSrcNode(), symbolLeafs, localVar);
			//if we find anything like this {localVar2 = localVar1 + 1}
			if (!symbolLeafs.empty()) {
				if (!localTempVar) {
					localTempVar = new Symbol::LocalVariable(localVar->getSize()); // create {tempVar1}
					m_decGraph->addSymbol(localTempVar);
				}
				for (auto symbolLeaf : symbolLeafs) { // transforming to {localVar2 = tempVar1 + 1}
					symbolLeaf->replaceWith(new ExprTree::SymbolLeaf(localTempVar));
					delete symbolLeaf;
				}
			}
		}

		if (localTempVar) { //if temp var requasted then add seq. assignments in the begining
			block->addSeqLine(new ExprTree::SymbolLeaf(localTempVar), new ExprTree::SymbolLeaf(localVar));
		}
		block->getSeqAssignmentLines().push_back(symbolAssignmentLine);
	}
}
