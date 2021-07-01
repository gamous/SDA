#pragma once
#include <decompiler/Graph/DecCodeGraph.h>

namespace CE::Decompiler
{
	class PrimaryDecompiler;
	class ExecContext;
	class AbstractPrimaryDecompiler;

	// it contains expr. values for each register
	struct RegisterExecContext
	{
		struct RegisterInfo {
			PCode::Register m_register;
			TopNode* m_expr;
			ExecContext* m_srcExecContext;

			// need for figuring out return registers (EAX, XMM)
			enum REGISTER_USING {
				REGISTER_NOT_USING,
				REGISTER_PARTIALLY_USING,
				REGISTER_FULLY_USING
			} m_using = REGISTER_NOT_USING;
		};

		struct RegisterPart {
			BitMask64 m_regMask; // register range value mask
			BitMask64 m_maskToChange; // that part of m_regMask where m_expr located
			ExprTree::INode* m_expr = nullptr;
		};

		AbstractPrimaryDecompiler* m_decompiler;
		std::map<PCode::RegisterId, std::list<RegisterInfo>> m_registers;
		ExecContext* m_execContext;
		bool m_isFilled = false;

		RegisterExecContext(AbstractPrimaryDecompiler* decompiler, ExecContext* execContext)
			: m_decompiler(decompiler), m_execContext(execContext)
		{}

		void clear();

		ExprTree::INode* requestRegister(const PCode::Register& reg);

		void setRegister(const PCode::Register& reg, ExprTree::INode* newExpr);

		void copyFrom(RegisterExecContext* ctx);

		void join(RegisterExecContext* ctx);

	private:
		std::list<RegisterPart> findRegisterParts(int regId, BitMask64& needReadMask);

		BitMask64 calculateMaxMask(const std::list<RegisterInfo>& regs);

		static std::list<BitMask64> FindNonIntersectedMasks(const std::list<RegisterInfo>& regs);

		static std::set<BitMask64> CalculateMasks(const std::list<RegisterInfo>& regs1, const std::list<RegisterInfo>& regs2);

		static ExprTree::INode* CreateExprFromRegisterParts(std::list<RegisterPart> regParts, BitMask64 requestRegMask);
	};

	// it containts register exec. context and values for symbol varnodes
	class ExecContext
	{
		std::map<PCode::SymbolVarnode*, TopNode*> m_symbolVarnodes;
	public:
		RegisterExecContext m_startRegisterExecCtx; // state before decompiling
		RegisterExecContext m_registerExecCtx; // state during decompiling and after
		PCodeBlock* m_pcodeBlock; // need as a key only

		ExecContext(AbstractPrimaryDecompiler* decompiler, PCodeBlock* pcodeBlock = nullptr)
			: m_startRegisterExecCtx(decompiler, this), m_registerExecCtx(m_startRegisterExecCtx), m_pcodeBlock(pcodeBlock)
		{}

		~ExecContext();

		ExprTree::INode* requestVarnode(PCode::Varnode* varnode);

		void setVarnode(PCode::Varnode* varnode, ExprTree::INode* newExpr);

		void join(ExecContext* ctx);
	};
};