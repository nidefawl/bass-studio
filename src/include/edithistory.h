#pragma once
#include <vector>
#include "str_util.h"
class MainCtrl;
class action_base {
public:
	std::string desc;
	bool errored = false;
	std::string errorDesc;
	virtual ~action_base(){ };
	virtual void undo(MainCtrl* ctrl) = 0;
	virtual void redo(MainCtrl* ctrl) = 0;

	virtual void releaseResources(MainCtrl* ctrl) { };
	String getDesc() {
		return desc;
	}
	void setError(String err) {
		this->errored = true;
		this->errorDesc = err;
	}
};

class edithistory {
	std::vector<action_base*> m_undo;
	std::vector<action_base*> m_redo;
public:
	void clear(MainCtrl* ctrl) {
		while (!m_redo.empty()) {
			action_base* redoAction = m_redo.back();
			m_redo.pop_back();
			redoAction->releaseResources(ctrl);
			delete redoAction;
		}
		while (!m_undo.empty()) {
			action_base* undoAction = m_undo.back();
			m_undo.pop_back();
			undoAction->releaseResources(ctrl);
			delete undoAction;
		}
	}
	void undoStep(MainCtrl* ctrl) {
		action_base* step = m_undo.back(); m_undo.pop_back();
		step->undo(ctrl);
		assert(!step->errored);
		m_redo.push_back(step);
	}
	void redoStep(MainCtrl* ctrl) {
		action_base* step = m_redo.back(); m_redo.pop_back();
		step->redo(ctrl);
		assert(!step->errored);
		m_undo.push_back(step);
	}
	void push(MainCtrl* ctrl, action_base* action) {
		while (!m_redo.empty()) {
			action_base* redoAction = m_redo.back();
			m_redo.pop_back();
			redoAction->releaseResources(ctrl);
			delete redoAction;
		}
		m_redo.clear();
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
