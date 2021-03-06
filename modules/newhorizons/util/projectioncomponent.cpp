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

#include <modules/newhorizons/util/projectioncomponent.h>

#include <modules/newhorizons/util/hongkangparser.h>
#include <modules/newhorizons/util/imagesequencer.h>
#include <modules/newhorizons/util/instrumenttimesparser.h>
#include <modules/newhorizons/util/labelparser.h>

#include <openspace/scene/scenegraphnode.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/io/texture/texturereader.h>
#include <ghoul/opengl/framebufferobject.h>
#include <ghoul/opengl/textureconversion.h>
#include <ghoul/systemcapabilities/openglcapabilitiescomponent.h>

namespace {
    const std::string keyPotentialTargets = "PotentialTargets";

    const std::string keyInstrument = "Instrument.Name";
    const std::string keyInstrumentFovy = "Instrument.Fovy";
    const std::string keyInstrumentAspect = "Instrument.Aspect";

    const std::string keyProjObserver = "Projection.Observer";
    const std::string keyProjTarget = "Projection.Target";
    const std::string keyProjAberration = "Projection.Aberration";

    const std::string keySequenceDir = "Projection.Sequence";
    const std::string keySequenceType = "Projection.SequenceType";
    const std::string keyTranslation = "DataInputTranslation";

    const std::string keyNeedsTextureMapDilation = "Projection.TextureMap";
    const std::string keyNeedsShadowing = "Projection.ShadowMap";
    const std::string keyTextureMapAspectRatio = "Projection.AspectRatio";

    const std::string sequenceTypeImage = "image-sequence";
    const std::string sequenceTypePlaybook = "playbook";
    const std::string sequenceTypeHybrid = "hybrid";
    const std::string sequenceTypeInstrumentTimes = "instrument-times";

    const std::string placeholderFile =
        "${OPENSPACE_DATA}/scene/common/textures/placeholder.png";

    const std::string _loggerCat = "ProjectionComponent";
}

namespace openspace {

using ghoul::Dictionary;
using glm::ivec2;

ProjectionComponent::ProjectionComponent()
    : properties::PropertyOwner()
    , _performProjection("performProjection", "Perform Projections", true)
    , _clearAllProjections("clearAllProjections", "Clear Projections", false)
    , _projectionFading("projectionFading", "Projection Fading", 1.f, 0.f, 1.f)
    , _textureSize("textureSize", "Texture Size", ivec2(16), ivec2(16), ivec2(32768))
    , _applyTextureSize("applyTextureSize", "Apply Texture Size")
    , _textureSizeDirty(false)
    , _projectionTexture(nullptr)
{
    setName("ProjectionComponent");

    _shadowing.isEnabled = false;
    _dilation.isEnabled = false;

    addProperty(_performProjection);
    addProperty(_clearAllProjections);
    addProperty(_projectionFading);

    addProperty(_textureSize);
    addProperty(_applyTextureSize);
    _applyTextureSize.onChange([this]() { _textureSizeDirty = true; });
}

bool ProjectionComponent::initialize() {
    int maxSize = OpenGLCap.max2DTextureSize();
    glm::ivec2 size;

    if (_projectionTextureAspectRatio > 1.f) {
        size.x = maxSize;
        size.y = static_cast<int>(maxSize / _projectionTextureAspectRatio);
    }
    else {
        size.x = static_cast<int>(maxSize * _projectionTextureAspectRatio);
        size.y = maxSize;
    }

    _textureSize.setMaxValue(size);
    _textureSize = size / 2;

    // We only want to use half the resolution per default:
    size /= 2;

    bool success = generateProjectionLayerTexture(size);
    success &= generateDepthTexture(size);
    success &= auxiliaryRendertarget();
    success &= depthRendertarget();

    using std::unique_ptr;
    using ghoul::opengl::Texture;
    using ghoul::io::TextureReader;

    unique_ptr<Texture> texture = TextureReader::ref().loadTexture(absPath(placeholderFile));
    if (texture) {
        texture->uploadTexture();
        // TODO: AnisotropicMipMap crashes on ATI cards ---abock
        //_textureProj->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);
        texture->setFilter(Texture::FilterMode::Linear);
        texture->setWrapping(Texture::WrappingMode::ClampToBorder);
    }
    _placeholderTexture = std::move(texture);
    
    if (_dilation.isEnabled) {
        _dilation.program = ghoul::opengl::ProgramObject::Build(
            "Dilation",
            "${MODULE_NEWHORIZONS}/shaders/dilation_vs.glsl",
            "${MODULE_NEWHORIZONS}/shaders/dilation_fs.glsl"
        );
        
        const GLfloat plane[] = {
            -1, -1,
            1,  1,
            -1,  1,
            -1, -1,
            1, -1,
            1,  1,
        };

        glGenVertexArrays(1, &_dilation.vao);
        glGenBuffers(1, &_dilation.vbo);

        glBindVertexArray(_dilation.vao);
        glBindBuffer(GL_ARRAY_BUFFER, _dilation.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(plane), plane, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(GLfloat) * 2,
            reinterpret_cast<void*>(0)
        );

        glBindVertexArray(0);
    }

    return success;
}

bool ProjectionComponent::deinitialize() {
    _projectionTexture = nullptr;

    glDeleteFramebuffers(1, &_fboID);

    if (_dilation.isEnabled) {
        glDeleteFramebuffers(1, &_dilation.fbo);
        glDeleteVertexArrays(1, &_dilation.vao);
        glDeleteBuffers(1, &_dilation.vbo);

        _dilation.program = nullptr;
        _dilation.texture = nullptr;
    }

    return true;
}

bool ProjectionComponent::isReady() const {
    return (_projectionTexture != nullptr);
}

bool ProjectionComponent::initializeProjectionSettings(const Dictionary& dictionary) {
    bool completeSuccess = true;
    completeSuccess &= dictionary.getValue(keyInstrument, _instrumentID);
    completeSuccess &= dictionary.getValue(keyProjObserver, _projectorID);
    completeSuccess &= dictionary.getValue(keyProjTarget, _projecteeID);
    completeSuccess &= dictionary.getValue(keyInstrumentFovy, _fovy);
    completeSuccess &= dictionary.getValue(keyInstrumentAspect, _aspectRatio);

    ghoul_assert(completeSuccess, "All neccessary attributes not found in modfile");

    std::string a = "NONE";
    bool s = dictionary.getValue(keyProjAberration, a);
    _aberration = SpiceManager::AberrationCorrection(a);
    completeSuccess &= s;
    ghoul_assert(completeSuccess, "All neccessary attributes not found in modfile");


    if (dictionary.hasKeyAndValue<ghoul::Dictionary>(keyPotentialTargets)) {
        ghoul::Dictionary potentialTargets = dictionary.value<ghoul::Dictionary>(
            keyPotentialTargets
        );

        _potentialTargets.resize(potentialTargets.size());
        for (int i = 0; i < potentialTargets.size(); ++i) {
            std::string target;
            potentialTargets.getValue(std::to_string(i + 1), target);
            _potentialTargets[i] = target;
        }
    }

    if (dictionary.hasKeyAndValue<bool>(keyNeedsTextureMapDilation)) {
        _dilation.isEnabled = dictionary.value<bool>(keyNeedsTextureMapDilation);
    }
    
    if (dictionary.hasKeyAndValue<bool>(keyNeedsShadowing)) {
        _shadowing.isEnabled = dictionary.value<bool>(keyNeedsShadowing);
    }

    _projectionTextureAspectRatio = 1.f;
    if (dictionary.hasKeyAndValue<double>(keyTextureMapAspectRatio)) {
        _projectionTextureAspectRatio = 
            static_cast<float>(dictionary.value<double>(keyTextureMapAspectRatio));
    }

    return completeSuccess;
}

bool ProjectionComponent::initializeParser(const ghoul::Dictionary& dictionary) {
    bool completeSuccess = true;

    std::string name;
    dictionary.getValue(SceneGraphNode::KeyName, name);

    std::vector<SequenceParser*> parsers;

    std::string sequenceSource;
    std::string sequenceType;
    bool foundSequence = dictionary.getValue(keySequenceDir, sequenceSource);
    if (foundSequence) {
        sequenceSource = absPath(sequenceSource);

        foundSequence = dictionary.getValue(keySequenceType, sequenceType);
        //Important: client must define translation-list in mod file IFF playbook
        if (dictionary.hasKey(keyTranslation)) {
            ghoul::Dictionary translationDictionary;
            //get translation dictionary
            dictionary.getValue(keyTranslation, translationDictionary);

            if (sequenceType == sequenceTypePlaybook) {
                parsers.push_back(new HongKangParser(
                    name, 
                    sequenceSource, 
                    _projectorID, 
                    translationDictionary, 
                    _potentialTargets));
            }
            else if (sequenceType == sequenceTypeImage) {
                parsers.push_back(new LabelParser(
                    name, 
                    sequenceSource, 
                    translationDictionary));
            }
            else if (sequenceType == sequenceTypeHybrid) {
                //first read labels
                parsers.push_back(new LabelParser(
                    name, 
                    sequenceSource, 
                    translationDictionary));

                std::string _eventFile;
                bool foundEventFile = dictionary.getValue("Projection.EventFile", _eventFile);
                if (foundEventFile) {
                    //then read playbook
                    _eventFile = absPath(_eventFile);
                    parsers.push_back(new HongKangParser(
                        name, 
                        _eventFile, 
                        _projectorID,
                        translationDictionary, 
                        _potentialTargets));
                }
                else {
                    LWARNING("No eventfile has been provided, please check modfiles");
                }
            }
            else if (sequenceType == sequenceTypeInstrumentTimes) {
                parsers.push_back(new InstrumentTimesParser(
                    name, 
                    sequenceSource, 
                    translationDictionary));
            }

            for(SequenceParser* parser : parsers){
                openspace::ImageSequencer::ref().runSequenceParser(parser);
                delete parser;
            }
        }
        else {
            LWARNING("No playbook translation provided, please make sure all spice calls match playbook!");
        }
    }

    return completeSuccess;
}

void ProjectionComponent::imageProjectBegin() {
    // keep handle to the current bound FBO
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_defaultFBO);

    if (_textureSizeDirty) {
        LDEBUG("Changing texture size to " << std::to_string(_textureSize));

        // If the texture size has changed, we have to allocate new memory and copy
        // the image texture to the new target

        using ghoul::opengl::Texture;
        using ghoul::opengl::FramebufferObject;

        // Make a copy of the old textures
        std::unique_ptr<Texture> oldProjectionTexture = std::move(_projectionTexture);
        std::unique_ptr<Texture> oldDilationStencil = std::move(_dilation.stencilTexture);
        std::unique_ptr<Texture> oldDilationTexture = std::move(_dilation.texture);
        std::unique_ptr<Texture> oldDepthTexture = std::move(_shadowing.texture);

        // Generate the new textures
        generateProjectionLayerTexture(_textureSize);

        if (_shadowing.isEnabled) { 
            generateDepthTexture(_textureSize);
        }

        auto copyFramebuffers = [](Texture* src, Texture* dst, const std::string& msg) {
            glFramebufferTexture(
                GL_READ_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                *src,
                0
            );

            GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
            if (!FramebufferObject::errorChecking(status).empty()) {
                LERROR(
                    "Read Buffer (" << msg << "): " <<
                    FramebufferObject::errorChecking(status)
                );
            }

            glFramebufferTexture(
                GL_DRAW_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                *dst,
                0
            );

            status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            if (!FramebufferObject::errorChecking(status).empty()) {
                LERROR(
                    "Draw Buffer (" << msg << "): " <<
                    FramebufferObject::errorChecking(status)
                );
            }

            glBlitFramebuffer(
                0, 0,
                src->dimensions().x, src->dimensions().y,
                0, 0,
                dst->dimensions().x, dst->dimensions().y,
                GL_COLOR_BUFFER_BIT,
                GL_LINEAR
            );
        };

        auto copyDepthBuffer = [](Texture* src, Texture* dst, const std::string& msg) {
            glFramebufferTexture(
                GL_READ_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                *src,
                0
            );

            GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
            if (!FramebufferObject::errorChecking(status).empty()) {
                LERROR(
                    "Read Buffer (" << msg << "): " <<
                    FramebufferObject::errorChecking(status)
                );
            }

            glFramebufferTexture(
                GL_DRAW_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                *dst,
                0
            );

            status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            if (!FramebufferObject::errorChecking(status).empty()) {
                LERROR(
                    "Draw Buffer (" << msg << "): " <<
                    FramebufferObject::errorChecking(status)
                );
            }

            glBlitFramebuffer(
                0, 0,
                src->dimensions().x, src->dimensions().y,
                0, 0,
                dst->dimensions().x, dst->dimensions().y,
                GL_DEPTH_BUFFER_BIT,
                GL_NEAREST
            );
        };

        GLuint fbos[2];
        glGenFramebuffers(2, fbos);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);

        copyFramebuffers(
            oldProjectionTexture.get(),
            _projectionTexture.get(),
            "Projection"
        );

        if (_dilation.isEnabled) {
            copyFramebuffers(
                oldDilationStencil.get(),
                _dilation.stencilTexture.get(),
                "Dilation Stencil"
            );

            copyFramebuffers(
                oldDilationTexture.get(),
                _dilation.texture.get(),
                "Dilation Texture"
            );
        }

        if (_shadowing.isEnabled) {
            copyDepthBuffer(
                oldDepthTexture.get(),
                _shadowing.texture.get(),
                "Shadowing"
            );
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDeleteFramebuffers(2, fbos);

        glBindFramebuffer(GL_FRAMEBUFFER, _fboID);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            *_projectionTexture,
            0
        );

        if (_dilation.isEnabled) {
            // We only need the stencil texture if we need to dilate
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT1,
                GL_TEXTURE_2D,
                *_dilation.stencilTexture,
                0
            );

            glBindFramebuffer(GL_FRAMEBUFFER, _dilation.fbo);
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D,
                *_dilation.texture,
                0
            );
        }

        if (_shadowing.isEnabled) {
            glBindFramebuffer(GL_FRAMEBUFFER, _depthFboID);
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D,
                *_shadowing.texture,
                0
            );
        }

        _textureSizeDirty = false;
    }

    glGetIntegerv(GL_VIEWPORT, _viewport);
    glBindFramebuffer(GL_FRAMEBUFFER, _fboID);

    glViewport(
        0, 0,
        static_cast<GLsizei>(_projectionTexture->width()),
        static_cast<GLsizei>(_projectionTexture->height())
    );

    if (_dilation.isEnabled) {
        GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, buffers);
    }
}

bool ProjectionComponent::needsShadowMap() const {
    return _shadowing.isEnabled;
}

ghoul::opengl::Texture& ProjectionComponent::depthTexture() {
    return *_shadowing.texture;
}

void ProjectionComponent::depthMapRenderBegin() {
    ghoul_assert(_shadowing.isEnabled, "Shadowing is not enabled");

    // keep handle to the current bound FBO
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_defaultFBO);
    glGetIntegerv(GL_VIEWPORT, _viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, _depthFboID);
    glEnable(GL_DEPTH_TEST);

    glViewport(
        0, 0,
        static_cast<GLsizei>(_shadowing.texture->width()),
        static_cast<GLsizei>(_shadowing.texture->height())
        );

    glClear(GL_DEPTH_BUFFER_BIT);
}

void ProjectionComponent::depthMapRenderEnd() {
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
    glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
}

void ProjectionComponent::imageProjectEnd() {
    if (_dilation.isEnabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, _dilation.fbo);

        glDisable(GL_BLEND);

        ghoul::opengl::TextureUnit unit[2];
        unit[0].activate();
        _projectionTexture->bind();

        unit[1].activate();
        _dilation.stencilTexture->bind();

        _dilation.program->activate();
        _dilation.program->setUniform("tex", unit[0]);
        _dilation.program->setUniform("stencil", unit[1]);

        glBindVertexArray(_dilation.vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        _dilation.program->deactivate();
    
        glEnable(GL_BLEND);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFBO);
    glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
}

void ProjectionComponent::update() {
    if (_dilation.isEnabled && _dilation.program->isDirty()) {
        _dilation.program->rebuildFromFile();
    }
}

bool ProjectionComponent::depthRendertarget() {
    GLint defaultFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);
    // setup FBO
    glGenFramebuffers(1, &_depthFboID);
    glBindFramebuffer(GL_FRAMEBUFFER, _depthFboID);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        *_shadowing.texture,
        0);

    glDrawBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
    return true;
}

bool ProjectionComponent::auxiliaryRendertarget() {
    bool completeSuccess = true;

    GLint defaultFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);

    // setup FBO
    glGenFramebuffers(1, &_fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, _fboID);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        *_projectionTexture,
        0
    );
    // check FBO status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LERROR("Main Framebuffer incomplete");
        completeSuccess &= false;
    }


    if (_dilation.isEnabled) {
        // We only need the stencil texture if we need to dilate
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT1,
            GL_TEXTURE_2D,
            *_dilation.stencilTexture,
            0
        );

        // check FBO status
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LERROR("Main Framebuffer incomplete");
            completeSuccess &= false;
        }

        glGenFramebuffers(1, &_dilation.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, _dilation.fbo);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            *_dilation.texture,
            0
        );

        // check FBO status
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LERROR("Dilation Framebuffer incomplete");
            completeSuccess &= false;
        }
    }

    // switch back to window-system-provided framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);

    return completeSuccess;
}

glm::mat4 ProjectionComponent::computeProjectorMatrix(const glm::vec3 loc, glm::dvec3 aim,
                                                      const glm::vec3 up,
                                                      const glm::dmat3& instrumentMatrix,
                                                      float fieldOfViewY,
                                                      float aspectRatio,
                                                      float nearPlane, float farPlane,
                                                      glm::vec3& boreSight)
{

    //rotate boresight into correct alignment
    boreSight = instrumentMatrix*aim;
    glm::vec3 uptmp(instrumentMatrix*glm::dvec3(up));

    // create view matrix
    glm::vec3 e3 = glm::normalize(-boreSight);
    glm::vec3 e1 = glm::normalize(glm::cross(uptmp, e3));
    glm::vec3 e2 = glm::normalize(glm::cross(e3, e1));

    glm::mat4 projViewMatrix = glm::mat4(e1.x, e2.x, e3.x, 0.f,
                                         e1.y, e2.y, e3.y, 0.f,
                                         e1.z, e2.z, e3.z, 0.f,
                                         glm::dot(e1, -loc), glm::dot(e2, -loc), glm::dot(e3, -loc), 1.f);
    // create perspective projection matrix
    glm::mat4 projProjectionMatrix = glm::perspective(glm::radians(fieldOfViewY), aspectRatio, nearPlane, farPlane);

    return projProjectionMatrix*projViewMatrix;
}

bool ProjectionComponent::doesPerformProjection() const {
    return _performProjection;
}

bool ProjectionComponent::needsClearProjection() const {
    return _clearAllProjections;
}

float ProjectionComponent::projectionFading() const {
    return _projectionFading;
}

ghoul::opengl::Texture& ProjectionComponent::projectionTexture() const {
    if (_dilation.isEnabled) {
        return *_dilation.texture;
    }
    else {
        return *_projectionTexture;
    }
}

std::string ProjectionComponent::projectorId() const {
    return _projectorID;
}

std::string ProjectionComponent::projecteeId() const {
    return _projecteeID;
}

std::string ProjectionComponent::instrumentId() const {
    return _instrumentID;
}

SpiceManager::AberrationCorrection ProjectionComponent::aberration() const {
    return _aberration;
}

float ProjectionComponent::fieldOfViewY() const {
    return _fovy;
}

float ProjectionComponent::aspectRatio() const {
    return _aspectRatio;
}

void ProjectionComponent::clearAllProjections() {
    // keep handle to the current bound FBO
    GLint defaultFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);

    GLint m_viewport[4];
    glGetIntegerv(GL_VIEWPORT, m_viewport);
    //counter = 0;
    glViewport(0, 0, static_cast<GLsizei>(_projectionTexture->width()), static_cast<GLsizei>(_projectionTexture->height()));

    glBindFramebuffer(GL_FRAMEBUFFER, _fboID);

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (_dilation.isEnabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, _dilation.fbo);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
    glViewport(m_viewport[0], m_viewport[1],
               m_viewport[2], m_viewport[3]);

    _clearAllProjections = false;
}

std::shared_ptr<ghoul::opengl::Texture> ProjectionComponent::loadProjectionTexture(
                                                           const std::string& texturePath,
                                                           bool isPlaceholder)
{
    using std::unique_ptr;
    using ghoul::opengl::Texture;
    using ghoul::io::TextureReader;


    if (isPlaceholder) {
        return _placeholderTexture;
    }


    unique_ptr<Texture> texture = TextureReader::ref().loadTexture(absPath(texturePath));
    if (texture) {
        if (texture->format() == Texture::Format::Red)
            ghoul::opengl::convertTextureFormat(ghoul::opengl::Texture::Format::RGB, *texture);
        texture->uploadTexture();
        // TODO: AnisotropicMipMap crashes on ATI cards ---abock
        //_textureProj->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);
        texture->setFilter(Texture::FilterMode::Linear);
        texture->setWrapping(Texture::WrappingMode::ClampToBorder);
    }
    return std::move(texture);
}

bool ProjectionComponent::generateProjectionLayerTexture(const ivec2& size) {
    LINFO(
        "Creating projection texture of size '" << size.x << ", " << size.y << "'"
    );
    _projectionTexture = std::make_unique<ghoul::opengl::Texture> (
        glm::uvec3(size, 1),
        ghoul::opengl::Texture::Format::RGBA
    );
    if (_projectionTexture) {
        _projectionTexture->uploadTexture();
        //_projectionTexture->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);
    }
    
    if (_dilation.isEnabled) {
        _dilation.texture = std::make_unique<ghoul::opengl::Texture>(
            glm::uvec3(size, 1),
            ghoul::opengl::Texture::Format::RGBA
        );

        if (_dilation.texture) {
            _dilation.texture->uploadTexture();
            //_dilation.texture->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);
        }

        _dilation.stencilTexture = std::make_unique<ghoul::opengl::Texture>(
            glm::uvec3(size, 1),
            ghoul::opengl::Texture::Format::Red,
            ghoul::opengl::Texture::Format::Red
        );

        if (_dilation.stencilTexture) {
            _dilation.stencilTexture->uploadTexture();
            //_dilation.texture->setFilter(ghoul::opengl::Texture::FilterMode::AnisotropicMipMap);
        }
    }


    return _projectionTexture != nullptr;

}

bool ProjectionComponent::generateDepthTexture(const ivec2& size) {
    LINFO(
        "Creating depth texture of size '" << size.x << ", " << size.y << "'"
        );

    _shadowing.texture = std::make_unique<ghoul::opengl::Texture>(
        glm::uvec3(size, 1),
        ghoul::opengl::Texture::Format::DepthComponent,
        GL_DEPTH_COMPONENT32F
        );

    if (_shadowing.texture) {
        _shadowing.texture->uploadTexture();
    }

    return _shadowing.texture != nullptr;

}

} // namespace openspace
