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

#ifndef __PROJECTIONCOMPONENT_H__
#define __PROJECTIONCOMPONENT_H__

#include <openspace/properties/scalarproperty.h>
#include <openspace/util/spicemanager.h>

#include <ghoul/misc/dictionary.h>
#include <ghoul/opengl/texture.h>

namespace openspace {

class ProjectionComponent {
public:
    ProjectionComponent();

protected:
    bool initialize();
    bool deinitialize();

    bool initializeProjectionSettings(const ghoul::Dictionary& dictionary);
    bool initializeParser(const ghoul::Dictionary& dictionary);

    void imageProjectBegin();
    void imageProjectEnd();

    bool generateProjectionLayerTexture();
    bool auxiliaryRendertarget();

    std::unique_ptr<ghoul::opengl::Texture> loadProjectionTexture(const std::string& texturePath);
    glm::mat4 computeProjectorMatrix(
        const glm::vec3 loc, glm::dvec3 aim, const glm::vec3 up,
        const glm::dmat3& instrumentMatrix,
        float fieldOfViewY, 
        float aspectRatio,
        float nearPlane,
        float farPlane,
        glm::vec3& boreSight
    );

    void clearAllProjections();


    properties::BoolProperty _performProjection;
    properties::BoolProperty _clearAllProjections;
    properties::FloatProperty _projectionFading;

    std::unique_ptr<ghoul::opengl::Texture> _projectionTexture;

    std::string _instrumentID;
    std::string _projectorID;
    std::string _projecteeID;
    SpiceManager::AberrationCorrection _aberration;
    std::vector<std::string> _potentialTargets;
    float _fovy;
    float _aspectRatio;
    float _nearPlane;
    float _farPlane;


    GLuint _fboID;

    GLint _defaultFBO;
    GLint _viewport[4];
};

} // namespace openspace

#endif // __PROJECTIONCOMPONENT_H__