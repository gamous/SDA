#pragma once
#include <decompiler/PCode/DecPCode.h>
#include <decompiler/PCode/DecPCodeConstValueCalc.h>
#include <set>

namespace CE::Decompiler
{
	class ImagePCodeGraph;
	class FunctionPCodeGraph;

	// pcode graph for a non-branched block
	class PCodeBlock
	{
		uint64_t m_minOffset;
		uint64_t m_maxOffset;
		std::list<PCode::Instruction*> m_instructions; // content of the block
		PCodeBlock* m_nextNearBlock = nullptr;
		PCodeBlock* m_nextFarBlock = nullptr;
	public:
		int ID = 0;
		int m_level = 0;
		std::list<PCodeBlock*> m_blocksReferencedTo;
		FunctionPCodeGraph* m_funcPCodeGraph = nullptr;

		PCodeBlock() = default;

		PCodeBlock(uint64_t minOffset, uint64_t maxOffset);

		void removeRefBlock(PCodeBlock* block);

		void disconnect();

		std::list<PCode::Instruction*>& getInstructions();

		uint64_t getMinOffset() const;

		uint64_t getMaxOffset() const;

		void setMaxOffset(uint64_t offset);

		void removeNextBlock(PCodeBlock* nextBlock);

		void setNextNearBlock(PCodeBlock* nextBlock);

		void setNextFarBlock(PCodeBlock* nextBlock);

		PCodeBlock* getNextNearBlock() const;

		PCodeBlock* getNextFarBlock() const;

		std::list<PCodeBlock*> getNextBlocks() const;

		PCode::Instruction* getLastInstruction();

		std::string printDebug(void* addr, const std::string& tabStr, bool extraInfo, bool pcode);
	};

	// pcode graph (consisted of PCode connected blocks) for a function
	class FunctionPCodeGraph
	{
		ImagePCodeGraph* m_imagePCodeGraph;
		PCodeBlock* m_startBlock = nullptr;
		std::set<PCodeBlock*> m_blocks;
		std::set<FunctionPCodeGraph*> m_refFuncCalls;
		std::set<FunctionPCodeGraph*> m_nonVirtFuncCalls;
		std::set<FunctionPCodeGraph*> m_virtFuncCalls;
		std::map<PCode::Instruction*, DataValue> m_constValues;
	public:
		FunctionPCodeGraph(ImagePCodeGraph* imagePCodeGraph);

		ImagePCodeGraph* getImagePCodeGraph() const;

		void setStartBlock(PCodeBlock* block);

		// head is a function that has not parents (main/all virtual functions)
		bool isHead() const;

		const std::set<FunctionPCodeGraph*>& getRefFuncCalls() const;

		const std::set<FunctionPCodeGraph*>& getNonVirtFuncCalls() const;

		const std::set<FunctionPCodeGraph*>& getVirtFuncCalls() const;

		void addNonVirtFuncCall(FunctionPCodeGraph* funcGraph);

		void addVirtFuncCall(FunctionPCodeGraph* funcGraph);

		const std::set<PCodeBlock*>& getBlocks() const;

		void addBlock(PCodeBlock* block);

		PCodeBlock* getStartBlock() const;

		std::map<PCode::Instruction*, PCode::DataValue>& getConstValues();

		void printDebug(void* addr);
	};

	// pcode graph (consisted of NON-connected function graphs in final state) for a whole program
	class ImagePCodeGraph
	{
		std::list<FunctionPCodeGraph> m_funcGraphList;
		std::list<FunctionPCodeGraph*> m_headFuncGraphs;
		std::map<uint64_t, PCodeBlock> m_blocks;
	public:
		// exceptions
		class BlockNotFoundException : public std::exception {};

		ImagePCodeGraph();

		FunctionPCodeGraph* createFunctionGraph();

		PCodeBlock* createBlock(uint64_t minOffset, uint64_t maxOffset);

		PCodeBlock* createBlock(uint64_t offset);

		const auto& getHeadFuncGraphs() const;

		std::list<FunctionPCodeGraph>& getFunctionGraphList();

		FunctionPCodeGraph* getEntryFunctionGraph();

		PCodeBlock* getBlockAtOffset(uint64_t offset, bool halfInterval = true);

		FunctionPCodeGraph* getFuncGraphAt(uint64_t offset, bool halfInterval = true);

		// add all head functions into the list HeadFuncGraphs
		void fillHeadFuncGraphs();
	};
};