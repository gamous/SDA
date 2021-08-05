#pragma once
#include "ImageDecorator.h"
#include "decompiler/PCode/DecPCodeInstructionPool.h"
#include "decompiler/PCode/DecPCodeVirtualMachine.h"
#include "decompiler/PCode/DecRegisterFactory.h"
#include "decompiler/Graph/DecPCodeGraph.h"
#include "decompiler/Graph/DecCodeGraphBlock.h"
#include "imgui_wrapper/Window.h"
#include "imgui_wrapper/controls/AbstractPanel.h"
#include "imgui_wrapper/controls/Text.h"
#include "utilities/Helper.h"

namespace GUI
{
	class Debugger : public Control
	{
		class ExecContextViewerPanel : public AbstractPanel
		{
			CE::Decompiler::PCode::VmExecutionContext* m_execCtx;
			CE::Decompiler::AbstractRegisterFactory* m_registerFactory;
		public:
			ExecContextViewerPanel(CE::Decompiler::PCode::VmExecutionContext* execCtx, CE::Decompiler::AbstractRegisterFactory* registerFactory)
				: AbstractPanel("Execution Context Viewer"), m_execCtx(execCtx), m_registerFactory(registerFactory)
			{}

		private:
			void renderPanel() override {
				if (ImGui::BeginTable("##empty", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV))
				{
					ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 70.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None);
					ImGui::TableHeadersRow();

					renderSymbolVarnodeRows();
					
					renderFlagRegisterRow();
					
					for(int regId = ZYDIS_REGISTER_RAX; regId <= ZYDIS_REGISTER_R15; regId++) {
						renderRegisterRow(regId, 0x8);
					}

					for (int regId = ZYDIS_REGISTER_XMM0; regId <= ZYDIS_REGISTER_XMM15; regId++) {
						renderRegisterRow(regId, 0x10);
					}

					ImGui::EndTable();
				}
			}

			void renderSymbolVarnodeRows() {
				const auto& symbolVarnodes = m_execCtx->getSymbolVarnodes();
				for(const auto& [symbolVarnode, value] : symbolVarnodes) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					CE::Decompiler::InstructionTextGenerator instrTextGenerator;
					instrTextGenerator.generateVarnode(symbolVarnode);
					Text::Text(instrTextGenerator.m_text).show();
					ImGui::TableNextColumn();
					Text::Text("0x" + Helper::String::NumberToHex(value, false)).show();
				}
			}

			void renderRegisterRow(int regId, int size) {
				using namespace Helper::String;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				Text::Text(CE::Decompiler::PCode::InstructionViewGenerator::GetRegisterName(regId)).show();
				ImGui::TableNextColumn();

				
				int offset = size;
				while (offset > 0) {
					offset -= 0x8;
					const auto reg = m_registerFactory->createRegister(regId, size, offset);
					CE::Decompiler::DataValue regValue;
					if (getRegisterValue(reg.getId(), regValue)) {
						Text::Text(NumberToHex(regValue, true)).show();
					} else {
						Text::Text("----------------").show();
					}
					SameLine(5.0f);
				}
			}

			void renderFlagRegisterRow() {
				const auto flags = {
					ZYDIS_CPUFLAG_CF,
					ZYDIS_CPUFLAG_OF,
					ZYDIS_CPUFLAG_SF,
					ZYDIS_CPUFLAG_ZF,
					ZYDIS_CPUFLAG_AF,
					ZYDIS_CPUFLAG_PF
				};
				
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				Text::Text("flags").show();
				ImGui::TableNextColumn();

				CE::Decompiler::DataValue regValue;
				if (getRegisterValue(ZYDIS_REGISTER_RFLAGS << 8, regValue)) {
					for (const auto flag : flags) {
						const auto flagName = CE::Decompiler::PCode::InstructionViewGenerator::GetFlagName(flag);
						const auto value = regValue >> flag & 0b1;
						Text::Text(flagName + ": " + std::to_string(value)).show();
						SameLine(5.0f);
					}
				}
				else {
					Text::Text("-").show();
				}
			}

			bool getRegisterValue(int regId, CE::Decompiler::DataValue& value) const {
				const auto& registers = m_execCtx->getRegisters();
				const auto it = registers.find(regId);
				if (it == registers.end())
					return false;
				value = it->second;
				return true;
			}
		};

		CE::Decompiler::RegisterFactoryX86 m_registerFactoryX86;
		CE::Decompiler::PCode::VmExecutionContext m_execCtx;
		CE::Decompiler::PCode::VmMemoryContext m_memCtx;
		CE::Decompiler::PCode::VirtualMachine m_vm;
		CE::ImageDecorator* m_imageDec;
		CE::ComplexOffset m_offset;

		StdWindow* m_execCtxViewerWin;
		EventHandler<bool> m_instrHandler;
	public:
		enum class StepWidth
		{
			STEP_PCODE_INSTR,
			STEP_ORIGINAL_INSTR,
			STEP_CODE_LINE
		};
		StepWidth m_stepWidth = StepWidth::STEP_PCODE_INSTR;
		CE::Decompiler::PCode::Instruction* m_curInstr = nullptr;
		CE::Decompiler::PCodeBlock* m_curPCodeBlock = nullptr;
		CE::Decompiler::DecBlock::BlockTopNode* m_curBlockTopNode = nullptr;
		
		Debugger(CE::ImageDecorator* imageDec, CE::ComplexOffset startOffset)
			: m_imageDec(imageDec), m_offset(startOffset), m_vm(&m_execCtx, &m_memCtx, false)
		{
			defineCurInstrutction();
			
			m_execCtxViewerWin = new StdWindow(new ExecContextViewerPanel(&m_execCtx, &m_registerFactoryX86));
		}

		void instrHandler(const std::function<void(bool)>& handler) {
			m_instrHandler = handler;
		}

		bool isNewOrigInstruction() const {
			return m_curInstr->m_orderId == 0;
		}

		void renderDebugMenu() {
			if (ImGui::MenuItem("Step Over", "F8")) {
				stepOver(true);
			}

			if (ImGui::MenuItem("Step Into", "F7")) {
				stepInto();
			}

			if (ImGui::BeginMenu("Step Width"))
			{
				if (ImGui::MenuItem("PCode Instruction", nullptr, m_stepWidth == StepWidth::STEP_PCODE_INSTR)) {
					m_stepWidth = StepWidth::STEP_PCODE_INSTR;
				}
				if (ImGui::MenuItem("Orig. Instruction", nullptr, m_stepWidth == StepWidth::STEP_ORIGINAL_INSTR)) {
					m_stepWidth = StepWidth::STEP_ORIGINAL_INSTR;
				}
				if (ImGui::MenuItem("Code Line", nullptr, m_stepWidth == StepWidth::STEP_CODE_LINE)) {
					m_stepWidth = StepWidth::STEP_CODE_LINE;
				}
				ImGui::EndMenu();
			}
		}
	private:
		void renderControl() override {
			if (ImGui::IsKeyPressed(VK_F8)) { // todo: linux, change imgui_impl_win32.cpp
				stepOver(true);
			}
			else if (ImGui::IsKeyPressed(VK_F7)) {
				stepInto();
			}
			
			Show(m_execCtxViewerWin);
		}

		void defineCurInstrutction() {
			const auto prevPCodeGraph = m_curPCodeBlock ? m_curPCodeBlock->m_funcPCodeGraph : nullptr;
			
			m_curInstr = m_imageDec->getInstrPool()->getPCodeInstructionAt(m_offset);
			m_curPCodeBlock = m_imageDec->getPCodeGraph()->getBlockAtOffset(m_offset);
			
			if (m_instrHandler.isInit()) {
				const auto isNewGraph = prevPCodeGraph != m_curPCodeBlock->m_funcPCodeGraph;
				m_instrHandler(isNewGraph);
			}
		}

		void defineNextInstrOffset(bool jmp) {
			if (jmp && m_execCtx.m_nextInstrOffset != 0) {
				m_offset = m_execCtx.m_nextInstrOffset;
				return;
			}
			
			const auto origInstr = m_curInstr->m_origInstruction;
			if (m_curInstr->m_orderId != origInstr->m_pcodeInstructions.size() - 1) {
				m_offset = CE::ComplexOffset(m_offset.getByteOffset(), m_offset.getOrderId() + 1);
			}
			else {
				m_offset = CE::ComplexOffset(origInstr->m_offset + origInstr->m_length, 0);
			}
		}

		void stepNextPCodeInstr(bool into) {
			m_vm.execute(m_curInstr);
			defineNextInstrOffset(into ? true : m_curInstr->m_id != CE::Decompiler::PCode::InstructionId::CALL);
			defineCurInstrutction();
			if (isNewOrigInstruction())
				sync();
		}

		void stepNextOrigInstr(bool into) {
			do {
				stepNextPCodeInstr(into);
			} while (!isNewOrigInstruction());
		}

		void stepNextBlockTopNode(bool into) {
			bool isNewBlockTopNode;
			do {
				const auto prevBlockTopNode = m_curBlockTopNode;
				stepNextPCodeInstr(into);
				isNewBlockTopNode = m_curBlockTopNode != prevBlockTopNode;
			} while (!isNewBlockTopNode);
		}
		
		void stepOver(bool into) {
			switch(m_stepWidth) {
			case StepWidth::STEP_PCODE_INSTR:
				stepNextPCodeInstr(into);
				break;
			case StepWidth::STEP_ORIGINAL_INSTR:
				stepNextOrigInstr(into);
				break;
			case StepWidth::STEP_CODE_LINE:
				if(m_curBlockTopNode)
					stepNextBlockTopNode(into);
				else stepNextOrigInstr(into);
				break;
			}
		}

		void stepInto() {
			stepOver(true);
		}

		void sync() {
			std::map<int, CE::Decompiler::DataValue> registers;
			registers = m_execCtx.getRegisters();
			m_execCtx.syncWith(registers);
		}
	};
};