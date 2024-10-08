#include <nanovg.h>
#include "assert_dbg.h"
#include "guibackgroundimage.h"
#include "logging.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "guiglobals.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/gui.h"
#include "container.h"
#include "basectrl.h"
#include "color_util.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "renderresources.h"
#include "gui/controls/button.h"
#include "saferef.h"
#include "str_util.h"
#include "host/daw/mainctrl.h"

void guictr_base::setControl(BaseCtrl* parentCtrl) {
    guibase::setControl(parentCtrl);
    for (guibase* g : guis) {
        g->setControl(parentCtrl);
    }
}
void guictr_base::setParent(guibase* parent) {
    guibase::setParent(parent);
}
void guictr_base::onRemove() {
    // The derived class has to remove guis
    // removeGuis() will not be called here
}

void guictr_base::onAdded() {
}

void guictr_base::render(NVGcontext* vg) {
    if (!isVisible()) {
        log_printf("warning, skip rendering container with state !isVisible()\n");
        return;
    }
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    for (auto c : guis) {
        if (!c->isVisible()) {
            //log_printf("warning, skip rendering child container with state !isVisible()\n");
            continue;
        }
        if (c->size.x <= 0 || c->size.y <= 0) {
            // log_printf("warning, skip rendering child container %s with size <= 0 0\n", StringAsCStr(c->getClassName()));
            continue;
        }
        {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }
}

void guictr_base::renderContainerLabel(NVGcontext* vg) {
    if (isFlag(FLG_RENDER_LABEL) && label.length()) {
        auto titleHeight    = theme->get(GuiConstant::CONST_FONT_SIZE_CTR_LABEL);
        auto posInset = vec2(INSET_CTR_SPACING, 0) + vec2(getPosContent());
        if (isFlag(FLG_VERTICAL_LABEL)) {
        } else {
            auto bounds = getTextLabelBounds(vg,
                            posInset+vec2(titleHeight*0.5f),
                            label,
                            theme,
                            titleHeight*1.2f,
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            auto sizeInset = ivec2(math::min(getSizeContent().x, static_cast<int32_t>(bounds.x + titleHeight)), titleHeight);
            
            const auto bgColor = getOuterBackgroundColorFromState(getStateFlags());
            posInset.y -= titleHeight;
            nvgBeginPath(vg);
            nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
            nvgFillColor(vg, theme->getColor(bgColor));
            nvgFill(vg);
            renderTextLabel(vg,
                            posInset+vec2(titleHeight*0.5f),
                            bounds,
                            label,
                            theme,
                            titleHeight*1.2f,
                            theme->getContrastColor(bgColor),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);         
        }
    }
}

void guictr_base::renderBackground(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    // dbgassert(isBackgroundRendered());
    // bool focused = parentCtrl->isCtrOrChildFocused(this);
    drawBackground(vg, theme, getPosContent(), getSizeContent(), margin, isBackgroundRenderedInset());
    renderContainerLabel(vg);
    /* render debug background if gui flag 1<<16 is set */
    if ((this->id & (1 << 16)) && size.x > 0 && size.y > 0) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, rgbaToNvg(0x7fff00ff));
        nvgFill(vg);
    }
    if (dawCtrl && safeRefGet(dawCtrl->getDragDropTarget().target) == this) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, rgbaToNvg(0x3fdddd33));
        nvgFill(vg);
    }
}

void guictr_base::renderFrameBase(NVGcontext* vg) {
    ivec2 sizeContent = size;
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, sizeContent.x, sizeContent.y);
    nvgFillColor(vg, theme->getFrameColorBase());
    nvgFill(vg);
}

void guictr_base::renderFrameOutline(NVGcontext* vg) {
    ivec2 sizeContent = size;
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, sizeContent.x, sizeContent.y);
    nvgStrokeColor(vg, theme->getFrameColorOutline());
    nvgStrokeWidth(vg, 2.0);
    nvgStroke(vg);
}
uint32_t guictr_base::getTitlebarColorFromState(int32_t flags) {
    uint32_t c = 0;
    if (flags & TITLEBAR_FLG_SELECTED) {
        c = theme->getColorInt32(GuiColor::COL_PLUG_TITLE_SELECTED);
    } else if (flags & TITLEBAR_FLG_FOCUSED) {
        c = theme->getColorInt32(GuiColor::COL_PLUG_TITLE_FOCUSED);
    } else {
        c = theme->getColorInt32(GuiColor::COL_PLUG_TITLE);
    }
    return c;
}
void guictr_base::renderTitleBar(NVGcontext* vg, const ivec2& sizeContent, String text, const GuiConstant::constant_t& constantHeight, float textOffsetX, int flags, bool isHorizontalTitle) {
    auto colorU32 = getTitlebarColorFromState(flags);
    const auto hpt = theme->get(constantHeight);
    if (hpt <= 0) {
        return;
    }
    nvgBeginPath(vg);
    float textMaxWidth;
    ivec2 posInset  = getPosContent();
    if (isHorizontalTitle) {
        nvgRect(vg, 0, 0, sizeContent.x, hpt);
        textMaxWidth = size.x - textOffsetX;
        for (auto* gui : guis) {
            if (gui->isVisible() && gui->top()+posInset.y < hpt && gui->bottom()+posInset.y > 0) {
                if (gui->left() > textOffsetX && gui->left() <= textOffsetX+textMaxWidth) {
                    textMaxWidth = math::min<float>(textMaxWidth, gui->left() - textOffsetX);
                }
            }
        }
    } else {
        nvgRect(vg, 0, 0, hpt, sizeContent.y);
        textMaxWidth = textOffsetX - (INSET_TITLE * 2.0f);
        for (auto* gui : guis) {
            if (gui->isVisible() && gui->left()+posInset.x < hpt && gui->right()+posInset.x > 0) {
                if (gui->bottom() < textOffsetX) {
                    textMaxWidth = math::min<float>(textMaxWidth, math::max(0.0f, textOffsetX - (gui->bottom() + INSET_TITLE)));
                }
            }
        }
    }
    nvgFillColor(vg, rgbaToNvg(colorU32));
    nvgFill(vg);
    if (textMaxWidth - 5 <= 0) {
        return;
    }
    if (text[0]) {
        const auto fontScaled = hpt * 0.8f;
        if (isHorizontalTitle) {
            nvgSave(vg);
            nvgIntersectScissor(vg, textOffsetX + INSET_TITLE - 1, 0, textMaxWidth + 2, hpt);
            renderText(vg, vec2(textOffsetX + INSET_TITLE, hpt * 0.5f), vec2(textMaxWidth, hpt), text, fontScaled);
            nvgRestore(vg);
        } else {
            nvgSave(vg);
            nvgTranslate(vg, hpt / 2, textOffsetX);
            nvgRotate(vg, (float) (-M_PI / 2.0));
            nvgIntersectScissor(vg, INSET_TITLE * 2 - 1, -hpt / 2, textMaxWidth, hpt);
            renderText(vg, vec2(INSET_TITLE * 2, 0), vec2(textMaxWidth, hpt), text, fontScaled);
            nvgRestore(vg);
        }
    }
}

void guictr_base::drawInsetBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset) {
    if (!isRenderableSizeAndContext(vg))
        return;
    if (sizeInset.y > 0 && sizeInset.x > 0) {
        nvgBeginPath(vg);
        nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_BRT));
        nvgFill(vg);
    }
}

void guictr_base::drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool drawInset) {
    const ivec2 borderThickness(theme?theme->get(GuiConstant::CONST_BORDER_WIDTH):32);
    posInset -= ivec2(margin);
    sizeInset += ivec2(margin) * 2;
    if (sizeInset.y > 0 && sizeInset.x > 0) {
        nvgTranslateZ(vg, -2.0f);
        auto stateflags = getStateFlags();
        nvgShapeAntiAlias(vg, 0);
        if (this == parentCtrl->getGuiCtrFocused()) {
            nvgBeginPath(vg);
            nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
            NVGcolor bg = theme->getColor(getOuterBackgroundColorFromState(stateflags));

            nvgFillColor(vg, bg);
            nvgFill(vg);
            nvgTranslateZ(vg, -2.0f);
            posInset += borderThickness;
            sizeInset -= borderThickness * 2;
            if (sizeInset.y > 0 && sizeInset.x > 0 && drawInset) {
                nvgBeginPath(vg);
                nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
                nvgFillColor(vg, theme->getColor(getInnerBackgroundColorFromState(stateflags)));
                nvgFill(vg);
            }
        } else {
            nvgBeginPath(vg);
            nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
            nvgFillColor(vg, theme->getColor(getInnerBackgroundColorFromState(stateflags)));
            nvgFill(vg);
        }
        nvgTranslateZ(vg, 3.0f);
    }
    nvgShapeAntiAlias(vg, USE_NANOVG_AA);
}

bool guictr_base::setScissorTransformContainer(NVGcontext* vg) {
    ivec2 posInset  = getPosContent();
    ivec2 sizeInset = getSizeContent();
    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return false;
    }
    //nvgBeginPath(vg);
    //nvgRect(vg, pos.x, pos.y, size.x, size.y);
    //nvgFillColor(vg, rgbfToNvg(0xff3300, 0.3f));
    //nvgFill(vg);
    nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
    nvgTranslate(vg, posInset.x, posInset.y);
    nvgTranslateZ(vg, -4.0f);
    return true;
}

bool guictr_base::isRenderableSizeAndContext(NVGcontext* vg) {
    ivec2 sizeInset = getSizeContent();
    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return false;
    }
    return true;
}
bool guictr_base::setScissorTransform(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg)) {
        return false;
    }
    ivec2 posInset  = getPosContent();
    ivec2 sizeInset = getSizeContent();
    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return false;
    }
    int expand = 1;
    nvgIntersectScissor(vg, posInset.x - expand, posInset.y - expand, sizeInset.x + expand * 2, sizeInset.y + expand * 2);
    nvgTranslate(vg, posInset.x, posInset.y);
    nvgTranslateZ(vg, -4.0f);
    return true;
}

void guictr_base::scissorClip(ivec2& vpos, ivec2& vsize) {
    ivec2 posTL     = toParentSpace(vpos);
    ivec2 posBR     = toParentSpace(vpos + vsize);
    ivec2 posCnt    = getPosContent();
    ivec2 sizeCnt   = getSizeContent();
    ivec2 posBRThis = posCnt + sizeCnt;
    vpos.x          = math::max(posTL.x, posCnt.x);
    vpos.y          = math::max(posTL.y, posCnt.y);
    vsize.x         = math::min(posBR.x, posBRThis.x) - vpos.x;
    vsize.y         = math::min(posBR.y, posBRThis.y) - vpos.y;
    if (parent != nullptr) {
        parent->scissorClip(vpos, vsize);
    }
    vpos = toContainerSpace(vpos);
}

template<typename T>
void addPropertiesFromGui(T& gui, Table::tbl* table);

template<>
void addPropertiesFromGui(guictr_base& gui, Table::tbl* table);

void guictr_base::addProperties(Table::tbl* table) {
    addPropertiesFromGui(*this, table);
}

void guictr_vert_layout::layout() {
    auto cs = getSizeContent();
    vec2 xy{};
    // int32_t numEntries = CtrSize(layouts);

    for (auto& entry : layouts) {
        auto entrySize = vec2(cs);
        
        if (entry.scale < 0) {
            entrySize[dir] = -entry.scale;
        } else {
            entrySize[dir] *= entry.scale;
        }
        entry.gui->size = math::roundvecS32(entrySize);
        entry.gui->pos  = math::roundvecS32(xy);
        xy[dir]         = (entry.gui->pos[dir] + entry.gui->size[dir]);
    }
    // for (int32_t idx = 1; idx < numEntries - 1; ++idx) {
    //     auto& entry = layouts[idx];
    //     entry.gui->size[dir] -= layoutPadding[dir] + (numEntries - 1 == idx || idx == 0 ? 0 : layoutPadding[dir]);
    //     if (idx > 0)
    //         entry.gui->pos[dir] += layoutPadding[dir];
    //     // entry.gui->size[1-dir] -= 2*layoutPadding[1-dir];
    //     // entry.gui->pos[1-dir]  += layoutPadding[1-dir];
    // }
    guictr_base::layout();
}

void guictr_base::layoutEntries(ivec2 pos, ivec2 cs, ivec2 dir) {
    int32_t numEntries = 0;
    for (guibase* gui : guis) {
        auto f = gui->getFlags();
        if (f & FLG_NO_LAYOUT || !(f & FLG_VISIBLE))
            continue;
        numEntries++;
    }
    if (numEntries == 0)
        return;

    vec2 sizePadded = (vec2(cs) - vec2(dir) * float((numEntries - 1) * padding));
    vec2 entrySize  = sizePadded / vec2(dir.x ? numEntries : 1, dir.y ? numEntries : 1);
    vec2 offsetPos  = pos;
    for (guibase* gui : guis) {
        auto f = gui->getFlags();
        if (f & FLG_NO_LAYOUT || !(f & FLG_VISIBLE))
            continue;
        gui->pos  = math::roundvecS32(offsetPos);
        gui->size = math::roundvecS32(entrySize);
        offsetPos += (entrySize + vec2(padding)) * vec2(dir);
    }
}
void guictr_base::layoutEntriesGrid(ivec2 pos, ivec2 cs, int32_t maxCols) {
    int32_t numEntries = 0;
    for (guibase* gui : guis) {
        auto f = gui->getFlags();
        if (f & FLG_NO_LAYOUT || !(f & FLG_VISIBLE))
            continue;
        numEntries++;
    }
    if (numEntries == 0)
        return;
    auto numRows = (numEntries + maxCols - 1) / maxCols;
    auto numCols = math::min(numEntries, maxCols);
    if (numRows > 1) {
        cs.y -= padding * (numRows - 1);
    }
    if (numCols > 1) {
        cs.x -= padding * (numCols - 1);
    }
    auto entrySize = vec2(cs) / vec2(numCols, numRows);
    vec2 offsetPos = pos;
    int32_t col    = 0;
    for (guibase* gui : guis) {
        auto f = gui->getFlags();
        if (f & FLG_NO_LAYOUT || !(f & FLG_VISIBLE))
            continue;
        gui->pos  = math::roundvecS32(offsetPos);
        gui->size = math::roundvecS32(entrySize);
        col++;
        if (col >= numCols) {
            col         = 0;
            offsetPos.x = pos.x;
            offsetPos.y += entrySize.y + padding;
        } else {
            offsetPos.x += entrySize.x + padding;
        }
    }
}

void guictr_base::layout() {
    switch (layoutMode) {
        case LAYOUT_NONE:
            break;
        case LAYOUT_HORIZONTAL:
            layoutEntries({}, getSizeContent(), { 1, 0 });
            break;
        case LAYOUT_VERTICAL:
            layoutEntries({}, getSizeContent(), { 0, 1 });
            break;
        case LAYOUT_GRID:
            layoutEntriesGrid({}, getSizeContent(), 4);
            break;
        case LAYOUT_STACK:
            layoutEntries({}, getSizeContent(), { 0, 0 });
            break;
        default:
            dbgassert(0);
    }
    for (guibase* gui : guis) {
        gui->layout();
    }
}

void guictr_base::determineSize(ivec2& prefSize) {
    if (prefSize.x == 0 && prefSize.y == 0) {
        auto padding2 = paddingBR(padding) + paddingTL(padding);
        ivec2 maxSize = ivec2(0);
        for (guibase* gui : guis) {
            if (!gui->isVisible())
                continue;
            maxSize.x = math::max(maxSize.x, gui->right());
            maxSize.y = math::max(maxSize.y, gui->bottom());
        }
        if (maxSize.x > 0 && maxSize.y > 0) {
            prefSize = maxSize + padding2;
        }
    }
}

void container_background_image::render(guictr_base* ctr, NVGcontext* vg) const {
    auto sizeCtr = vec2(ctr->getSizeContent());
    int flags = NVG_IMAGE_GENERATE_MIPMAPS;
    if (layout == repeat) {
        flags |= NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY;
    }
    const RenderResources::NvgImageTexture* bgImage = RenderResources::loadTexture(vg, path, flags);
    if (bgImage) {
        nvgSave(vg);
        auto id = bgImage->perContextId.find(vg);
        dbgassert(id != bgImage->perContextId.end());
        NVGpaint paintIcon = nvgImagePattern(vg, 0, 0, bgImage->width, bgImage->height, 0, id->second, 1.0f);
        auto imgSize = vec2(bgImage->width, bgImage->height);
        paintIcon.innerColor = paintIcon.outerColor = rgbaToNvg(this->rgba);
        vec2 targetSize = (sizeCtr * scale);
        if (scaleAbsolute && scale.x > 0) {
            // scale to absolute pixels (width)
            targetSize.x = scale.x;
            targetSize.y = imgSize.y * (targetSize.x / imgSize.x);
        } else if (scaleAbsolute && scale.y > 0) {
            // scale to absolute pixels (height)
            targetSize.y = scale.y;
            targetSize.x = imgSize.x * (targetSize.y / imgSize.y);
        }
        auto scale = (targetSize / imgSize);
        auto minScale = math::min(scale.x, scale.y);
        vec2 offset(0);
        if (layout == position) {
            if (horizontalPos == left) {
                offset.x = 0;
            } else if (horizontalPos == center) {
                offset.x = (sizeCtr.x - imgSize.x * minScale) / 2.0f;
            } else if (horizontalPos == right) {
                offset.x = sizeCtr.x - imgSize.x * minScale;
            }
            if (verticalPos == top) {
                offset.y = 0;
            } else if (verticalPos == middle) {
                offset.y = (sizeCtr.y - imgSize.y * minScale) / 2.0f;
            } else if (verticalPos == bottom) {
                offset.y = sizeCtr.y - imgSize.y * minScale;
            }
            scale = vec2(minScale);
        } else if (layout == fill) {
            scale = sizeCtr / imgSize;
        } else if (layout == contain) {
            scale = vec2(minScale);
            offset.y = (sizeCtr.y - imgSize.y * minScale) / 2.0f;
            offset.x = (sizeCtr.x - imgSize.x * minScale) / 2.0f;
        } else if (layout == cover) {
            if (imgSize.x * sizeCtr.y > imgSize.y * sizeCtr.x) {
                scale = vec2(sizeCtr.y / imgSize.y);
                offset.x = (sizeCtr.x - imgSize.x * scale.x) / 2.0f;
            } else {
                scale = vec2(sizeCtr.x / imgSize.x);
                offset.y = (sizeCtr.y - imgSize.y * scale.y) / 2.0f;
            }
        }
        if (layout == repeat) {
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, sizeCtr.x, sizeCtr.y);
            nvgFillPaint(vg, paintIcon);
            nvgFill(vg);
        } else {
            nvgTranslate(vg, offset.x, offset.y);
            nvgScale(vg, scale.x, scale.y);
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, bgImage->width, bgImage->height);
            nvgFillPaint(vg, paintIcon);
            nvgFill(vg);
        }
        nvgRestore(vg);
    }
}

guictr_base::guictr_base(gui_type guiType)
    : guibase(guiType) {
    setBackgroundRendered(false);
    setBackgroundRenderedInset(true);
    setFlag(FLG_SUPPRESS_TOOLTIP, true);
}

bool guictr_base::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        // iterate over guis vector in reverse
        for (auto it = guis.rbegin(); it != guis.rend(); ++it) {
            auto gui = *it;
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) return false;
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_FILE) return false;
        if (evt.type == MouseHitType::MOUSE_SCROLL) {
            evt.requestFocus(this);
            return true;
        }
        if (canMouseHit()) {
            evt.requestFocus(this);
            return true;
        }
    }
    return false;
}

guibase* guictr_base::getFocusedContainer() {
    if (this->parent != nullptr && !this->isBackgroundRendered()) {
        if (this->parent->getGuiType() != gui_type::CTR_TYPE_LAYOUT) {
            return this->parent->getFocusedContainer();
        }
    }
    return this;
}
