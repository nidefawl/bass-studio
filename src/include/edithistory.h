#pragma once
#include <vector>
class MainCtrl;
class action_base {
public:
	std::string desc;
	virtual ~action_base(){ };
	virtual void undo(MainCtrl* ctrl) = 0;
	virtual void redo(MainCtrl* ctrl) = 0;
	String getDesc() {
		return desc;
	}
};
class edithistory {
	std::vector<action_base*> m_undo;
	std::vector<action_base*> m_redo;
public:
	void clear() {
		while (!m_redo.empty()) {
			action_base* redoAction = m_redo.back();
			m_redo.pop_back();
			delete redoAction;
		}
		while (!m_undo.empty()) {
			action_base* undoAction = m_undo.back();
			m_undo.pop_back();
			delete undoAction;
		}
	}
	void undoStep(MainCtrl* ctrl) {
		action_base* step = m_undo.back(); m_undo.pop_back();
		step->undo(ctrl);
		m_redo.push_back(step);
	}
	void redoStep(MainCtrl* ctrl) {
		action_base* step = m_redo.back(); m_redo.pop_back();
		step->redo(ctrl);
		m_undo.push_back(step);
	}
	void push(action_base* action) {
		while (!m_redo.empty()) {
			action_base* redoAction = m_redo.back();
			m_redo.pop_back();
			delete redoAction;
		}
		m_redo.clear(); //TODO: DEALLOC FFS
		m_undo.push_back(action);
	}
	bool canUndo() {
		return !m_undo.empty();
	}
	bool canRedo() {
		return !m_redo.empty();
	}
	String getRedoStep() {
		return (m_redo.back()?m_redo.back()->getDesc():"??");
	}
	String getUndoStep() {
		return (m_undo.back()?m_undo.back()->getDesc():"??");
	}
	int getNumUndoSteps() {
		return m_undo.size();
	}
	int getNumRedoSteps() {
		return m_redo.size();
	}
};
