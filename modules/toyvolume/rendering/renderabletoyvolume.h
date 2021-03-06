/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2016                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#ifndef __RENDERABLETOYVOLUME_H__
#define __RENDERABLETOYVOLUME_H__

#include <openspace/properties/vectorproperty.h>
#include <openspace/util/boxgeometry.h>
#include <openspace/util/blockplaneintersectiongeometry.h>

#include <openspace/rendering/renderable.h>
#include <modules/toyvolume/rendering/toyvolumeraycaster.h>

namespace openspace {

struct RenderData;
    
class RenderableToyVolume : public Renderable {
public:
    RenderableToyVolume(const ghoul::Dictionary& dictionary);
    ~RenderableToyVolume();
    
    bool initialize() override;
    bool deinitialize() override;
    bool isReady() const override;
    void render(const RenderData& data, RendererTasks& tasks) override;
    void update(const UpdateData& data) override;

private:
    properties::Vec3Property _scaling;
    properties::IntProperty _scalingExponent;
    properties::FloatProperty _stepSize;
    properties::Vec3Property _translation;
    properties::Vec3Property _rotation;
    properties::Vec4Property _color;
    
    std::unique_ptr<ToyVolumeRaycaster> _raycaster;
};
}

#endif // __RENDERABLETOYVOLUME_H__
