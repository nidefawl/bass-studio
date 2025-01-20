#include "DropTarget.hpp"
#include <ole2.h>
#include "event.hpp"
#include "math/vec.hpp"
#include "str_win32.hpp"
#include "keyboard.hpp"
#include "error.hpp"



class DropTargetImpl : public IDropTarget {
public:
    DropTargetImpl(HWND hwnd, DropTargetListener* dropTargetListener);
    virtual ~DropTargetImpl() = default;

private:
    static void getFilePaths(IDataObject* pDataObject, std::vector<String>& files);

    DropTargetListener* m_pDropTargetListener;
    LONG m_lRefCount     = 0;
    bool m_validDropType = false;
    HWND m_hwnd          = nullptr;


public:
    // IUnknown implementation
    HRESULT __stdcall QueryInterface(REFIID iid, void** ppvObject) override;
    ULONG __stdcall AddRef() override;
    ULONG __stdcall Release() override;

    // IDropTarget implementation
    HRESULT __stdcall DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT __stdcall DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    HRESULT __stdcall DragLeave() override;
    HRESULT __stdcall Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;

public:
};


bool IsOfType(IDataObject* pDataObject, CLIPFORMAT type) {
    FORMATETC fmtetc = { type, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return pDataObject->QueryGetData(&fmtetc) == S_OK;
}

DropTargetImpl::DropTargetImpl(HWND hwnd, DropTargetListener* dropTargetListener) {
    m_pDropTargetListener = dropTargetListener;
    m_hwnd                = hwnd;
}

HRESULT __stdcall DropTargetImpl::QueryInterface(REFIID iid, void** ppvObject) {
    if (iid == IID_IDropTarget || iid == IID_IUnknown) {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG __stdcall DropTargetImpl::AddRef() {
    return InterlockedIncrement(&m_lRefCount);
}

ULONG __stdcall DropTargetImpl::Release() {
    LONG count = InterlockedDecrement(&m_lRefCount);
    return count;
}

ivec2 toScreenSpace(HWND hwnd, POINTL pt) {
    POINT pos = { 0, 0 };
    ClientToScreen(hwnd, &pos);
    return { pt.x - pos.x, pt.y - pos.y };
}
KeyboardMods toInternalKeyboardMods(DWORD grfKeyState) {
    int mods = 0;
    if (grfKeyState & MK_CONTROL)
        mods |= KB_MOD_SYSTEM;
    if (grfKeyState & MK_SHIFT)
        mods |= KB_MOD_SHIFT;
    if (grfKeyState & MK_ALT)
        mods |= KB_MOD_ALT;
    return static_cast<KeyboardMods>(mods);
}

HRESULT __stdcall DropTargetImpl::DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    // does the dataobject contain data we want?
    try {
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

    } catch (std::exception& e) {
        handleStdException(e);
    }
    return S_OK;
}
HRESULT __stdcall DropTargetImpl::DragOver(DWORD grfKeyState, POINTL pt,
                                           DWORD* pdwEffect) {
    try {
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
    } catch (std::exception& e) {
        handleStdException(e);
    }
    return S_OK;
}

HRESULT __stdcall DropTargetImpl::DragLeave() {
    try {
        if (m_validDropType) {
            this->m_pDropTargetListener->filesDropCancel();
        }
    } catch (std::exception& e) {
        handleStdException(e);
    }
    return S_OK;
}

HRESULT __stdcall DropTargetImpl::Drop(IDataObject* pDataObject, DWORD grfKeyState,
                                       POINTL pt, DWORD* pdwEffect) {

    try {
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

    } catch (std::exception& e) {
        handleStdException(e);
    }

    return S_OK;
}

void DropTargetImpl::getFilePaths(IDataObject* pDataObject, std::vector<String>& files) {
    // construct a FORMATETC object
    FORMATETC fmtetc = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stgmed;

    // See if the dataobject contains any TEXT stored as a HGLOBAL
    if (pDataObject->QueryGetData(&fmtetc) == S_OK) {
        // Yippie! the data is there, so go get it!
        if (pDataObject->GetData(&fmtetc, &stgmed) == S_OK) {

            // we asked for the data as a HGLOBAL, so access it appropriately
            PVOID data = GlobalLock(stgmed.hGlobal);

            auto hdrop = static_cast<HDROP>(data);

            UINT uNumFiles = DragQueryFile(hdrop, (UINT) -1, nullptr, 0U);

            for (UINT uFile = 0; uFile < uNumFiles; uFile++) {
                // Get the next filename from the HDROP info.
                TCHAR szNextFile[MAX_PATH]{};
                if (DragQueryFile(hdrop, uFile, szNextFile, MAX_PATH) > 0) {
                    files.emplace_back(szNextFile);
                }
            }

            GlobalUnlock(stgmed.hGlobal);

            // release the data using the COM API
            ReleaseStgMedium(&stgmed);
        }
    }
}

DropTarget* RegisterDropWindow(HWND hwnd,
                               DropTargetListener* dropTargetListener) {
    auto* pDropTarget = new DropTargetImpl(hwnd, dropTargetListener);

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
