#include "contextmenu.h"
#include "menu.h"
#include "window.h"
#include "renderresources.h"

ctxtmenu_entry::ctxtmenu_entry(AppCtrl* ctrl, GlobalCommandType _type) 
    : commandtype(_type)
{
    dbgassert(ctrl->getCommandManager());
    auto cmd = ctrl->getCommandManager()->getCommand(commandtype);
    dbgassert(cmd);
    if (cmd) {
        id    = static_cast<int32_t>(_type);
        title = cmd->desc.name;
        if (cmd->desc.iconId > -1) {
            setIcon(&RenderResources::imgIcons[cmd->desc.iconId], GuiColor::COL_WHITE);
        }
        auto combo = cmd->getFirstKeyCombo();
        if (combo && combo->keyCode != KeyboardKey::DAW_KB_INVALID) {
            rightTitle = combo->toString();
        }
    }
}
void ngui::Menu::addCommand(AppCtrl* ctrl, GlobalCommandType _type, int arg1, String customText) {
    dbgassert(ctrl->getCommandManager());
    auto cmd = ctrl->getCommandManager()->getCommand(_type);
    dbgassert(cmd);
    if (cmd) {
        Menu& m   = makeChild_();
        m.registeredCommand = cmd;
        m.type    = menu_type::command;
        m.command.command = static_cast<int32_t>(_type);
        m.command.argInt = arg1;
        if (!customText.empty()) {
            m.title = std::move(customText);
        } else {
            m.title = cmd->desc.name;
        }
        m.icon = cmd->desc.iconId;
        add(&m);
    }
}

guictxtmenu::guictxtmenu() : guictxtmenu_base() {
    setCanMouseHit(true);
    setBackgroundRendered(true);
    setBackgroundRenderedInset(false);
    setSnapSides(ivec4(1));
}

guictxtmenu::~guictxtmenu() {
    for (ctxtmenu_entry* e : entries) {
        delete e;
    }
}

void guictxtmenu::setControl(BaseCtrl* parentCtrl) {
    guictxtmenu_base::setControl(parentCtrl);
    for (auto* g : entries) {
        g->theme = parentCtrl ? parentCtrl->getTheme() : nullptr;
    }
}

void guictxtmenu::addEntry(ctxtmenu_entry* entry) {
    size.x = math::max(size.x, entry->width);
    entries.push_back(entry);
    entry->theme = theme;
}

void guictxtmenu::handleDraggedBegin(MouseEvent& evt) {
    ivec2 local = evt.relMousepos;
    for (ctxtmenu_entry* e : entries) {
        if (pos.y + e->y > parent->size.y)
            break;
        if (pos.y + e->y + e->height < 0)
            continue;
        int n = e->getClicked(size, local);
        if (n >= 0) {
            clickedElement(e, n);
            return;
        }
    }
}

void guictxtmenu::layout() {
    determine_string_width strw(parentCtrl, theme);
    int y = paddingV;
    for (ctxtmenu_entry* e : entries) {
        e->layout(size, fontSize, strw);
        e->y = y;
        y += e->height + paddingV;
    }
}

void guictxtmenu::determineSize(ivec2& prefSize) {
    ivec2 newMaxSize = { size.x, paddingV };
    for (ctxtmenu_entry* e : entries) {
        newMaxSize.x = math::max(newMaxSize.x, e->width);
        newMaxSize.y += e->height + paddingV;
    }
    if (entries.empty()) {
        newMaxSize.y += paddingV;
    }
    prefSize = newMaxSize;
}

void guictxtmenu::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    setScissorTransform(vg);
    int idx     = 0;
    ivec2 mouse = parentCtrl->m_mousePos;
    mouse       = toContainerSpace(mouse);
    for (ctxtmenu_entry* e : entries) {
        if (pos.y + e->y > parent->size.y)
            break;
        if (pos.y + e->y + e->height < 0)
            continue;
        e->render(size, vg, idx, mouse);
        idx++;
    }
}

bool guictxtmenu::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 localMouse         = this->toContainerSpace(mpos);
        ctxtmenu_entry* entryHit = nullptr;
        for (ctxtmenu_entry* e : entries) {
            int n = e->getClicked(size, localMouse);
            if (n >= 0) {
                entryHit = e;
                break;
            }
        }
        if (!entryHit || !entryHit->isMenuOpen()) {
            //close other submenu at same level
            closeAllSubmenus();
            if (!parent || !parentCtrl)
                return true;
        }
        if (entryHit && !entryHit->isMenuOpen()) {
            guictxtmenu* popup = createPopupForEntry(entryHit, lvl + 1);
            if (popup) {
                entryHit->setIsMenuOpen(true);
                popup->setLevel(this->getLevel() + 1);
                popup->size = size;
                popup->setFontSize(entryHit->fontSize);
                popup->size.x        = math::max(CONTEXT_MENU_MIN_WIDTH, popup->size.x);
                auto popupPosRelCtrl = toScreenSpace(ivec2(right() + 2, top() + entryHit->y));
                auto appCtrlParent   = parentCtrl->getParentCtrl();
                appCtrlParent->openAppMenu(popup->getLevel(), popup, parentCtrl->toScreenSpace(popupPosRelCtrl), WINDOW_IS_BORDERLESS | WINDOW_POS_ABSOLUTE);
            }
        }
        for (guibase* gui : guis) {
            if (!gui->isVisible())
                continue;
            if (gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (canMouseHit()) {
            evt.requestFocus(this);
            return true;
        }
    }
    return false;
}

void guictxtmenu::closeAllSubmenus() {
    auto appCtrlParent = parentCtrl->getParentCtrl();
    bool anyOpen       = false;
    for (ctxtmenu_entry* ctxtEntry : entries) {
        if (ctxtEntry) {
            anyOpen |= ctxtEntry->isMenuOpen();
            ctxtEntry->setIsMenuOpen(false);
        }
    }
    if (anyOpen) {
        /* close all menus deeper than this menu */
        appCtrlParent->closeAppMenusAtLvl(lvl + 1);
    }
}
String ngui::Menu::getTitle() const {
    return title;
}

String ngui::Menu::getRight() const {
    if (registeredCommand) {
        auto combo = registeredCommand->getFirstKeyCombo();
        if (combo && combo->keyCode != KeyboardKey::DAW_KB_INVALID) {
            return combo->toString();
        }
    }
    return "";
}

void ctxtmenu_entry::layout(ivec2 size, float _fontSize, determine_string_width& strw) {
    this->fontSize = _fontSize;
    this->height   = math::roundfS32(_fontSize * 1.1f);
    auto entryW    = leftOffset() + strw.getStringWidth(title, _fontSize, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    if (icon) entryW += height - 4;
    if (!rightTitle.empty()) entryW += strw.getStringWidth(rightTitle, _fontSize, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    this->width = math::max(size.x, math::roundfS32(entryW));
}

void ctxtmenu_entry::render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
    if (contains(ctxtSize, mouse)) {
        nvgBeginPath(vg);
        nvgRect(vg, 0, y, ctxtSize.x, height);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
        nvgFill(vg);
    }
    if (this->icon) {
        nvgTranslate(vg, height / 4, y + 2);
        drawIconColored(vg, ivec2(height - 4), icon, theme->getColor(iconColor), 4);
        nvgTranslate(vg, -height / 4, -(y + 2));
    }

    renderTextLabel(vg,
                    vec2(leftOffset(), y + height * 0.5f),
                    vec2(width, height),
                    title,
                    theme,
                    fontSize,
                    theme->getColor(bGrayedOut ? GuiColor::COL_LABEL_INACTIVE : GuiColor::COL_TEXT),
                    NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    String rightSide;
    if (this->rightTitle.length()) {
        rightSide = this->rightTitle;
    } else if (showSubmenuArrow()) {
        rightSide = ">";
    }
    if (rightSide.length()) {
        auto defoffset = this->fontSize / 2.4f;
        renderTextLabel(vg,
                        vec2(width - defoffset, y + height * 0.5f),
                        vec2(width, height),
                        rightSide,
                        theme,
                        fontSize,
                        THEMECOL_TEXT,
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    }
}

float ctxtmenu_entry::leftOffset() {
    if (fixedLeftOffset >= 0) {
        return fixedLeftOffset;
    }
    auto offset = this->fontSize / 2.4f;
    if (icon != nullptr) {
        offset += height;
    }
    return offset;
}
