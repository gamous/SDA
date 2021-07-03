#pragma once
#include "DecExecContext.h"
#include "PCodeInterpreter.h"
#include <functional>

namespace CE::Decompiler
{
	class AbstractPrimaryDecompiler
	{
	protected:
		struct DecompiledBlockInfo {
			PCodeBlock* m_pcodeBlock = nullptr;
			DecBlock* m_decBlock = nullptr;
			ExecContext* m_execCtx = nullptr;
			int m_enterCount = 0;
			int m_versionOfDecompiling = 0;
			bool m_isDecompiled = false;
		};

	private:
		AbstractRegisterFactory* m_registerFactory;
		int m_loopsCount = 0;

	public:
		struct LocalVarInfo {
			PCode::Register m_register;
			std::set<ExecContext*> m_execCtxs;
			bool m_used = false;
		};

		std::map<Symbol::LocalVariable*, LocalVarInfo> m_localVars;

		DecompiledCodeGraph* m_decompiledGraph;
		ReturnInfo m_returnInfo;
		std::map<PCodeBlock*, DecompiledBlockInfo> m_decompiledBlocks;

		AbstractPrimaryDecompiler(DecompiledCodeGraph* decompiledGraph, AbstractRegisterFactory* registerFactory, ReturnInfo returnInfo)
			: m_decompiledGraph(decompiledGraph), m_registerFactory(registerFactory), m_returnInfo(returnInfo)
		{}

		~AbstractPrimaryDecompiler();

		void start();

		AbstractRegisterFactory* getRegisterFactory() const;

		// called when a function call appears during decompiling
		FunctionCallInfo requestFunctionCallInfo(ExecContext* ctx, PCode::Instruction* instr);

	protected:
		// called when a function call appears during decompiling
		virtual FunctionCallInfo requestFunctionCallInfo(ExecContext* ctx, PCode::Instruction* instr, int dstLocOffset) = 0;

		virtual void onFinal() {}

	private:
		void interpreteGraph(PCodeBlock* pcodeBlock, int versionOfDecompiling = 1);
		
		void setAllDecBlocksLinks();
	};

	class PrimaryDecompiler : public AbstractPrimaryDecompiler
	{
	public:
		using FuncCallInfoCallbackType = std::function<FunctionCallInfo(PCode::Instruction*, int)>;
	private:
		FuncCallInfoCallbackType m_funcCallInfoCallback;
	public:
		PrimaryDecompiler(DecompiledCodeGraph* decompiledGraph, AbstractRegisterFactory* registerFactory, ReturnInfo returnInfo, FuncCallInfoCallbackType funcCallInfoCallback)
			: AbstractPrimaryDecompiler(decompiledGraph, registerFactory, returnInfo), m_funcCallInfoCallback(funcCallInfoCallback)
		{}

	protected:
		FunctionCallInfo requestFunctionCallInfo(ExecContext* ctx, PCode::Instruction* instr, int funcOffset) override;
	};
};