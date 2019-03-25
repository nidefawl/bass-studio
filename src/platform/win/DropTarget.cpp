#include "DropTarget.h"
#include <ole2.h>
#include "math/vec.h"
#include "str_win32.h"
#include "keyboard.h"


void handleStdException(std::exception& e);
void handleException();
#define EXC_TRY try {
#define EXC_CATCH \
	} catch (std::exception& e) { 									\
		handleStdException(e);										\
	} catch (...) {													\
		handleException();											\
	}
class DropTargetImpl : public IDropTarget
{
public:
	DropTargetImpl(HWND hwnd, DropTargetListener *dropTargetListener);
	virtual ~DropTargetImpl() { }

private:

    void getFilePaths(IDataObject *pDataObject, std::vector<String>& files);

    DropTargetListener *m_pDropTargetListener;
	LONG	m_lRefCount = 0;
	bool    m_validDropType = false;
	HWND m_hwnd = NULL;


public:
    // IUnknown implementation
	HRESULT __stdcall QueryInterface (REFIID iid, void ** ppvObject);
	ULONG	__stdcall AddRef (void);
	ULONG	__stdcall Release (void);

	// IDropTarget implementation
	HRESULT __stdcall DragEnter (IDataObject * pDataObject, DWORD grfKeyState, POINTL pt, DWORD * pdwEffect);
	HRESULT __stdcall DragOver (DWORD grfKeyState, POINTL pt, DWORD * pdwEffect);
	HRESULT __stdcall DragLeave (void);
	HRESULT __stdcall Drop (IDataObject * pDataObject, DWORD grfKeyState, POINTL pt, DWORD * pdwEffect);

public:
};


bool IsOfType(IDataObject *pDataObject, CLIPFORMAT type) {
	FORMATETC fmtetc = { type, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	return pDataObject->QueryGetData(&fmtetc) == S_OK ? true : false;
}

DropTargetImpl::DropTargetImpl(HWND hwnd, DropTargetListener *dropTargetListener) {
	m_pDropTargetListener = dropTargetListener;
	m_hwnd = hwnd;
}

HRESULT __stdcall DropTargetImpl::QueryInterface(REFIID iid, void ** ppvObject) {
	if(iid == IID_IDropTarget || iid == IID_IUnknown) {
		AddRef();
		*ppvObject = this;
		return S_OK;
	}
	*ppvObject = 0;
	return E_NOINTERFACE;
}

ULONG __stdcall DropTargetImpl::AddRef(void) {
	return InterlockedIncrement(&m_lRefCount);
}

ULONG __stdcall DropTargetImpl::Release(void) {
	LONG count = InterlockedDecrement(&m_lRefCount);
	return count;
}

ivec2 toScreenSpace(HWND hwnd, POINTL pt) {
    POINT pos = { 0, 0 };
    ClientToScreen(hwnd, &pos);
	return ivec2(pt.x - pos.x, pt.y - pos.y);
}
int toInternalKeyboardMods(DWORD grfKeyState) {
	int mods = 0;
	if (grfKeyState&MK_CONTROL)
		mods |= KB_MOD_SYSTEM;
	if (grfKeyState&MK_SHIFT)
		mods |= KB_MOD_SHIFT;
	if (grfKeyState&MK_ALT)
		mods |= KB_MOD_ALT;
	return mods;
}

HRESULT __stdcall DropTargetImpl::DragEnter(IDataObject * pDataObject, DWORD grfKeyState, POINTL pt, DWORD * pdwEffect) {
	// does the dataobject contain data we want?
	EXC_TRY
	m_validDropType = IsOfType(pDataObject, CF_HDROP);
	if (m_validDropType) {
		*pdwEffect = (*pdwEffect) & DROPEFFECT_COPY;
		if (((*pdwEffect) & DROPEFFECT_COPY) != 0) {
			std::vector<String> filePaths;
			getFilePaths(pDataObject, filePaths);
			bool result = this->m_pDropTargetListener->filesDropBegin(filePaths, toScreenSpace(m_hwnd, pt), toInternalKeyboardMods(grfKeyState));
			if (!result) {
				*pdwEffect = DROPEFFECT_NONE;
			}
		}
	} else {
		*pdwEffect = DROPEFFECT_NONE;
	}
	EXC_CATCH
	return S_OK;
}
HRESULT __stdcall DropTargetImpl::DragOver(DWORD grfKeyState, POINTL pt,
		DWORD * pdwEffect) {
	EXC_TRY
	if (m_validDropType) {
		*pdwEffect = (*pdwEffect) & DROPEFFECT_COPY;
		if (((*pdwEffect) & DROPEFFECT_COPY) != 0) {
			bool result = this->m_pDropTargetListener->filesDropMove(toScreenSpace(m_hwnd, pt), toInternalKeyboardMods(grfKeyState));
			if (!result) {
				*pdwEffect = DROPEFFECT_NONE;
			}
		}
	} else {
		*pdwEffect = DROPEFFECT_NONE;
	}

	EXC_CATCH
	return S_OK;
}

HRESULT __stdcall DropTargetImpl::DragLeave(void) {
	return S_OK;
}

HRESULT __stdcall DropTargetImpl::Drop(IDataObject * pDataObject, DWORD grfKeyState,
		POINTL pt, DWORD * pdwEffect) {

	EXC_TRY
	if (m_validDropType) {
		*pdwEffect = (*pdwEffect) & DROPEFFECT_COPY;
		if (((*pdwEffect) & DROPEFFECT_COPY) != 0) {
			std::vector<String> filePaths;
			getFilePaths(pDataObject, filePaths);

			bool result = this->m_pDropTargetListener->filesDropFinal(filePaths, toScreenSpace(m_hwnd, pt), toInternalKeyboardMods(grfKeyState));
			if (!result) {
				*pdwEffect = DROPEFFECT_NONE;
			}
		}
	} else {
		*pdwEffect = DROPEFFECT_NONE;
	}
	EXC_CATCH

	return S_OK;
}

void DropTargetImpl::getFilePaths(IDataObject *pDataObject, std::vector<String>& files) {
	// construct a FORMATETC object
	FORMATETC fmtetc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stgmed;

	;
	// See if the dataobject contains any TEXT stored as a HGLOBAL
	if (pDataObject->QueryGetData(&fmtetc) == S_OK) {
		// Yippie! the data is there, so go get it!
		if (pDataObject->GetData(&fmtetc, &stgmed) == S_OK) {

			// we asked for the data as a HGLOBAL, so access it appropriately
			PVOID data = GlobalLock(stgmed.hGlobal);

			HDROP &hdrop = (HDROP &) data;

			UINT uNumFiles = DragQueryFile(hdrop, -1, NULL, 0);

			for (UINT uFile = 0; uFile < uNumFiles; uFile++) {
				// Get the next filename from the HDROP info.
				TCHAR buf[MAX_PATH];
				LPSTR szNextFile = buf;
				if (DragQueryFile(hdrop, uFile, szNextFile, MAX_PATH) > 0) {
					files.push_back(szNextFile);
				}
			}

			GlobalUnlock(stgmed.hGlobal);

			// release the data using the COM API
			ReleaseStgMedium(&stgmed);
		}
	}
}

DropTarget *RegisterDropWindow(HWND hwnd,
		DropTargetListener *dropTargetListener) {
	DropTargetImpl *pDropTarget = new DropTargetImpl(hwnd, dropTargetListener);

	// acquire a strong lock
	CoLockObjectExternal(pDropTarget, TRUE, FALSE);

	// tell OLE that the window is a drop target
	RegisterDragDrop(hwnd, pDropTarget);

	return new DropTarget(pDropTarget);
}

void UnregisterDropWindow(HWND hwnd, DropTarget* pDropTarget) {
	DropTargetImpl* pDropTargetImpl = pDropTarget->impl;
	// remove drag+drop
	RevokeDragDrop(hwnd);

	// remove the strong lock
	CoLockObjectExternal(pDropTargetImpl, FALSE, TRUE);

	// release our own reference
	pDropTargetImpl->Release();
	delete pDropTarget;
	delete pDropTargetImpl;
}
