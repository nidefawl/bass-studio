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
    if (registeredCommand) {
        return GetMenuNameWithKeybind(title, registeredCommand->getFirstKeyCombo());
    }
    return title;
}
