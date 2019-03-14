#include "guiscrollcontainer.h"


void guictr_scrollbar::render(NVGcontext* vg) {
	//		renderBackground(vg);
	nvgSave(vg);
	if (!setScissorTransform(vg)) {
		return;
	}
	ivec2 pos(0);
	ivec2 size(getSizeContent());
	nvgBeginPath(vg);
	nvgMoveTo(vg, pos.x + size.x, pos.y);
	nvgLineTo(vg, pos.x, pos.y);
	nvgLineTo(vg, pos.x, pos.y + size.y);
	nvgLineTo(vg, pos.x + size.x, pos.y + size.y);
	nvgLineTo(vg, pos.x + size.x, pos.y);
	nvgStrokeColor(vg, theme->getColor(COL_CTXTMNU_OUTLINE));
	nvgStrokeWidth(vg, 2);
	nvgStroke(vg);
	ivec2 ipos = pos + ivec2(1);
	ivec2 isize = size - ivec2(2);
	nvgBeginPath(vg);
	nvgRect(vg, ipos.x, ipos.y, isize.x, isize.y);
	nvgFillColor(vg, theme->getColor(COL_CTXTMNU_BG));
	nvgFill(vg);
	for (guibase* gui : guis) {
		if (gui == &scrollbar)
			continue;

		gui->render(vg);
	}
	nvgRestore(vg);
	if (scrollbar.isVisible()) {
		nvgSave(vg);
		nvgTranslate(vg, this->pos.x, this->pos.y);
		scrollbar.render(vg);
		nvgRestore(vg);
	}
}

void guictr_scrollbar::determineSize() {
	for (guibase* gui : guis) {
		if (gui == &scrollbar)
			continue;

		gui->size = size;
		gui->determineSize();
		gui->layout();
	}
	ivec2 maxSize = ivec2(0);
	for (guibase* gui : guis) {
		if (gui == &scrollbar)
			continue;

		maxSize.x = max(maxSize.x, gui->right());
		maxSize.y = max(maxSize.y, gui->bottom());
	}
	//maxSize += ivec2(insetCtxtMenu);
	size.x = std::max(maxSize.x, size.x);
	contentHeight = maxSize.y;
	const gui_scrollbar* bar = &scrollbar;
	if (maxHeight == -1) {
		hasScrollbar = maxSize.y > size.y;
	} else if (maxHeight > 0 && maxSize.y > maxHeight) {
		size.y = maxHeight - 5;
		hasScrollbar = true;
	} else {
		hasScrollbar = false;
		size.y = maxSize.y;
	}
	scrollbar.setVisible(hasScrollbar);
	scrollbar.parent = this;
	guis.erase(std::remove_if(guis.begin(), guis.end(), [bar](const guibase* x) {
		return x == bar;
	}), guis.end());
	if (hasScrollbar) {
		guis.insert(guis.begin(), &scrollbar);
		scrollbar.parent = this;
	}
	//		size.x = std::max(maxSize.x, size.x);
}

void guictr_scrollbar::onChildLayoutChanged(guibase* g) {
	determineSize();
	layout();
	if (this->parent != NULL) {
		this->parent->onChildLayoutChanged(this);
	}
}
void guictr_scrollbar::layout() {
	//		hasScrollbar = false;
	//		if (maxSize)
	ivec2 cs = getSizeContent();
	int scrollW = gui_scrollbar::defaultW;
	if (hasScrollbar) {
		if (scrollbarOutside) {
			scrollW = gui_scrollbar::smallW;
			scrollbar.size = ivec2(scrollW - 2, cs.y - 2);
			scrollbar.pos = ivec2(cs.x, 1);
			size.x += scrollW + 2;
		} else {
			int entryW = cs.x - scrollW;
			scrollbar.size = ivec2(scrollW - 2, cs.y - 2);
			scrollbar.pos = ivec2(cs.x - scrollW + 1, 1);
			for (guibase* gui : guis) {
				if (gui == &scrollbar)
					continue;

				gui->size.x = min(entryW, gui->size.x);
			}
		}
		scrollOffsetChanged(1, scrollbar.scrollOffset);
	} else {
		for (guibase* gui : guis) {
			if (gui == &scrollbar)
				continue;
			gui->size.x = cs.x;
		}
		scrollOffsetChanged(1, 0);
	}
	for (guibase* gui : guis) {
		if (gui == &scrollbar)
			continue;

		gui->size = ivec2(cs.x, contentHeight);
		if (hasScrollbar) {
			int newRight = cs.x - scrollW;
			int32_t right = gui->right();
			if (right > newRight) {
				gui->size.x = std::max(10, newRight - gui->pos.x);
			}
		}
		gui->layout();
	}
}

bool guictr_scrollbar::mouseHitTest(ivec2 v, MouseHitEvt& evt) {
	if (this->contains(v)) {
		ivec2 localMouse = this->toContainerSpace(v);
		for (guibase* gui : guis) {
			if (gui == &scrollbar && !scrollbar.isVisible())
				continue;
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
	}
	return false;
}
