#pragma once
#include <vector>
#include "str_util.h"
#include "assert_dbg.h"

class DawInstance;
class action_base {
public:
	std::string desc;
	bool errored = false;
	std::string errorDesc;
	virtual ~action_base(){ };
	virtual void undo(DawInstance* ctrl) = 0;
	virtual void redo(DawInstance* ctrl) = 0;

	virtual void releaseResources(DawInstance* ctrl) { };
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
	int64_t revision = 0;
public:
	int64_t getRevision() const {
		return revision;
	}
	void clear(DawInstance* ctrl) {
		revision = -1;
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
		revision = 0;
	}
	void undoStep(DawInstance* ctrl) {
		action_base* step = m_undo.back(); m_undo.pop_back();
		step->undo(ctrl);
		dbgassert(!step->errored);
		m_redo.push_back(step);
		revision--;
	}
	void redoStep(DawInstance* ctrl) {
		action_base* step = m_redo.back(); m_redo.pop_back();
		step->redo(ctrl);
		dbgassert(!step->errored);
		m_undo.push_back(step);
		revision++;
	}
	void push(DawInstance* ctrl, action_base* action) {
		while (!m_redo.empty()) {
			action_base* redoAction = m_redo.back();
			m_redo.pop_back();
			redoAction->releaseResources(ctrl);
			delete redoAction;
		}
		m_redo.clear();
		m_undo.push_back(action);
		revision++;
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
	size_t getNumUndoSteps() {
		return m_undo.size();
	}
	size_t getNumRedoSteps() {
		return m_redo.size();
	}
	void getActions(std::vector<action_base*>& outUndo,
			std::vector<action_base*>& outRedo) {
		outUndo = m_undo;
		outRedo = m_redo;
	}
};
