#pragma once
#include <functional>
#include "shape.h"
#include "gui/container/container.h"

class i_ctr_shape_editor {
protected:
    ~i_ctr_shape_editor() = default;

public:
    i_ctr_shape_editor() = default;

public:
    virtual void setShapeEditorCallback(std::function<void(const DAW::Shape::shape_base_t&)> callback) = 0;
    virtual void setShapeEditorShapeRef(DAW::Shape::shape_t* shape) = 0;
    virtual guictr_base* getGuiContainer() = 0;
};

i_ctr_shape_editor* makeShapeEditor();