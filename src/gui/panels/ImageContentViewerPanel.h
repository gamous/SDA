#pragma once
#include "DebugerPanel.h"
#include "ImageDecorator.h"
#include "imgui_wrapper/controls/AbstractPanel.h"
#include "imgui_wrapper/controls/List.h"
#include "imgui_wrapper/controls/Text.h"
#include "utilities/Helper.h"
#include "InstructionViewer.h"
#include "decompiler/Decompiler.h"
#include "decompiler/Graph/DecPCodeGraph.h"
#include "decompiler/Optimization/Graph/DecGraphDebugProcessing.h"
#include "decompiler/SDA/Optimizaton/SdaGraphMemoryOptimization.h"
#include "decompiler/SDA/Optimizaton/SdaGraphUselessLineOptimization.h"
#include "decompiler/SDA/Symbolization/DecGraphSdaBuilding.h"
#include "decompiler/SDA/Symbolization/SdaGraphDataTypeCalc.h"
#include "panels/FuncGraphViewerPanel.h"
#include "panels/DecompiledCodeViewerPanel.h"

namespace GUI
{
	class ImageSectionListModel : public IListModel<const CE::ImageSection*>
	{
		class Iterator : public IListModel<const CE::ImageSection*>::Iterator
		{
			 std::list<CE::ImageSection>::const_iterator m_it;
		protected:
			ImageSectionListModel* m_listModel;
		public:
			Iterator(ImageSectionListModel* listModel)
				: m_listModel(listModel), m_it(listModel->m_image->getImageSections().begin())
			{}

			void getNextItem(std::string* text, const CE::ImageSection** data) override
			{
				*text = m_it->m_name;
				*data = &(*m_it);
				++m_it;
			}

			bool hasNextItem() override
			{
				return m_it != m_listModel->m_image->getImageSections().end();
			}
		};

		CE::AbstractImage* m_image;
	public:
		ImageSectionListModel(CE::AbstractImage* image)
			: m_image(image)
		{}

		void newIterator(const IteratorCallback& callback) override
		{
			Iterator iterator(this);
			callback(&iterator);
		}
	};
	
	class ImageContentViewerPanel : public AbstractPanel
	{
		inline const static ImGuiTableFlags TableFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

		class GoToPanel : public AbstractPanel
		{
			ImageContentViewerPanel* m_imageContentViewerPanel;
			Input::TextInput m_offsetInput;
			std::string m_errorMessage;
		public:
			GoToPanel(ImageContentViewerPanel* imageContentViewerPanel)
				: AbstractPanel("Go to"), m_imageContentViewerPanel(imageContentViewerPanel)
			{}

		private:
			void renderPanel() override {
				if(!m_errorMessage.empty())
					Text::Text("Error: " + m_errorMessage).show();
				m_offsetInput.show();
				SameLine();
				if (Button::StdButton("Go").present()) {
					const auto offsetStr = m_offsetInput.getInputText();
					const auto offset = ParseOffset(offsetStr);
					try {
						m_imageContentViewerPanel->goToOffset(offset);
						m_window->close();
					} catch (WarningException& ex) {
						m_errorMessage = ex.what();
					}
				}
			}

			static CE::Offset ParseOffset(const std::string& offsetStr) {
				using namespace Helper::String;
				return offsetStr[1] == 'x' ? static_cast<int>(HexToNumber(offsetStr)) : std::stoi(offsetStr);
			}
		};
		
		class AbstractSectionViewer : public Control
		{
		protected:
			ImGuiListClipper m_clipper;
			int m_scrollToRowIdx = -1;
			bool m_selectCurRow = false;
			ColorRGBA m_selectedRowColor = 0;
			int m_startSelectionRowIdx = -1;
		public:
			AbstractSectionController* m_sectionController;
			
			AbstractSectionViewer(AbstractSectionController* controller)
				: m_sectionController(controller)
			{}

			void goToOffset(CE::Offset offset) {
				const auto rowIdx = getRowIdxByOffset(offset);
				if (rowIdx == -1)
					throw WarningException("Offset not found.");
				m_scrollToRowIdx = rowIdx;
			}

			bool isRowsMultiSelecting() const {
				return m_startSelectionRowIdx != -1;
			}
		protected:
			virtual int getRowIdxByOffset(CE::Offset offset) = 0;
			
			void scroll() {
				if (m_scrollToRowIdx != -1) {
					ImGui::SetScrollY(m_scrollToRowIdx * m_clipper.ItemsHeight);
					m_scrollToRowIdx = -1;
				}
			}

			void tableNextRow() {
				const auto Color = ToImGuiColorU32(m_selectedRowColor ? m_selectedRowColor : 0x1d333dFF);
				if (m_selectCurRow) {
					ImGui::PushStyleColor(ImGuiCol_TableRowBg, Color);
					ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, Color);
				}
				ImGui::TableNextRow();
				if (m_selectCurRow) {
					ImGui::PopStyleColor();
					ImGui::PopStyleColor();
					m_selectCurRow = false;
					m_selectedRowColor = 0;
				}
			}
		};

		class DataSectionViewer : public AbstractSectionViewer
		{
			DataSectionController* m_dataSectionController;
		public:
			DataSectionViewer(DataSectionController* dataSectionController)
				: AbstractSectionViewer(dataSectionController), m_dataSectionController(dataSectionController)
			{}

		private:
			void renderControl() override {
				ImGui::BeginChild("##empty", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove);
				if (ImGui::BeginTable("##empty", 3, TableFlags))
				{
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_None);
					ImGui::TableSetupColumn("Data type", ImGuiTableColumnFlags_None);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None);
					ImGui::TableHeadersRow();

					m_clipper.Begin(m_dataSectionController->getRowsCount());
					while (m_clipper.Step())
					{
						for (auto rowIdx = m_clipper.DisplayStart; rowIdx < m_clipper.DisplayEnd; rowIdx++) {
							tableNextRow();
							const auto offset = m_dataSectionController->getRow(rowIdx);
							ImGui::TableNextColumn();
							RenderAddress(offset);
							renderSpecificColumns(offset);
						}
					}

					scroll();
					ImGui::EndTable();
				}
				ImGui::EndChild();
			}

			void renderSpecificColumns(uint64_t offset) const {
				using namespace Helper::String;

				auto symbol = m_dataSectionController->getSymbol(offset);
				const auto pValue = reinterpret_cast<uint64_t*>(&m_dataSectionController->getImageData()[offset]);
				ImGui::TableNextColumn();
				Text::Text(symbol->getDataType()->getDisplayName()).show();
				ImGui::TableNextColumn();
				Text::Text("some value").show();
			}

			int getRowIdxByOffset(CE::Offset offset) override {
				return m_dataSectionController->getRowIdx(offset);
			}
		};
		
		class CodeSectionViewer : public AbstractSectionViewer
		{
			class FunctionReferencesPanel : public AbstractPanel
			{
				class FuncCallListModel : public IListModel<CE::Function*>
				{
					CE::Decompiler::FunctionPCodeGraph* m_funcPCodeGraph;
					CE::ImageDecorator* m_imageDec;
				public:
					FuncCallListModel(CE::Function* function)
						: m_funcPCodeGraph(function->getFuncGraph()), m_imageDec(function->getImage())
					{}

					bool empty() const {
						return m_funcPCodeGraph->getRefFuncCalls().empty();
					}
				private:
					class FuncCallIterator : public Iterator
					{
						std::set<CE::Decompiler::FunctionPCodeGraph*>::iterator m_it;
						const std::set<CE::Decompiler::FunctionPCodeGraph*>* m_list;
						CE::ImageDecorator* m_imageDec;
					public:
						FuncCallIterator(const std::set<CE::Decompiler::FunctionPCodeGraph*>* list, CE::ImageDecorator* imageDec)
							: m_list(list), m_it(list->begin()), m_imageDec(imageDec)
						{}

						void getNextItem(std::string* text, CE::Function** data) override {
							const auto funcGraph = *m_it;
							++m_it;
							if (const auto function = m_imageDec->getFunctionAt(funcGraph->getStartBlock()->getMinOffset().getByteOffset())) {
								*text = function->getName();
								*data = function;
							} else {
								if(hasNextItem())
									getNextItem(text, data);
							}
						}

						bool hasNextItem() override {
							return m_it != m_list->end();
						}
					};

					void newIterator(const IteratorCallback& callback) override {
						FuncCallIterator iterator(&m_funcPCodeGraph->getRefFuncCalls(), m_imageDec);
						callback(&iterator);
					}
				};
				
				CE::Function* m_function;
				CodeSectionViewer* m_codeSectionViewer;
				FuncCallListModel m_listModel;
				SelectableTableListView<CE::Function*>* m_listView;
				std::string m_errorMessage;
			public:
				FunctionReferencesPanel(CE::Function* function, CodeSectionViewer* codeSectionViewer)
					: AbstractPanel("Function References"), m_function(function), m_codeSectionViewer(codeSectionViewer), m_listModel(function)
				{
					m_listView = new SelectableTableListView(&m_listModel, {
						ColInfo("Function")
					});
					m_listView->handler([&](CE::Function* function)
						{
							try {
								m_codeSectionViewer->goToOffset(function->getOffset());
								m_window->close();
							}
							catch (WarningException& ex) {
								m_errorMessage = ex.what();
							}
						});
				}

			private:
				void renderPanel() override {
					if (!m_errorMessage.empty())
						Text::Text("Error: " + m_errorMessage).show();
					
					if (!m_listModel.empty()) {
						m_listView->show();
					} else {
						Text::Text("No functions referenced to.").show();
					}
				}
			};
			
			class CodeSectionInstructionViewer : public InstructionTableRowViewer
			{
				EventHandler<> m_renderJmpArrow;
			public:
				using InstructionTableRowViewer::InstructionTableRowViewer;
				
				void renderJmpArrow(const std::function<void()>& renderJmpArrow)
				{
					m_renderJmpArrow = renderJmpArrow;
				}
			
			protected:
				void renderMnemonic() override {
					InstructionTableRowViewer::renderMnemonic();
					m_renderJmpArrow();
				}
			};

			class RowContextPanel : public AbstractPanel
			{
				CodeSectionViewer* m_codeSectionViewer;
				CodeSectionRow m_codeSectionRow;
			public:
				RowContextPanel(CodeSectionViewer* codeSectionViewer, CodeSectionRow codeSectionRow)
					: m_codeSectionViewer(codeSectionViewer), m_codeSectionRow(codeSectionRow)
				{}

			private:
				void renderPanel() override {
					if (ImGui::MenuItem("Analyze")) {
						delete m_codeSectionViewer->m_popupModalWindow;
						const auto panel = new ImageAnalyzerPanel(m_codeSectionViewer->m_codeSectionController->m_imageDec, m_codeSectionRow.m_byteOffset);
						m_codeSectionViewer->m_popupModalWindow = new PopupModalWindow(panel);
						m_codeSectionViewer->m_popupModalWindow->open();
					}

					if (m_codeSectionViewer->m_debugger && !m_codeSectionViewer->m_debugger->m_isStopped) {
						if (ImGui::MenuItem("Move Debug Here")) {
							m_codeSectionViewer->m_debugger->goDebug(m_codeSectionRow.getOffset());
						}
					}
					else {
						if (ImGui::MenuItem("Start Debug Here")) {
							m_codeSectionViewer->m_startDebug = true;
						}
					}
				}
			};
			
			std::set<CodeSectionController::Jmp*> m_shownJmps;
			ImGuiWindow* m_window = nullptr;
			PopupBuiltinWindow* m_builtinWindow = nullptr;
			PopupModalWindow* m_popupModalWindow = nullptr;
			PopupContextWindow* m_ctxWindow = nullptr;
		public:
			ImageDebugger* m_debugger = nullptr;
			bool m_startDebug = false;
			CodeSectionController* m_codeSectionController;
			AbstractInstructionViewDecoder* m_instructionViewDecoder;
			CE::Decompiler::FunctionPCodeGraph* m_curFuncPCodeGraph = nullptr;
			bool m_obscureUnknownLocation = true;
			std::list<CodeSectionRow> m_selectedRows;
			CE::Decompiler::PCodeBlock* m_clickedPCodeBlock = nullptr;
			
			CodeSectionViewer(CodeSectionController* codeSectionController, AbstractInstructionViewDecoder* instructionViewDecoder)
				: AbstractSectionViewer(codeSectionController), m_codeSectionController(codeSectionController), m_instructionViewDecoder(instructionViewDecoder)
			{}

			~CodeSectionViewer() override {
				delete m_instructionViewDecoder;
				delete m_builtinWindow;
				delete m_popupModalWindow;
				delete m_ctxWindow;
			}

		private:
			void renderControl() override {
				m_shownJmps.clear();
				m_curFuncPCodeGraph = nullptr;
				m_clickedPCodeBlock = nullptr;
				m_startDebug = false;
				
				if(Button::StdButton("go to func").present()) {
					goToOffset(0x20c80);
				}

				ImGui::BeginChild("##empty", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove);
				if (ImGui::BeginTable("##empty", 3, TableFlags))
				{
					m_window = ImGui::GetCurrentWindow();
					const auto tableSize = ImGui::GetItemRectSize();
					ImGui::TableSetupScrollFreeze(0, 1);
					ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_None);
					ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_None);
					ImGui::TableSetupColumn("Operands", ImGuiTableColumnFlags_None);
					ImGui::TableHeadersRow();

					m_clipper.Begin(m_codeSectionController->getRowsCount());
					while (m_clipper.Step())
					{
						for (auto rowIdx = m_clipper.DisplayStart; rowIdx < m_clipper.DisplayEnd; rowIdx++) {
							bool obscure = false;
							tableNextRow();

							// getting info about the current row
							const auto codeSectionRow = m_codeSectionController->getRow(rowIdx);
							const auto pcodeBlock = m_codeSectionController->m_imageDec->getPCodeGraph()->getBlockAtOffset(codeSectionRow.m_fullOffset);
							if (pcodeBlock) {
								if (rowIdx == (m_clipper.DisplayStart + m_clipper.DisplayEnd) / 2)
									m_curFuncPCodeGraph = pcodeBlock->m_funcPCodeGraph;
							} else {
								if (m_obscureUnknownLocation)
									obscure = true;
							}

							// select rows
							if (m_debugger && !m_debugger->m_isStopped) {
								if (const auto instr = m_debugger->m_curInstr) {
									bool isSelected;
									if (m_codeSectionController->m_showPCode)
										isSelected = codeSectionRow.m_isPCode && instr->getOffset() == codeSectionRow.getOffset();
									else isSelected = instr->getOffset().getByteOffset() == codeSectionRow.getOffset().getByteOffset();
									if (isSelected) {
										m_selectCurRow = true;
										m_selectedRowColor = 0x420001FF;
									}
								}
							}
							for(auto selRow : m_selectedRows) {
								if(m_codeSectionController->m_showPCode ? selRow == codeSectionRow : selRow.m_byteOffset == codeSectionRow.m_byteOffset) {
									m_selectCurRow = true;
									break;
								}
							}

							if(obscure)
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7f);

							ImVec2 startRowPos;
							if (!codeSectionRow.m_isPCode) {
								const auto instrOffset = codeSectionRow.m_byteOffset;
								ImGui::TableNextColumn();
								startRowPos = ImGui::GetCursorScreenPos();
								RenderAddress(instrOffset);

								// Asm
								InstructionViewInfo instrViewInfo;
								m_instructionViewDecoder->decode(m_codeSectionController->getImageDataByOffset(instrOffset), &instrViewInfo);
								CodeSectionInstructionViewer instructionViewer(&instrViewInfo);
								instructionViewer.renderJmpArrow([&]()
									{
										renderJmpLines(rowIdx, m_clipper);
									});
								instructionViewer.show();
							} else {
								// PCode
								const auto instrOffset = codeSectionRow.m_fullOffset;
								if (const auto instr = m_codeSectionController->m_imageDec->getInstrPool()->getPCodeInstructionAt(instrOffset)) {
									ImGui::TableNextColumn();
									startRowPos = ImGui::GetCursorScreenPos();
									Text::Text("").show();
									ImGui::TableNextColumn();
									Text::Text("").show();
									ImGui::TableNextColumn();
									PCodeInstructionRender instrRender;
									instrRender.generate(instr);
								}
							}

							// decorate the current row
							const auto rowSize = ImVec2(tableSize.x - 25, m_clipper.ItemsHeight);
							decorateRow(startRowPos, rowSize, codeSectionRow, rowIdx, pcodeBlock);

							if (obscure)
								ImGui::PopStyleVar();
						}					
					}

					scroll();
					ImGui::EndTable();
				}
				ImGui::EndChild();

				Show(m_builtinWindow);
				Show(m_popupModalWindow);
				Show(m_ctxWindow);
			}

			void decorateRow(const ImVec2& startRowPos, const ImVec2& rowSize, CodeSectionRow row, int rowIdx, CE::Decompiler::PCodeBlock* pcodeBlock) {
				// function header
				bool isEventProcessed = false;
				if (!row.m_isPCode) {
					if (const auto function = m_codeSectionController->m_imageDec->getFunctionAt(row.m_byteOffset)) {
						renderFunctionHeader(function, startRowPos, startRowPos + ImVec2(rowSize.x, 0), isEventProcessed);
					}
				}

				// process events
				if (!isEventProcessed && ImGui::IsWindowHovered()) {
					ImGuiContext& g = *GImGui;
					if (ImGui::IsMousePosValid(&g.IO.MousePos)) {
						if (ImRect(startRowPos, startRowPos + rowSize).Contains(g.IO.MousePos)) {
							if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
								// row's multi-selection
								if(m_startSelectionRowIdx != -1) {
									auto curRowIdx = m_startSelectionRowIdx;
									auto lastRowIdx = rowIdx;
									if (curRowIdx > lastRowIdx)
										std::swap(curRowIdx, lastRowIdx);
									m_selectedRows.clear();
									while(curRowIdx <= lastRowIdx)
										m_selectedRows.push_back(m_codeSectionController->getRow(curRowIdx++));
								}
								else {
									m_startSelectionRowIdx = rowIdx;
								}
							}
							else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
								m_startSelectionRowIdx = -1;
							}
							else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
								m_selectedRows = { row };
								m_clickedPCodeBlock = pcodeBlock;
							}
							else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
								if(m_selectedRows.size() == 1)
									m_selectedRows = { row };
								delete m_ctxWindow;
								m_ctxWindow = new PopupContextWindow(new RowContextPanel(this, row));
								m_ctxWindow->open();
							}
						}
					}
				}
			}

			void renderFunctionHeader(CE::Function* function, const ImVec2& startRowPos, const ImVec2& endRowPos, bool& isEventProcessed) {
				const auto Color = ToImGuiColorU32(0xc9c59fFF);
				m_window->DrawList->AddLine(startRowPos, endRowPos, Color, 0.7f);
				
				// function name
				{
					const auto text = function->getName();
					const auto textSize = ImGui::CalcTextSize(text);
					const auto textPos = endRowPos - ImVec2(textSize.x, 0.0);

					// click event
					ImGuiContext& g = *GImGui;
					if (ImGui::IsMousePosValid(&g.IO.MousePos)) {
						if (ImRect(textPos, textPos + textSize).Contains(g.IO.MousePos)) {
							ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
							if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
								delete m_builtinWindow;
								m_builtinWindow = new PopupBuiltinWindow(new FunctionReferencesPanel(function, this));
								m_builtinWindow->getSize() = ImVec2(300.0f, 200.0f);
								m_builtinWindow->getPos() = endRowPos - ImVec2(m_builtinWindow->getSize().x, 0);
								m_builtinWindow->addFlags(ImGuiWindowFlags_AlwaysAutoResize, false);
								m_builtinWindow->open();
								isEventProcessed = true;
							}
						}
					}

					// render
					m_window->DrawList->AddText(textPos, Color, text);
				}
			}

			void renderJmpLines(const int rowIdx, const ImGuiListClipper& clipper) {
				if (const auto it = m_codeSectionController->m_offsetToJmp.find(m_codeSectionController->getRow(rowIdx).m_fullOffset);
					it != m_codeSectionController->m_offsetToJmp.end()) {
					auto pJmps = it->second;
					for (auto pJmp : pJmps) {
						if (m_shownJmps.find(pJmp) == m_shownJmps.end()) {
							drawJmpLine(rowIdx, pJmp, clipper);
							m_shownJmps.insert(pJmp);
						}
					}
				}
			}

			void drawJmpLine(const int rowIdx, const CodeSectionControllerX86::Jmp* pJmp, const ImGuiListClipper& clipper) const {
				const float JmpLineLeftOffset = 10.0f;
				const float JmpLineGap = 8.0f;
				
				const bool isStart = m_codeSectionController->getRow(rowIdx).m_fullOffset == pJmp->m_startOffset;
				const auto targetRowIdx = m_codeSectionController->getRowIdx(CodeSectionRow(isStart ? pJmp->m_endOffset : pJmp->m_startOffset));

				auto lineColor = ToImGuiColorU32(-1);
				const ImVec2 point1 = { ImGui::GetItemRectMin().x - 2.0f, (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) / 2.0f };
				const ImVec2 point2 = ImVec2(point1.x - pJmp->m_level * JmpLineGap - JmpLineLeftOffset, point1.y);
				const ImVec2 point3 = ImVec2(point2.x, point2.y + clipper.ItemsHeight * (targetRowIdx - rowIdx));
				const ImVec2 point4 = ImVec2(point1.x, point3.y);

				// click event of the jump arrow
				ImGuiContext& g = *GImGui;
				if (ImGui::IsMousePosValid(&g.IO.MousePos)) {
					auto rect = ImRect(point2, { point3.x + JmpLineGap, point3.y });
					if(rect.Min.x > rect.Max.x || rect.Min.y > rect.Max.y)
						rect = ImRect(point3, { point2.x + JmpLineGap, point2.y });
					if (rect.Contains(g.IO.MousePos)) {
						ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
						lineColor = ToImGuiColorU32(0x00bfffFF);
						if(ImGui::IsMouseClicked(0)) {
							const auto offsetToCenter = clipper.ItemsHeight * (clipper.DisplayEnd - clipper.DisplayStart) / 2.0f;
							ImGui::SetScrollY(targetRowIdx * clipper.ItemsHeight - offsetToCenter);
						}
					}
				}


				// render the jump arrow
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				window->DrawList->AddLine(point1, point2, lineColor);
				window->DrawList->AddLine(point2, point3, lineColor);
				window->DrawList->AddLine(point3, point4, lineColor);
				auto arrowPos = isStart ? point4 : point1;
				arrowPos -= ImVec2(7.0f, 3.0f);
				ImGui::RenderArrow(window->DrawList, arrowPos, lineColor, ImGuiDir_Right, 0.7f);
			}

			int getRowIdxByOffset(CE::Offset offset) override {
				return m_codeSectionController->getRowIdx(CodeSectionRow(CE::ComplexOffset(offset, 0)));
			}
		};

		enum class DecompilerStep
		{
			DECOMPILING,
			PROCESSING,
			SYMBOLIZING,
			FINAL_PROCESSING,
			//DEFAULT = FINAL_PROCESSING
			DEFAULT = PROCESSING
		};

		enum class ProcessingStep
		{
			CONDITION_PROCESSING		= 1 << 0,
			EXPR_OPTIMIZING				= 1 << 1,
			PAR_ASSIGNMENT_CREATING		= 1 << 2,
			ORDER_FIXING				= 1 << 3,
			VIEW_OPTIMIZING				= 1 << 4,
			LINE_EXPANDING				= 1 << 5,
			USELESS_LINES_REMOVING		= 1 << 6,
			DEBUG_PROCESSING			= 1 << 7,
			DEFAULT = -1 & ~USELESS_LINES_REMOVING
		};

		enum class SymbolizingStep
		{
			BUILDING,
			CALCULATING,
			DEFAULT = CALCULATING
		};

		enum class FinalProcessingStep
		{
			MEMORY_OPTIMIZING			= 1 << 0,
			USELESS_LINES_OPTIMIZING	= 1 << 1,
			DEFAULT = MEMORY_OPTIMIZING | USELESS_LINES_OPTIMIZING
		};
		
		AbstractSectionViewer* m_imageSectionViewer = nullptr;
		std::map<const CE::ImageSection*, AbstractSectionController*> m_imageSectionControllers; // todo: move out of the scope
		ImageSectionListModel m_imageSectionListModel;
		MenuListView<const CE::ImageSection*> m_imageSectionMenuListView;
		ImageDebugger* m_debugger = nullptr;
		StdWindow* m_funcGraphViewerWindow = nullptr;
		StdWindow* m_decompiledCodeViewerWindow = nullptr;
		PopupModalWindow* m_popupModalWindow = nullptr;

		CE::Decompiler::DecompiledCodeGraph* m_curDecGraph = nullptr;

		DecompilerStep m_decompilerStep = DecompilerStep::DEFAULT;
		ProcessingStep m_processingStep = ProcessingStep::DEFAULT;
		SymbolizingStep m_symbolizingStep = SymbolizingStep::DEFAULT;
		FinalProcessingStep m_finalProcessingStep = FinalProcessingStep::DEFAULT;
	public:
		CE::ImageDecorator* m_imageDec;
		
		ImageContentViewerPanel(CE::ImageDecorator* imageDec)
			: AbstractPanel(std::string("Image: ") + imageDec->getName() + "###ImageContentViewer"), m_imageDec(imageDec), m_imageSectionListModel(imageDec->getImage())
		{
			m_imageSectionMenuListView = MenuListView(&m_imageSectionListModel);
			m_imageSectionMenuListView.handler([&](const CE::ImageSection* imageSection)
				{
					selectImageSection(imageSection);
				});

			// select the code segment by default
			for(const auto& section : imageDec->getImage()->getImageSections()) {
				if(section.m_type == CE::ImageSection::CODE_SEGMENT) {
					selectImageSection(&section);
					break;
				}
			}
		}

		~ImageContentViewerPanel() override {
			delete m_imageSectionViewer;
			for (const auto& pair : m_imageSectionControllers)
				delete pair.second;
			delete m_funcGraphViewerWindow;
			delete m_decompiledCodeViewerWindow;
			delete m_debugger;
			delete m_popupModalWindow;
		}

		StdWindow* createStdWindow() {
			return new StdWindow(this, ImGuiWindowFlags_MenuBar);
		}

	private:
		void renderPanel() override {
			m_imageSectionViewer->show();

			processSectionViewerEvents();
			processDecompiledCodeViewerEvents();
			processDebuggerEvents();

			if (m_debugger) {
				m_debugger->show();
			}
			Show(m_funcGraphViewerWindow);
			Show(m_decompiledCodeViewerWindow);
			Show(m_popupModalWindow);
		}

		void renderMenuBar() override {
			auto codeSectionViewer = dynamic_cast<CodeSectionViewer*>(m_imageSectionViewer);
			
			if (ImGui::BeginMenu("Select"))
			{
				m_imageSectionMenuListView.show();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Navigation"))
			{
				if (ImGui::MenuItem("Go To")) {
					delete m_popupModalWindow;
					m_popupModalWindow = new PopupModalWindow(new GoToPanel(this));
					m_popupModalWindow->open();
				}
				ImGui::EndMenu();
			}

			if (codeSectionViewer) {
				if (ImGui::BeginMenu("Debug"))
				{
					if (codeSectionViewer->m_curFuncPCodeGraph) {
						if (ImGui::MenuItem("Start Debug")) {
							createDebugger(codeSectionViewer->m_curFuncPCodeGraph->getStartBlock()->getMinOffset(), ImageDebugger::StepWidth::STEP_ORIGINAL_INSTR);
						}
					}
					if (m_debugger) {
						m_debugger->renderDebugMenu();
					}
					ImGui::EndMenu();
				}
				
				if (codeSectionViewer->m_curFuncPCodeGraph) {
					if (ImGui::BeginMenu("Decompiler"))
					{
						if (ImGui::MenuItem("Decompile")) {
							decompile(codeSectionViewer->m_curFuncPCodeGraph);
						}

						ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);
						if (ImGui::MenuItem("Decompiling Step", nullptr, m_decompilerStep >= DecompilerStep::DECOMPILING)) {
							m_decompilerStep = DecompilerStep::DECOMPILING;
						}
						if (ImGui::MenuItem("Processing Step", nullptr, m_decompilerStep >= DecompilerStep::PROCESSING)) {
							m_decompilerStep = DecompilerStep::PROCESSING;
						}
						if (ImGui::MenuItem("Symbolizing Step", nullptr, m_decompilerStep >= DecompilerStep::SYMBOLIZING)) {
							m_decompilerStep = DecompilerStep::SYMBOLIZING;
						}
						if (ImGui::MenuItem("Final Processing Step", nullptr, m_decompilerStep >= DecompilerStep::FINAL_PROCESSING)) {
							m_decompilerStep = DecompilerStep::FINAL_PROCESSING;
						}

						if (ImGui::BeginMenu("Processing Steps"))
						{
							using namespace magic_enum::bitwise_operators;
							if (ImGui::MenuItem("Condition Processing Step", nullptr, (m_processingStep | ProcessingStep::CONDITION_PROCESSING) == m_processingStep)) {
								m_processingStep ^= ProcessingStep::CONDITION_PROCESSING;
							}
							if (ImGui::MenuItem("Expr. Optimizing Step", nullptr, (m_processingStep | ProcessingStep::EXPR_OPTIMIZING) == m_processingStep)) {
								m_processingStep ^= ProcessingStep::EXPR_OPTIMIZING;
							}
							if (ImGui::MenuItem("Par. Assignment Creating Step", nullptr, (m_processingStep | ProcessingStep::PAR_ASSIGNMENT_CREATING) == m_processingStep)) {
								m_processingStep ^= ProcessingStep::PAR_ASSIGNMENT_CREATING;
							}
							if (ImGui::MenuItem("Order Fixing Step", nullptr, (m_processingStep | ProcessingStep::ORDER_FIXING) == m_processingStep)) {
								m_processingStep ^= ProcessingStep::ORDER_FIXING;
							}
							if (ImGui::MenuItem("View Optimizing Step", nullptr, (m_processingStep | ProcessingStep::VIEW_OPTIMIZING) == m_processingStep)) {
								m_processingStep ^= ProcessingStep::VIEW_OPTIMIZING;
							}
							if (ImGui::MenuItem("Line Expanding Step", nullptr, (m_processingStep | ProcessingStep::LINE_EXPANDING) == m_processingStep)) {
								m_processingStep ^= ProcessingStep::LINE_EXPANDING;
							}
							if (ImGui::MenuItem("Useless Lines Removing Step", nullptr, (m_processingStep | ProcessingStep::USELESS_LINES_REMOVING) == m_processingStep)) {
								m_processingStep ^= ProcessingStep::USELESS_LINES_REMOVING;
							}
							if (ImGui::MenuItem("Debug Processing Step", nullptr, (m_processingStep | ProcessingStep::DEBUG_PROCESSING) == m_processingStep)) {
								m_processingStep ^= ProcessingStep::DEBUG_PROCESSING;
							}
							ImGui::EndMenu();
						}
						
						if (ImGui::BeginMenu("Symbolizing Steps"))
						{
							if (ImGui::MenuItem("Building step", nullptr, m_symbolizingStep >= SymbolizingStep::BUILDING)) {
								m_symbolizingStep = SymbolizingStep::BUILDING;
							}
							if (ImGui::MenuItem("Calculating step", nullptr, m_symbolizingStep >= SymbolizingStep::CALCULATING)) {
								m_symbolizingStep = SymbolizingStep::CALCULATING;
							}
							ImGui::EndMenu();
						}

						if (ImGui::BeginMenu("Final Processing Steps"))
						{
							using namespace magic_enum::bitwise_operators;
							if (ImGui::MenuItem("Memory Optimizing Step", nullptr, (m_finalProcessingStep | FinalProcessingStep::MEMORY_OPTIMIZING) == m_finalProcessingStep)) {
								m_finalProcessingStep ^= FinalProcessingStep::MEMORY_OPTIMIZING;
							}
							if (ImGui::MenuItem("Useless Lines Optimizing Step", nullptr, (m_finalProcessingStep | FinalProcessingStep::USELESS_LINES_OPTIMIZING) == m_finalProcessingStep)) {
								m_finalProcessingStep ^= FinalProcessingStep::USELESS_LINES_OPTIMIZING;
							}
							ImGui::EndMenu();
						}
						ImGui::PopItemFlag();
						ImGui::EndMenu();
					}
				}
			}

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Update")) {
					m_imageSectionViewer->m_sectionController->update();
				}
				if (codeSectionViewer) {
					if (ImGui::MenuItem("Show PCode", nullptr, codeSectionViewer->m_codeSectionController->m_showPCode)) {
						codeSectionViewer->m_codeSectionController->m_showPCode ^= true;
						codeSectionViewer->m_codeSectionController->update();
					}
					if (codeSectionViewer->m_curFuncPCodeGraph) {
						if (ImGui::MenuItem("Show function graph", nullptr)) {
							if (!m_funcGraphViewerWindow) {
								auto funcGraphViewerPanel = new FuncGraphViewerPanel(m_imageDec, new InstructionViewDecoderX86);
								funcGraphViewerPanel->setFuncGraph(codeSectionViewer->m_curFuncPCodeGraph);
								m_funcGraphViewerWindow = funcGraphViewerPanel->createStdWindow();
							}
							else {
								if (auto funcGraphViewerPanel = dynamic_cast<FuncGraphViewerPanel*>(m_funcGraphViewerWindow->getPanel())) {
									funcGraphViewerPanel->setFuncGraph(codeSectionViewer->m_curFuncPCodeGraph);
								}
							}
						}
					}
					if (ImGui::MenuItem("Obscure unknown location", nullptr, codeSectionViewer->m_obscureUnknownLocation)) {
						codeSectionViewer->m_obscureUnknownLocation ^= true;
					}
				}
				ImGui::EndMenu();
			}
		}

		void selectImageSection(const CE::ImageSection* imageSection) {
			delete m_imageSectionViewer;
			m_imageSectionMenuListView.m_selectedItem = imageSection;

			AbstractSectionController* controller = nullptr;
			if (const auto it = m_imageSectionControllers.find(imageSection);
				it != m_imageSectionControllers.end())
				controller = it->second;

			if (imageSection->m_type == CE::ImageSection::CODE_SEGMENT) {
				auto codeController = dynamic_cast<CodeSectionControllerX86*>(controller);
				if (!codeController)
					m_imageSectionControllers[imageSection] = codeController = new CodeSectionControllerX86(m_imageDec, imageSection);
				m_imageSectionViewer = new CodeSectionViewer(codeController, new InstructionViewDecoderX86);
			}
			else {
				auto dataController = dynamic_cast<DataSectionController*>(controller);
				if (!dataController)
					m_imageSectionControllers[imageSection] = dataController = new DataSectionController(m_imageDec, imageSection);
				m_imageSectionViewer = new DataSectionViewer(dataController);
			}
		}

		void goToOffset(CE::Offset offset) {
			const auto section = m_imageDec->getImage()->getSectionByOffset(offset);
			if (section->m_type == CE::ImageSection::NONE_SEGMENT)
				throw WarningException("Offset not found.");
			selectImageSection(section);
			m_imageSectionViewer->goToOffset(offset);
		}

		void decompile(CE::Decompiler::FunctionPCodeGraph* functionPCodeGraph, bool updateDebug = true) {
			using namespace CE::Decompiler;

			RegisterFactoryX86 registerFactoryX86;
			const auto funcOffset = functionPCodeGraph->getStartBlock()->getMinOffset().getByteOffset();
			if(const auto function = m_imageDec->getFunctionAt(funcOffset)) {
				const auto project = m_imageDec->getImageManager()->getProject();

				if (m_decompilerStep >= DecompilerStep::DECOMPILING)
				{
					auto funcCallInfoCallback = [&](Instruction* instr, int offset)
					{
						if(const auto func = m_imageDec->getFunctionAt(offset)) {
							return func->getSignature()->getCallInfo();
						}
						const auto it = m_imageDec->getVirtFuncCalls().find(instr->getOffset());
						if(it != m_imageDec->getVirtFuncCalls().end())
							return it->second->getCallInfo();
						return project->getTypeManager()->getDefaultFuncSignature()->getCallInfo();
					};
					SdaCodeGraph* sdaCodeGraph = nullptr;
					auto decCodeGraph = new DecompiledCodeGraph(functionPCodeGraph);
					auto primaryDecompiler = new PrimaryDecompiler(decCodeGraph, &registerFactoryX86,
						function->getSignature()->getCallInfo().getReturnInfo(), funcCallInfoCallback);
					primaryDecompiler->start();

					if (m_decompilerStep >= DecompilerStep::PROCESSING)
					{
						using namespace magic_enum::bitwise_operators;
						{
							decCodeGraph->cloneAllExpr();

							if ((m_processingStep | ProcessingStep::CONDITION_PROCESSING) == m_processingStep) {
								Optimization::GraphCondBlockOptimization graphCondBlockOptimization(decCodeGraph);
								graphCondBlockOptimization.start();
							}

							if ((m_processingStep | ProcessingStep::EXPR_OPTIMIZING) == m_processingStep) {
								Optimization::GraphExprOptimization graphExprOptimization(decCodeGraph);
								graphExprOptimization.start();
							}

							if ((m_processingStep | ProcessingStep::PAR_ASSIGNMENT_CREATING) == m_processingStep) {
								Optimization::GraphParAssignmentCreator graphParAssignmentCreator(decCodeGraph, primaryDecompiler);
								graphParAssignmentCreator.start();
							}

							if ((m_processingStep | ProcessingStep::ORDER_FIXING) == m_processingStep) {
								Optimization::GraphLastLineAndConditionOrderFixing graphLastLineAndConditionOrderFixing(decCodeGraph);
								graphLastLineAndConditionOrderFixing.start();
							}

							if ((m_processingStep | ProcessingStep::VIEW_OPTIMIZING) == m_processingStep) {
								Optimization::GraphViewOptimization graphViewOptimization(decCodeGraph);
								graphViewOptimization.start();
							}

							if ((m_processingStep | ProcessingStep::DEBUG_PROCESSING) == m_processingStep) {
								Optimization::GraphDebugProcessing graphDebugProcessing(decCodeGraph, false);
								graphDebugProcessing.start();
							}

							if ((m_processingStep | ProcessingStep::LINE_EXPANDING) == m_processingStep) {
								Optimization::GraphLinesExpanding graphLinesExpanding(decCodeGraph);
								graphLinesExpanding.start();
							}

							if ((m_processingStep | ProcessingStep::DEBUG_PROCESSING) == m_processingStep) {
								Optimization::GraphDebugProcessing graphDebugProcessing(decCodeGraph, true);
								graphDebugProcessing.start();
							}

							if ((m_processingStep | ProcessingStep::USELESS_LINES_REMOVING) == m_processingStep) {
								Optimization::GraphUselessLineDeleting graphUselessLineDeleting(decCodeGraph);
								graphUselessLineDeleting.start();
							}
							
							DecompiledCodeGraph::CalculateHeightForDecBlocks(decCodeGraph->getStartBlock());
						}

						if (m_decompilerStep >= DecompilerStep::SYMBOLIZING)
						{
							if (m_symbolizingStep >= SymbolizingStep::BUILDING) {
								sdaCodeGraph = new SdaCodeGraph(decCodeGraph);
								auto symbolCtx = function->getSymbolContext();
								Symbolization::SdaBuilding sdaBuilding(sdaCodeGraph, &symbolCtx, project);
								sdaBuilding.start();

								if (m_symbolizingStep >= SymbolizingStep::CALCULATING) {
									Symbolization::SdaDataTypesCalculater sdaDataTypesCalculater(sdaCodeGraph, symbolCtx.m_signature, project);
									sdaDataTypesCalculater.start();
								}

								if (m_decompilerStep >= DecompilerStep::FINAL_PROCESSING)
								{
									if ((m_finalProcessingStep | FinalProcessingStep::MEMORY_OPTIMIZING) == m_finalProcessingStep) {
										Optimization::SdaGraphMemoryOptimization memoryOptimization(sdaCodeGraph);
										memoryOptimization.start();
									}

									if ((m_finalProcessingStep | FinalProcessingStep::USELESS_LINES_OPTIMIZING) == m_finalProcessingStep) {
										Optimization::SdaGraphUselessLineOptimization uselessLineOptimization(sdaCodeGraph);
										uselessLineOptimization.start();
									}
								}
							}
						}
					}

					// open the window
					DecompiledCodeViewerPanel* panel;
					if (sdaCodeGraph) {
						panel = new DecompiledCodeViewerPanel(sdaCodeGraph, function);
					} else {
						panel = new DecompiledCodeViewerPanel(decCodeGraph);
					}
					m_curDecGraph = decCodeGraph;
					panel->m_decompiledCodeViewer->setInfoToShowAsm(m_imageDec->getImage(), new InstructionViewDecoderX86);
					panel->m_decompiledCodeViewer->setInfoToShowExecCtxs(primaryDecompiler);
					if (m_decompiledCodeViewerWindow) {
						if (auto prevPanel = dynamic_cast<DecompiledCodeViewerPanel*>(m_decompiledCodeViewerWindow->getPanel()))
							panel->m_decompiledCodeViewer->m_show = prevPanel->m_decompiledCodeViewer->m_show;
					}
					delete m_decompiledCodeViewerWindow;
					m_decompiledCodeViewerWindow = panel->createStdWindow();

					if(updateDebug) {
						if (m_debugger && !m_debugger->m_isStopped) {
							instrHandler(false);
						}
					}
				}
			}
		}

		// e.g. if rows were selected
		void processSectionViewerEvents() {
			if (const auto codeSectionViewer = dynamic_cast<CodeSectionViewer*>(m_imageSectionViewer))
			{
				const auto decCodeViewerPanel = m_decompiledCodeViewerWindow ? dynamic_cast<DecompiledCodeViewerPanel*>(m_decompiledCodeViewerWindow->getPanel()) : nullptr;
				if (decCodeViewerPanel) {
					auto& instrs = decCodeViewerPanel->m_decompiledCodeViewer->m_selectedCodeByInstr;
					
					// select block list by instruction selection
					if (codeSectionViewer->m_clickedPCodeBlock && m_curDecGraph) {
						CE::Decompiler::DecBlock* selDecBlock = nullptr;
						for (const auto decBlock : m_curDecGraph->getDecompiledBlocks()) {
							if (decBlock->m_pcodeBlock == codeSectionViewer->m_clickedPCodeBlock) {
								selDecBlock = decBlock;
								break;
							}
						}
						if (selDecBlock) {
							decCodeViewerPanel->m_decompiledCodeViewer->selectBlockList(selDecBlock);
						}
						instrs.clear();
					}
					// select dec. code by row's multi-selection
					if (codeSectionViewer->isRowsMultiSelecting()) {
						instrs.clear();
						for (const auto row : codeSectionViewer->m_selectedRows) {
							if (row.m_isPCode) {
								if (const auto instr = m_imageDec->getInstrPool()->getPCodeInstructionAt(row.getOffset())) {
									instrs.insert(instr);
								}
							} else {
								if (const auto origInstr = m_imageDec->getInstrPool()->getOrigInstructionAt(row.m_byteOffset)) {
									for(auto& [orderId, instr] : origInstr->m_pcodeInstructions)
										instrs.insert(&instr);
								}
							}
						}
					}
				}

				// start debug
				if (codeSectionViewer->m_startDebug) {
					const auto codeSectionRow = *codeSectionViewer->m_selectedRows.begin();
					const auto stepWidth = codeSectionRow.m_isPCode ? ImageDebugger::StepWidth::STEP_PCODE_INSTR : ImageDebugger::StepWidth::STEP_ORIGINAL_INSTR;
					createDebugger(codeSectionRow.getOffset(), stepWidth);
				}
			}
			else if (const auto dataSectionViewer = dynamic_cast<DataSectionViewer*>(m_imageSectionViewer))
			{

			}
		}
		
		// e.g. if a symbol was renamed
		void processDecompiledCodeViewerEvents() {
			if (!m_decompiledCodeViewerWindow)
				return;
			
			if (const auto decCodeViewerPanel = dynamic_cast<DecompiledCodeViewerPanel*>(m_decompiledCodeViewerWindow->getPanel())) {
				if (const auto codeSectionViewer = dynamic_cast<CodeSectionViewer*>(m_imageSectionViewer)) {
					// symbol was changed
					if (decCodeViewerPanel->m_decompiledCodeViewer->m_codeChanged) {
						decompile(codeSectionViewer->m_curFuncPCodeGraph);
					}
					// select instructions by code selection
					if (!decCodeViewerPanel->m_decompiledCodeViewer->m_selectedInstrs.empty()) {
						codeSectionViewer->m_selectedRows.clear();
						for(const auto instr : decCodeViewerPanel->m_decompiledCodeViewer->m_selectedInstrs) {
							codeSectionViewer->m_selectedRows.emplace_back(instr->getOffset(), true);
						}
					}
				}

				// click function
				if (const auto function = decCodeViewerPanel->m_decompiledCodeViewer->m_clickedFunction) {
					goToOffset(function->getOffset());
					decCodeViewerPanel->m_decompiledCodeViewer->m_codeChanged = true;
				}
				// click global var
				else if (const auto globalVar = decCodeViewerPanel->m_decompiledCodeViewer->m_clickedGlobalVar) {
					goToOffset(globalVar->getOffset());
				}
				// start debug
				else if(decCodeViewerPanel->m_startDebug) {
					createDebugger(m_curDecGraph->getStartBlock()->m_pcodeBlock->getMinOffset(), ImageDebugger::StepWidth::STEP_CODE_LINE);
				}
			}
		}

		void processDebuggerEvents() const {
			if (!m_debugger || m_debugger->m_isStopped)
				return;

			if(m_decompiledCodeViewerWindow) {
				if(const auto decCodeViewerPanel = dynamic_cast<DecompiledCodeViewerPanel*>(m_decompiledCodeViewerWindow->getPanel())) {
					decCodeViewerPanel->m_decompiledCodeViewer->m_debugger = m_debugger;
				}
			}
			if(const auto codeSectionViewer = dynamic_cast<CodeSectionViewer*>(m_imageSectionViewer)) {
				codeSectionViewer->m_debugger = m_debugger;
			}
		}

		void createDebugger(CE::ComplexOffset startOffset, ImageDebugger::StepWidth stepWidth) {
			delete m_debugger;
			m_debugger = new ImageDebugger(m_imageDec, startOffset);
			m_debugger->m_stepWidth = stepWidth;
			m_debugger->instrHandler([&](bool isNewGraph)
				{
					instrHandler(isNewGraph);
				});
			m_debugger->defineCurInstrutction();
		}

		void instrHandler(bool isNewGraph) {
			if (isNewGraph) {
				if (const auto codeSectionViewer = dynamic_cast<CodeSectionViewer*>(m_imageSectionViewer))
					codeSectionViewer->goToOffset(m_debugger->m_curInstr->getOffset().getByteOffset());
				if (m_debugger->m_stepWidth == ImageDebugger::StepWidth::STEP_CODE_LINE) {
					if (!m_curDecGraph || m_debugger->m_curPCodeBlock->m_funcPCodeGraph != m_curDecGraph->getFuncGraph()) {
						decompile(m_debugger->m_curPCodeBlock->m_funcPCodeGraph, false);
					}
				} else {
					m_debugger->m_curBlockTopNode = nullptr;
				}
			}

			if (m_curDecGraph) {
				const auto instrOffset = m_debugger->m_curInstr->getOffset();
				m_debugger->m_curBlockTopNode = m_curDecGraph->findBlockTopNodeAtOffset(instrOffset);
				m_debugger->m_stackPointerValue = m_curDecGraph->getStackPointerValueAtOffset(instrOffset);
			}
		}
	};
};