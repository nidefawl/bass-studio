#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>
#include <nanovg.h>
#include <vector>
#include <memory>
#include <numeric>

#include "math/seq_math.h"
#include "saferef.h"
#include "str_util.h"
#include "seq_util.h"
#include "color_util.h"
#include "event.h"
#include "mouse.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "guifonts.h"
#include "gui/container/container.h"
#include "gui/container/container_dnd_layout.h"
#include "guiconstant.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/plugin/plugin.h"
#include "gui/controls/textfield.h"
#include "gui/controls/button.h"
#include "gui/controls/colorpick.h"
#include "gui/table/table.h"
#include "theme.h"
#include "thememgr.h"

#include "gui/container/container.h"
#include "gui/container/scrollcontainer.h"
#include "gui/controls/colorpick.h"
#include "gui/dropdown/dropdown.h"
#include "properties_table.h"
#include "gui/controls/inputfield.h"
#include "automation.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/vst_plugin.h"
#include "logging.h"
#include "renderresources.h"

namespace Table {
    struct tbltype_gui_flags {
        SafeRef<guibase> saferef;
        int mask;
    };

    class click_type_handler {
    public:
        virtual void onClickNotImplemented(const click_ctxt_t& ctxt) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, int32_t& value) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, float& value) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, glm::ivec2& value) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, glm::ivec4& value) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, NVGcolor& value) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, guibase* value) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, const tbltype_gui_flags& obj) {};
        virtual void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiColor::constant_t constant) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiConstant::constant_t constant) = 0;
        virtual void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, UIFont::font_type_t fonttype) = 0;
        virtual ~click_type_handler() = default;
    };

    template<>
    void cellClicked(const click_ctxt_t& ctxt, const tbltype_gui_flags& obj) {
        if (ctxt.callback) {
            ctxt.callback->onClick(ctxt, obj);
        }
    }

    template<typename T>
    void cellClicked(const click_ctxt_t& ctxt, const SafeRef<T>& obj) {
        auto* c = safeRefGet(obj);
        if (c && ctxt.callback) {
            ctxt.callback->onClick(ctxt, c);
        }
    }

    template <typename T>
    void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<T>& obj) {
        if (safeRefOk(obj.saferef)) {
            if (ctxt.callback) {
                ctxt.callback->onClick(ctxt, obj.t);
            }
        }
    }

    template<>
    void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<int32_t>& obj) {
        if (safeRefOk(obj.saferef)) {
            if (ctxt.callback) {
                ctxt.callback->onClick(ctxt, obj.t);
            }
        }
    }

    template<>
    void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<float>& obj) {
        if (safeRefOk(obj.saferef)) {
            if (ctxt.callback) {
                ctxt.callback->onClick(ctxt, obj.t);
            }
        }
    }

    template<>
    void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<glm::ivec2>& obj) {
        if (safeRefOk(obj.saferef)) {
            if (ctxt.callback) {
                ctxt.callback->onClick(ctxt, obj.t);
            }
        }
    }
}

using namespace Table;

class guidropdown_selectfont_ctxt : public guictxtmenu {
    guitheme_mgr* themeMgr;
    std::vector<String> strFontNames;
    UIFont::font_type_t fonttype;
public:
    guidropdown_selectfont_ctxt(guitheme_mgr* _themeMgr, UIFont::font_type_t _fonttype)
    : themeMgr(_themeMgr),
      fonttype(_fonttype)
    {
        this->size.x = 120;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
        int32_t idx = 0;
        strFontNames.reserve(RenderResources::fontsInstalled.size());
        for (auto& fontInstalled : RenderResources::fontsInstalled) {
            strFontNames.push_back(fontInstalled.name);
            addEntry(new ctxtmenu_entry(fontInstalled.name, idx));
            idx++;
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (_id >= 0 && _id < CtrSize(strFontNames)) {
            closeContextMenu();
            themeMgr->getRef().setFont(fonttype, strFontNames[_id]);
            //TODO: reload fonts (repopulate RenderResources::fontsLoaded

            themeMgr->getRef().bindFonts();
            return true;
        }
        return false;
    }
};

class guidropdown_selectfont : public guidropdownbase {
public:
    String current;
    UIFont::font_type_t fonttype;
    guidropdown_selectfont() :
        guidropdownbase() {
    }
    int32_t getLastIndex() override {
        return CtrSize(RenderResources::fontsInstalled) - 1;
    }
    String getString() override {
        guitheme_mgr* themeMgr = this->parentCtrl->getThemeMgr();
        if (this->parentCtrl) {
            return themeMgr->getRef().getFont(fonttype).name;
        }

        return current;
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        guitheme_mgr* themeMgr = this->parentCtrl->getThemeMgr();
        guictxtmenu_base *popup = new guidropdown_selectfont_ctxt(themeMgr, fonttype);
        popup->size = size;
        popup->setFontSize(size.y);
        this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
    }
};

template <typename T>
class guiproperties_table : public guictr_properties_table {
protected:
    struct cellclicked_t {
        ivec2 idx{-1,-1};
        ivec2 pos{0};
        ivec2 size{0};
    };
    Table::tbl m_table;
    const bool m_bGlobalInstance;
    const bool m_bAutoUpdateContents;
    const bool m_bOwnsObjPtr;
    bool m_bInteractiveMode = true;
    bool m_bMouseDown       = false;
    bool m_bNeedsRelayout   = false;

    T* m_unsafePointer;

    SafeRef<T> m_currentSafeRef;
    T* getObjPtr() {
        return safeRefGet(m_currentSafeRef);
    }

    gui_textfield m_textField;
    gui_numberinput_i32 m_numberInputI32;
    gui_numberinput_field_generic<float> m_numberInputFloat;
    gui_color_pick m_colorPick;
    guidropdown_selectfont m_selectFont;
    int32_t m_numberInputTmp = 0;

    float m_fontSize = 20;
    cellclicked_t m_LastClicked;

    std::vector<guibase*> m_controls;
public:
    guiproperties_table(T* _ptr, bool _isGlobalInstance, bool _ownsPtr)
        : guictr_properties_table(),
          m_bGlobalInstance(_isGlobalInstance),
          m_bAutoUpdateContents(_isGlobalInstance),
          m_bOwnsObjPtr(_ownsPtr),
          m_unsafePointer(_ptr),
          m_currentSafeRef(SafeRef<T>()),
          m_numberInputI32(nullptr),
          m_numberInputFloat(nullptr)
    {
        guiType = CTR_TYPE_PROPERTIES;
        getContainerLabel(guiType, this->label);
        setBackgroundRendered(true);
        padding = 4;
        margin = 2;

        //setBackgroundRendered(true);
        //setBackgroundRenderedInset(false);
        //setSnapSides(ivec4(1));
        addControl(&m_textField);
        addControl(&m_selectFont);
        addControl(&m_numberInputI32);
        addControl(&m_numberInputFloat);
        addControl(&m_colorPick);
        m_textField.fnFocus = [this](MouseHitEvt& evt, bool focused) {
            if (!focused) {
                setActiveControl(nullptr);
            }
        };
        m_numberInputI32.getField().fnFocus = [this](MouseHitEvt& evt, bool focused) {
            if (!focused) {
                setActiveControl(nullptr);
            }
        };
        m_numberInputFloat.getField().fnFocus = [this](MouseHitEvt& evt, bool focused) {
            if (!focused) {
                setActiveControl(nullptr);
            }
        };
        //padding          = 0;
        //margin           = 0;
        //scrollbarOutside = true;
        //maxHeight        = 220;
    }

    ~guiproperties_table() override {
        removeGuis();
        if (m_bOwnsObjPtr)
            delete m_unsafePointer;
    }

    // template specialization must provide
    void validateReferences();

    void addControl(guibase* g) {
        m_controls.push_back(g);
        g->setVisible(false);
        add(g);
    }

    void setActiveControl(guibase* g) {
        dbgassert(!g || isControl(g));
        for (guibase* g2 : m_controls) {
            g2->setVisible(g2 == g);
        }
        if (g) {
            this->parentCtrl->focusGui(g);
        }
    }

    bool isControl(guibase* g) {
        return STL_CONTAINS(m_controls, g);
    }

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        validateReferences();
        if (this->contains(mpos)) {
            ivec2 localMouse = this->toContainerSpace(mpos);
            for (guibase* gui : guis) {
                if (!gui->isVisible())
                    continue;
                if (gui->mouseHitTest(localMouse, evt)) {
                    return true;
                }
            }
            validateReferences();
            m_LastClicked  = cellclicked_t();
            ivec2 local = localMouse;
            ivec2 tableMin = ivec2(INSET_TABLE);
            ivec2 tableMax = tableMin + getSizeContent()-ivec2(INSET_TABLE<<1);
            if (local.x >= tableMin.x && local.y >= tableMin.y && local.x < tableMax.x && local.y < tableMax.y) {
                m_bMouseDown = true;
                GetCellClicked(m_table, theme, local-tableMin, m_LastClicked.idx, m_LastClicked.pos, m_LastClicked.size);
                onCellHover(m_LastClicked, evt);
            }
            evt.requestFocus(this);
            return true;
        }
        return false;
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        validateReferences();
        m_LastClicked  = cellclicked_t();
        ivec2 local = evt.relMousepos;
        ivec2 tableMin = ivec2(INSET_TABLE);
        ivec2 tableMax = tableMin + getSizeContent()-ivec2(INSET_TABLE<<1);
        if (local.x >= tableMin.x && local.y >= tableMin.y && local.x < tableMax.x && local.y < tableMax.y) {
            m_bMouseDown = true;
            GetCellClicked(m_table, theme, local-tableMin, m_LastClicked.idx, m_LastClicked.pos, m_LastClicked.size);
            onCellClicked(m_LastClicked, evt);
        }
        if (m_LastClicked.idx.x < 0 || m_LastClicked.idx.y < 0) {
            setActiveControl(nullptr);
        }
    }

    void handleDraggedMove(MouseEvent& evt) override {
        validateReferences();
    }

    void handleDraggedRelease(MouseEvent& evt) override {
        validateReferences();
        if (m_bMouseDown) {
            m_bMouseDown = false;
            onCellClicked(m_LastClicked, evt);
            m_LastClicked = cellclicked_t();
        }
    }

    void onCellHover(const cellclicked_t cell, const MouseHitEvt& evt) {
        if (cell.idx.x >= 0 && cell.idx.y >= 0) {
            table_entry_t& tableCell = GetCell(m_table, cell.idx.x, cell.idx.y);
            class mouseover_handler_t : public click_type_handler {
                guiproperties_table* const table;
                const cellclicked_t& clickedcell;
                const MouseHitEvt& evt;
            public:
                mouseover_handler_t(guiproperties_table* _table, const cellclicked_t& _clickedcell, const MouseHitEvt& _evt)
                    : click_type_handler(), table(_table), clickedcell(_clickedcell), evt(_evt) {

                }
                void hover() {
                    //log_printf("hover %d %d\n", clickedcell.idx.x, clickedcell.idx.y);
                }
                void onClickNotImplemented(const click_ctxt_t& ctxt) override {
                    table->setActiveControl(nullptr);
                }
                void onClick(const click_ctxt_t& ctxt, const tbltype_gui_flags& obj) override
                {
                    hover();
                }
                void onClick(const click_ctxt_t& ctxt, int32_t& value) override {
                    hover();
                }
                void onClick(const click_ctxt_t& ctxt, float& value) override {
                    hover();
                }
                void onClick(const click_ctxt_t& ctxt, glm::ivec2& value) override {
                    hover();
                }
                void onClick(const click_ctxt_t& ctxt, glm::ivec4& value) override {
                    hover();
                }
                void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiConstant::constant_t constant) override {
                    hover();
                }
                void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, UIFont::font_type_t fonttype) override {
                    hover();
                }
                void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiColor::constant_t constant) override {
                    hover();
                    BaseCtrl* const ctrl = table->parentCtrl;
                    if (theme == nullptr) {
                        ctrl->getTheme()->pingConstant(constant);
                    } else {
                        ctrl->getTheme()->endPing();
                    }
                }

                void onClick(const click_ctxt_t& ctxt, NVGcolor& value) override {
                    hover();
                }
                void onClick(const click_ctxt_t& ctxt, guibase* value) override {
                };
            };
            mouseover_handler_t handler(this, cell, evt);
            const click_ctxt_t ctxt = {this, &handler};
            tableCellClicked(ctxt, tableCell);
        }
    }

    void onCellClicked(cellclicked_t cell, MouseEvent& evt) {
        if (cell.idx.x >= 0 && cell.idx.y >= 0) {
            if (evt.type == MouseEventType::M_EVT_BTN_DOWN) {
                table_entry_t& tableCell = GetCell(m_table, cell.idx.x, cell.idx.y);
                class click_handler_t : public click_type_handler {
                    guiproperties_table* const table;
                    cellclicked_t& clickedcell;
                    MouseEvent& evt;
                public:
                    click_handler_t(guiproperties_table* _table, cellclicked_t& _clickedcell, MouseEvent& _evt)
                        : click_type_handler(), table(_table), clickedcell(_clickedcell), evt(_evt) {

                    }
                    void click() {
                    }
                    void onClickNotImplemented(const click_ctxt_t& ctxt) override {
                        table->setActiveControl(nullptr);
                    }
                    void onClick(const click_ctxt_t& ctxt, const tbltype_gui_flags& obj) override {
                        guibase* ref = safeRefGet(obj.saferef);
                        if (ref) {
                            bool b = ref->isFlag(obj.mask);
                            ref->setFlag(obj.mask, !b);
                        }

                    }
                    void onClick(const click_ctxt_t& ctxt, int32_t& value) override {
                        click();
                        int32_t posRight = clickedcell.pos.x+clickedcell.size.x-100;
                        auto& numberInput = table->m_numberInputI32;
                        numberInput.pos = ivec2(posRight, clickedcell.pos.y);
                        numberInput.size.x = 100;
                        numberInput.setRef(&value);
                        numberInput.layout();
                        table->setActiveControl(&numberInput);
                        evt.guiDragged = &numberInput;
                        numberInput.handleDraggedBegin(evt);
                    }
                    void onClick(const click_ctxt_t& ctxt, float& value) override {
                        click();
                        int32_t posRight = clickedcell.pos.x+clickedcell.size.x-100;
                        auto& numberInput = table->m_numberInputFloat;
                        numberInput.pos = ivec2(posRight, clickedcell.pos.y);
                        numberInput.size.x = 100;
                        numberInput.setRef(&value);
                        numberInput.layout();
                        table->setActiveControl(&numberInput);
                        evt.guiDragged = &numberInput;
                        numberInput.handleDraggedBegin(evt);
                    }
                    void onClick(const click_ctxt_t& ctxt, glm::ivec2& value) override {
                        click();
                        int32_t posRight = clickedcell.pos.x+clickedcell.size.x-100;
                        bool wasRightSide = evt.relMousepos.x > posRight;
                        auto& numberInput = table->m_numberInputI32;
                        numberInput.size = clickedcell.size;
                        if (wasRightSide) {
                            numberInput.pos = ivec2(posRight, clickedcell.pos.y);
                            numberInput.size.x = 100;
                            numberInput.setRef(&value.y);
                        } else {
                            numberInput.pos = ivec2(posRight-100, clickedcell.pos.y);
                            numberInput.size.x = 100;
                            numberInput.setRef(&value.x);
                        }
                        numberInput.layout();
                        table->setActiveControl(&numberInput);
                        evt.guiDragged = &numberInput;
                        numberInput.handleDraggedBegin(evt);
                    }
                    void onClick(const click_ctxt_t& ctxt, glm::ivec4& value) override {
                        click();
                        int w = 50;
                        for (int i = 0; i < 4; i++) {
                            int32_t posLeft = clickedcell.pos.x+clickedcell.size.x-(w*((3-i)+1));
                            int32_t posRight = clickedcell.pos.x+clickedcell.size.x-(w*(3-i));
                            bool inside = evt.relMousepos.x >= posLeft && evt.relMousepos.x < posRight;
                            if (inside) {
                                auto& numberInput = table->m_numberInputI32;
                                numberInput.size = clickedcell.size;
                                numberInput.pos = ivec2(posLeft, clickedcell.pos.y);
                                numberInput.size.x = w;
                                numberInput.setRef(&value[i]);
                                numberInput.layout();
                                table->setActiveControl(&numberInput);
                                evt.guiDragged = &numberInput;
                                numberInput.handleDraggedBegin(evt);
                            }
                        }
                    }
                    void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiConstant::constant_t constant) override {
                        click();
                        if (theme == nullptr)
                            return;
                        auto& numberInput = table->m_numberInputI32;
                        table->m_numberInputTmp            = theme->get(constant);
                        numberInput.setRef(&table->m_numberInputTmp);
                        numberInput.pos = clickedcell.pos;
                        numberInput.size = clickedcell.size;
                        numberInput.layout();
                        BaseCtrl* const ctrl = table->parentCtrl;
                        numberInput.fnValueEditChanged = [theme, constant, ctrl](gui_numberinput_field_base*,int32_t rgba) {
                            theme->set(constant, rgba);
                            if (ctrl)
                                ctrl->relayout();
                        };
                        numberInput.fnClamp = [constant](int32_t i) {
                            return i > constant.rangeMax ? constant.rangeMax : i < constant.rangeMin ? constant.rangeMin : i;
                        };
                        table->setActiveControl(&numberInput);
                        evt.guiDragged = &numberInput;
                        numberInput.handleDraggedBegin(evt);
                    }
                    void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, UIFont::font_type_t fonttype) override {
                        click();
                        if (theme == nullptr)
                            return;
                        guidropdown_selectfont& selectFont = table->m_selectFont;
                        selectFont.pos = clickedcell.pos;
                        selectFont.size = clickedcell.size;
                        auto t = theme->getFont(fonttype);
                        selectFont.current = t.name;
                        selectFont.fonttype = fonttype;
//                        textField.setValue(t.name);
//                        textField.setCallback([theme,ft=fonttype,&textField](const String& str) {
//
//                            textField.setCallback(nullptr);
//                            theme->setFont(ft, str);
//                            return true;
//                        });
                        evt.guiDragged = &selectFont;
                        table->setActiveControl(&selectFont);
                        selectFont.handleDraggedBegin(evt);
                        selectFont.handleDraggedRelease(evt);
                    }
                    void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiColor::constant_t constant) override {
                        click();
                        if (theme == nullptr)
                            return;
                        gui_color_pick* color = new gui_color_pick();
                        color->size = {480, 240};
                        color->pos = {0, 0};
                        //color->setRefNvg(&value);
                        color->setU32(theme->getColorInt32(constant));
                        color->fnSetValue = [theme, constant](int32_t rgba) {
                            theme->setColor(constant, rgba);
                        };
                        guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
                        ctxtMenu->size = color->size;
                        ctxtMenu->add(color);
                        // color->layout();
                        // ctxtMenu->layout();
                        ctxtMenu->canTakeInputFocus = true;
                        ctxtMenu->maxHeight = color->size.y;
                        dbgassert(!ctxtMenu->isBackgroundRendered());
                        ctxtMenu->setBackgroundRendered(false);
                        table->parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
                        table->setActiveControl(nullptr);
                        dbgassert(!ctxtMenu->isBackgroundRendered());
                    }

                    void onClick(const click_ctxt_t& ctxt, NVGcolor& value) override {
                        click();
                        gui_color_pick* color = new gui_color_pick();
                        color->size = { 480, 240 };
                        color->pos = { 0, 0 };
                        color->setRefNvg(&value);
                        color->setU32(nvgToRGBA(value));
                        guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
                        ctxtMenu->size = color->size;
                        ctxtMenu->add(color);
                        // color->layout();
                        // ctxtMenu->layout();
                        ctxtMenu->canTakeInputFocus = true;
                        ctxtMenu->maxHeight = color->size.y;
                        dbgassert(!ctxtMenu->isBackgroundRendered());
                        ctxtMenu->setBackgroundRendered(false);
                        table->parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
                        table->setActiveControl(nullptr);
                        dbgassert(!ctxtMenu->isBackgroundRendered());
                    }
                    void onClick(const click_ctxt_t& ctxt, guibase* value) override {
                        table->setDebugPropertyHandle(value);
                        table->determineSize(table->size);
                        table->layout();
                    };
                };
                click_handler_t handler( this, cell, evt );
                const click_ctxt_t ctxt = {this, &handler};
                tableCellClicked(ctxt, tableCell);
            }
        } else {
            setActiveControl(nullptr);
        }
    }

    void onTick(AppCtrl* appctrl) override {
        dbgassert(0);
        //layout();
    }

    void layout() override;

    void render(NVGcontext* vg) override;

    void renderDefault(NVGcontext* vg) {
        if (isBackgroundRendered()){
            renderBackground(vg);
        }
        //if (!setScissorTransformContainer(vg)) {
        //    return;
        //}
        if (!setScissorTransform(vg)) {
            return;
        }
        constexpr int FONT_SIZE_TOOLTIP_TITLE = 24;
        setFont(vg, FONT_SIZE_TOOLTIP_TITLE, THEMECOL_TEXT, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
        Table::DrawTableNVG(m_table, vg, theme, ivec2(INSET_TABLE), getSizeContent()-ivec2(INSET_TABLE<<1), m_fontSize);
        for (guibase* ctrl : m_controls) {
            if (ctrl->isVisible()) {
                ctrl->render(vg);
            }
        }
    }

    void setDebugPropertyHandle(void *ptr) override;
    void determineSize(glm::ivec2& prefSize) override;
};

template<typename T>
void addPropertiesFromGui(T& gui, Table::tbl* table);


template<>
void addPropertiesFromGui(guibase& gui, Table::tbl* table) {
    SafeRef<guibase> ref = gui.makeSafeRef();
    std::vector<tbl_row_t>& rows = table->rows;
    rows.push_back({{tblstr{"this"}, ref}});
    rows.push_back({{tblstr{"id"}, tbltypesaferef<int32_t>{ref, gui.id, nullptr}}});
    rows.push_back({{tblstr{"pos"}, tbltypesaferef<glm::ivec2>{ref, gui.pos, nullptr}}});
    rows.push_back({{tblstr{"size"}, tbltypesaferef<glm::ivec2>{ref, gui.size, nullptr}}});

    rows.push_back({{tblstr{"FLG_VISIBLE"}, tbltype_gui_flags{ref, FLG_VISIBLE}}});
    rows.push_back({{tblstr{"FLG_RENDER_BACKGROUND"}, tbltype_gui_flags{ref, FLG_RENDER_BACKGROUND}}});
    rows.push_back({{tblstr{"FLG_RENDER_BACKGROUND_INSET"}, tbltype_gui_flags{ref, FLG_RENDER_BACKGROUND_INSET}}});
    rows.push_back({{tblstr{"FLG_ENBL"}, tbltype_gui_flags{ref, FLG_ENBL}}});
    rows.push_back({{tblstr{"FLG_HVRD"}, tbltype_gui_flags{ref, FLG_HVRD}}});
    rows.push_back({{tblstr{"FLG_FOC"}, tbltype_gui_flags{ref, FLG_FOC}}});
    rows.push_back({{tblstr{"FLG_ACT"}, tbltype_gui_flags{ref, FLG_ACT}}});
    rows.push_back({{tblstr{"FLG_DRG"}, tbltype_gui_flags{ref, FLG_DRG}}});
    rows.push_back({{tblstr{"FLG_HAS_COLOR_BG"}, tbltype_gui_flags{ref, FLG_HAS_COLOR_BG}}});

    if (gui.parent) {
        SafeRef<guibase> parentSafeRef = gui.parent->makeSafeRef();
        rows.push_back({{tblstr{"parent"}, parentSafeRef}});
    } else {
        rows.push_back({{tblstr{"parent"}, tblstr{"<null>", 1}}});
    }
    String strTheme = gui.theme->name+StringFormat("[%7zX]", reinterpret_cast<uint64_t>(gui.theme));
    rows.push_back({{tblstr{"theme"}, tblString{strTheme, 1}}});
    rows.push_back({{tblstr{"theme2"}, tblString{strTheme, 1}}});
}

template<>
void addPropertiesFromGui(guictr_base& gui, Table::tbl* table) {
    SafeRef<guibase> ref = gui.makeSafeRef();
    std::vector<tbl_row_t>& rows = table->rows;
    rows.push_back({{tblstr{"this"}, ref}});
    rows.push_back({{tblstr{"id"}, tbltypesaferef<int32_t>{ref, gui.id, nullptr}}});
    rows.push_back({{tblstr{"pos"}, tbltypesaferef<glm::ivec2>{ref, gui.pos, nullptr}}});
    rows.push_back({{tblstr{"size"}, tbltypesaferef<glm::ivec2>{ref, gui.size, nullptr}}});
    guictr_layout* ctrlayout = nullptr;
    if ((ctrlayout = dynamic_cast<guictr_layout*>(&gui))) {
        String layoutName;
        switch (ctrlayout->getLayout()) {
        case container_layout::SOLE:
            layoutName = "SOLE";
            break;
        case container_layout::SPLIT_H:
            layoutName = "SPLIT_H";
            break;
        case container_layout::SPLIT_V:
            layoutName = "SPLIT_V";
            break;
        case container_layout::TABBED:
            layoutName = "TABBED";
            break;
        }
        rows.push_back({{tblstr{"layout"}, tblString{layoutName, 1}}});
    }
    //int padding = CONTENT_INSET;
    //int margin  = CTR_SPACING;
    //ivec4 snapSides{ 0, 0, 0, 0 };
    //std::vector<guibase*> guis;
    //bool sortChildren = false;
    rows.push_back({{tblstr{"padding"}, tbltypesaferef<int32_t>{ref, gui.padding, nullptr}}});
    rows.push_back({{tblstr{"margin"}, tbltypesaferef<int32_t>{ref, gui.margin, nullptr}}});
    rows.push_back({{tblstr{"snapSides"}, tbltypesaferef<glm::ivec4>{ref, gui.snapSides, nullptr}}});

    rows.push_back({{tblstr{"FLG_VISIBLE"}, tbltype_gui_flags{ref, FLG_VISIBLE}}});
    rows.push_back({{tblstr{"FLG_RENDER_BACKGROUND"}, tbltype_gui_flags{ref, FLG_RENDER_BACKGROUND}}});
    rows.push_back({{tblstr{"FLG_RENDER_BACKGROUND_INSET"}, tbltype_gui_flags{ref, FLG_RENDER_BACKGROUND_INSET}}});
    rows.push_back({{tblstr{"FLG_ENBL"}, tbltype_gui_flags{ref, FLG_ENBL}}});
    rows.push_back({{tblstr{"FLG_HVRD"}, tbltype_gui_flags{ref, FLG_HVRD}}});
    rows.push_back({{tblstr{"FLG_FOC"}, tbltype_gui_flags{ref, FLG_FOC}}});
    rows.push_back({{tblstr{"FLG_ACT"}, tbltype_gui_flags{ref, FLG_ACT}}});
    rows.push_back({{tblstr{"FLG_DRG"}, tbltype_gui_flags{ref, FLG_DRG}}});
    rows.push_back({{tblstr{"FLG_HAS_COLOR_BG"}, tbltype_gui_flags{ref, FLG_HAS_COLOR_BG}}});

    if (gui.parent) {
        SafeRef<guibase> parentSafeRef = gui.parent->makeSafeRef();
        rows.push_back({{tblstr{"parent"}, parentSafeRef}});
    } else {
        rows.push_back({{tblstr{"parent"}, tblstr{"<null>", 1}}});
    }
    String strTheme = gui.theme->name+StringFormat("[%7zX]", reinterpret_cast<uint64_t>(gui.theme));
    rows.push_back({{tblstr{"theme"}, tblString{strTheme, 1}}});
}

template <>
void guiproperties_table<guibase>::layout() {
    m_table.tableWidth = getSizeContent().x - (INSET_TABLE<<1);
    AdjustColSizes(m_table);
    if (m_table.colSizes.size() == 2) {
        m_table.colSizes[0] = math::max(100.0f, math::min(250.0f, 0.25f*m_table.tableWidth));
        m_table.colSizes[1] = m_table.tableWidth - m_table.colSizes[0];
    }
}
template <>
void guiproperties_table<guibase>::determineSize(glm::ivec2& prefSize) {
    //if (size.x == 0)
    prefSize.x = math::max(250, prefSize.x);
    m_fontSize = G_FONT_SCALE(theme->getFloat(GuiConstant::CONST_FONT_SIZE_TABLE));
    m_table.rowHeight = m_fontSize +INSET_TABLE_CELL_PADDING*2;
    m_table.rows.clear();
    m_table.titleCols.clear();
    m_table.colSizes.clear();
    auto obj = getObjPtr();
    if (obj && obj->parentCtrl)
    {
        obj->addProperties(&m_table);
    }
    prefSize.y = m_table.rows.size()* m_table.rowHeight+ m_table.rowHeight + 10;

}
template <>
void guiproperties_table<guibase>::render(NVGcontext* vg)  {
    renderDefault(vg);
}
template <>
void guiproperties_table<guibase>::validateReferences()  {
    auto obj = getObjPtr();
    if (!obj) {
        setActiveControl(nullptr);
        m_table.rows.clear();
        m_table.titleCols.clear();
        m_table.colSizes.clear();
    }
}
template <>
void guiproperties_table<guibase>::onTick(AppCtrl* appctrl) {
    if (m_bAutoUpdateContents) {
        auto obj = getObjPtr();
        auto ptrNew = appctrl->getGuiFocused();
        auto guibaseCheck = ptrNew;
        while (guibaseCheck) {
            if (guibaseCheck == this->parent) {
                return;
            }
            guibaseCheck = guibaseCheck->parent;
        }
        if (obj != ptrNew) {
            if (ptrNew) {
                m_currentSafeRef = ptrNew->makeSafeRef();
            } else {
                m_currentSafeRef = SafeRef<guibase>();
            }
            m_bNeedsRelayout = true;
        }
    }
    if (m_bNeedsRelayout) {
        m_bNeedsRelayout = false;
        onChildLayoutChanged(this);
    }
}
template <>
void guiproperties_table<guibase>::setDebugPropertyHandle(void *vPtr)  {
    if (!vPtr) {
        if (m_currentSafeRef.handler) {
            m_table.rows.clear();
            m_table.titleCols.clear();
            m_table.colSizes.clear();
            m_currentSafeRef = SafeRef<guibase>();
            m_bNeedsRelayout         = true;
        }
    } else {
        auto pObject = static_cast<guibase*>(vPtr);
        if (getObjPtr() != pObject) {
            /* enable debug background rendering */
            //pGui->id |= (1<<16);
            if (pObject) {
                m_currentSafeRef = pObject->makeSafeRef();
            } else {
                m_currentSafeRef = SafeRef<guibase>();
            }
            m_bNeedsRelayout = true;
        }
    }
}
struct tbltype_theme_color {
    guitheme_t* theme;
    GuiColor::constant_t constant;
};
struct tbltype_theme_constant {
    guitheme_t* theme;
    GuiConstant::constant_t constant;
};
struct tbltype_theme_font {
    guitheme_t* theme;
    UIFont::font_type_t fonttype;
};

namespace Table {
void drawColor(NVGcontext* vg, ivec2 pos, ivec2 size, uint32_t rgba) {
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
    String strColorHex = StringFormat("%08X", rgba);
    int sizeQuad = size.y - INSET_TABLE_CELL_PADDING * 2;

    nvgText(vg, pos.x + size.x - INSET_TABLE_CELL_PADDING * 2 - sizeQuad, pos.y + size.y - INSET_TABLE_CELL_PADDING, StringAsCStr(strColorHex), nullptr);
    
    nvgBeginPath(vg);
    nvgRect(vg, pos.x + size.x - INSET_TABLE_CELL_PADDING - sizeQuad, pos.y + INSET_TABLE_CELL_PADDING,
        sizeQuad, sizeQuad);
    auto previous = nvgGetCurrentAndSetFillColor(vg, rgbaToNvg(rgba));
    nvgFill(vg);
    nvgFillColor(vg, previous);
}

template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltype_gui_flags& obj) {
    const vec2& pos = ctxt.pos;
    const vec2& size = ctxt.size;
    guibase* ref = safeRefGet(obj.saferef);
    if (ref)
    {
        int flags = ref->getStateFlags();
        const char* strState = (flags&obj.mask) != 0 ? "1" : "0";
        nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
        nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, strState, nullptr);
    }
}

void drawTbl(const table_ctxt_t& ctxt, const tbltype_theme_color& obj) {
    auto t = obj.theme->getColorInt32(obj.constant);
    drawColor(ctxt.vg, ctxt.pos, ctxt.size, t);
}
template <>
void cellClicked(const click_ctxt_t& ctxt, const tbltype_theme_color& obj) {
    ctxt.callback->onClick(ctxt, obj.theme, obj.constant);
}
template <>
void cellClicked(const click_ctxt_t& ctxt, const GuiColor::constant_t& obj) {
    if (ctxt.callback) {
        ctxt.callback->onClick(ctxt, (guitheme_t*)nullptr, obj);
    }
}
template <>
void cellClicked(const click_ctxt_t& ctxt, const GuiConstant::constant_t& obj) {
    if (ctxt.callback) {
        ctxt.callback->onClick(ctxt, (guitheme_t*)nullptr, obj);
    }
}

void drawTbl(const table_ctxt_t& ctxt, const tbltype_theme_constant& obj) {
    int32_t t = obj.theme->get(obj.constant);
    drawTbl(ctxt, t);
}
template <>
void cellClicked(const click_ctxt_t& ctxt, const tbltype_theme_constant& obj) {
    ctxt.callback->onClick(ctxt, obj.theme, obj.constant);
}

void drawTbl(const table_ctxt_t& ctxt, const tbltype_theme_font& obj) {
    auto t = obj.theme->getFont(obj.fonttype);
    drawTbl(ctxt, t.name);
}
template <>
void cellClicked(const click_ctxt_t& ctxt, const tbltype_theme_font& obj) {
    ctxt.callback->onClick(ctxt, obj.theme, obj.fonttype);
}
}

template <>
void guiproperties_table<guitheme_t>::layout() {
    m_table.tableWidth = getSizeContent().x - (INSET_TABLE<<1);
    AdjustColSizes(m_table);
    if (m_table.colSizes.size() == 2) {
        m_table.colSizes[0] = math::max(220.0f, math::min(450.0f, 0.25f*m_table.tableWidth));
        m_table.colSizes[1] = m_table.tableWidth - m_table.colSizes[0];
    }
}

template <>
void guiproperties_table<guitheme_t>::determineSize(glm::ivec2& prefSize) {
    //TODO: only fully repopulate table if necessary.
    //    size.x = 250;
    m_fontSize = G_FONT_SCALE(theme->getFloat(GuiConstant::CONST_FONT_SIZE_TABLE));
    m_fontSize = math::max(8.0f, m_fontSize);
    m_table.rowHeight = m_fontSize +INSET_TABLE_CELL_PADDING*2;
    m_table.rows.clear();
    m_table.titleCols.clear();
    m_table.colSizes.clear();
    auto currentObjPtr = m_unsafePointer;
    m_table.rows.push_back({{tblstr{"this"}, tblint{(int64_t) currentObjPtr, "%08X"}}});
    if (currentObjPtr)
    {
        auto add = [this](auto && x, const auto& y) {
            m_table.rows.push_back({{x, y}});
        };
        std::vector<GuiColor::constant_t> vec = GuiColor::getAllConstants();
        std::sort(vec.begin(), vec.end(), [](auto& a, auto& b){ return strcmp(a.name, b.name) < 0; });
        for (auto _constant : vec) {
            add(_constant, tbltype_theme_color{ currentObjPtr, _constant });
        }
        std::vector<GuiConstant::constant_t> vec2 = GuiConstant::getAllConstants();
        std::sort(vec2.begin(), vec2.end(), [](auto& a, auto& b){ return strcmp(a.name, b.name) < 0; });
        for (auto _constant2 : vec2) {
            add(_constant2, tbltype_theme_constant{ currentObjPtr, _constant2 });
        }
        std::vector<UIFont::font_type_t> vec3 = UIFont::getAllConstants();
        std::sort(vec3.begin(), vec3.end(), [](auto& a, auto& b){ return strcmp(a.name, b.name) < 0; });
        for (auto _constant3 : vec3) {
            add(tblstr{ _constant3.name }, tbltype_theme_font{ currentObjPtr, _constant3 });
        }
    }

    prefSize.y = m_table.rows.size()* m_table.rowHeight;
}

template <>
void guiproperties_table<guitheme_t>::render(NVGcontext* vg)  {
    renderDefault(vg);
}
template <>
void guiproperties_table<guitheme_t>::validateReferences()  {
}

template <>
void guiproperties_table<guitheme_t>::onTick(AppCtrl*) {
    if (m_bNeedsRelayout) {
        m_bNeedsRelayout = false;
        onChildLayoutChanged(this);
    }
}

template <>
void guiproperties_table<guitheme_t>::setDebugPropertyHandle(void *vPtr) {
    if (!vPtr) {
        m_unsafePointer = nullptr;
        m_table.rows.clear();
        m_table.titleCols.clear();
        m_table.colSizes.clear();
    } else {
        m_unsafePointer = static_cast<guitheme_t*>(vPtr);
    }
}

class guidropdown_selecttheme_ctxt : public guictxtmenu {
    guitheme_mgr* themeMgr;
    std::vector<String> strThemeNames;
public:
    explicit guidropdown_selecttheme_ctxt(guitheme_mgr* _themeMgr) : themeMgr(_themeMgr) {
        this->size.x = 120;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
        int32_t idx = 0;
        _themeMgr->getThemeNames(strThemeNames);
        for (auto str : strThemeNames) {
            addEntry(new ctxtmenu_entry(str, idx));
            idx++;
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (_id >= 0 && _id < CtrSize(strThemeNames)) {
            closeContextMenu();
            themeMgr->setThemeName(strThemeNames[_id]);
            return true;
        }
        return false;
    }
};

class guidropdown_selecttheme : public guidropdownbase {
public:
    guidropdown_selecttheme() :
        guidropdownbase() {
    }
    String getString() override {
        guitheme_mgr* themeMgr = this->parentCtrl->getThemeMgr();
        return themeMgr->getThemeName();
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        guitheme_mgr* themeMgr = this->parentCtrl->getThemeMgr();
        guictxtmenu_base *popup = new guidropdown_selecttheme_ctxt(themeMgr);
        popup->size = size;
        popup->setFontSize(size.y);
        this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
    }
};

class guictr_theme_settings : public guictr_base {
    guiproperties_table<guitheme_t> themeProperties;
    guictr_scrollbar scrollContainer;
    guidropdown_selecttheme selectTheme;
    guibutton buttonAdd;
    guibutton buttonRemove;
    guibutton buttonSave;
public:
    guictr_theme_settings()
        : guictr_base(),
          themeProperties(nullptr, false, false),
          scrollContainer(),
          selectTheme()
    {
        guiType = CTR_TYPE_THEME;
        getContainerLabel(guiType, this->label);
        padding = 4;
        margin = 2;
        buttonAdd.setText("+");
        buttonRemove.setText("-");
        buttonSave.setText("save");
        add(&scrollContainer);
        add(&selectTheme);
        add(&buttonSave);
        add(&buttonRemove);
        add(&buttonAdd);
        scrollContainer.add(&themeProperties);
        scrollContainer.maxHeight = -1;
    }

    ~guictr_theme_settings() override {
        removeGuis();
    }

    void render(NVGcontext* vg) override {
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto c : guis) {
            if (c->size.x <= 0 || c->size.y <= 0) {
                log_printf("warning, skip rendering child container with size <= 0 0\n");
                continue;
            }
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }

    void buttonClicked(guibase* button) override {
        if (button == &buttonAdd) {
            guitheme_mgr* thememgr = parentCtrl->getThemeMgr();
            thememgr->saveCurrentAsNewTheme("User");
            this->parentCtrl->relayout();
        }
        if (button == &buttonRemove) {
            guitheme_mgr* thememgr = parentCtrl->getThemeMgr();
            thememgr->removeTheme(thememgr->getRef());
            this->parentCtrl->relayout();
        }
        if (button == &buttonSave) {
            guitheme_mgr* thememgr = parentCtrl->getThemeMgr();
            thememgr->saveThemes();
        }
    }

    void layout() override {
        ivec2 size = getSizeContent();
        int32_t hTop = HEIGHT_DEFAULT_INPUT;
        buttonAdd.size = { hTop, hTop };
        buttonRemove.size = { hTop, hTop };
        buttonSave.size = { hTop*3, hTop };
        buttonSave.pos = { size.x - buttonSave.size.x, 0 };
        buttonRemove.pos = { buttonSave.left() - buttonRemove.size.x, 0 };
        buttonAdd.pos = { buttonRemove.left() - buttonAdd.size.x, 0 };
        selectTheme.pos = {0, 0};
        selectTheme.size = { buttonAdd.left(), hTop};
        scrollContainer.pos = {0, hTop};
        scrollContainer.size = {size.x, size.y-hTop};
        scrollContainer.determineSize(scrollContainer.size);

        for (auto c : guis) {
            c->layout();
        }
    }

    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        guitheme_mgr* thememgr = parentCtrl->getThemeMgr();
        themeProperties.setDebugPropertyHandle(&thememgr->getRef());
    }
};

guictr_base* makeCtrTheme() {
    auto* ctr = new guictr_theme_settings();
    return ctr;
}

std::vector<guiproperties_table<guibase>*> g_propTableInstances;
void setGlobalDebugPropertyHandle(void* ptr) {
    for (auto* instance : g_propTableInstances) {
        if (instance->parentCtrl) {
            instance->setDebugPropertyHandle(ptr);
        }

    }
}

guictr_properties_table* makeUniquePropertiesCtr() {
    return new guiproperties_table<guibase>(nullptr, false, false);
}

guictr_base* makeCtrProperties() {
    auto* ptr = new guiproperties_table<guibase>(nullptr, true, false);
    g_propTableInstances.push_back(ptr);
    return ptr;
}

template <>
guiproperties_table<guibase>::~guiproperties_table() {
    removeGuis();
    if (m_bGlobalInstance) {
        always_assert(removeEntry(g_propTableInstances, this));
    }
    if (m_bOwnsObjPtr)
        delete m_unsafePointer;
}
