/*****************************************************************************************
*                                                                                       *
* OpenSpace                                                                             *
*                                                                                       *
* Copyright (c) 2014                                                                    *
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

// open space includes
#include <openspace/rendering/planets/RenderablePlanetProjection.h>
#include <openspace/util/constants.h>
#include <openspace/rendering/planets/planetgeometryprojection.h>

#include <ghoul/opengl/texturereader.h>
#include <ghoul/opengl/textureunit.h>
#include <ghoul/filesystem/filesystem.h>

#include <openspace/util/time.h>
#include <openspace/util/spicemanager.h>

#include <openspace/engine/openspaceengine.h>
#include <sgct.h>
#include <iomanip> 

#define _USE_MATH_DEFINES
#include <math.h>

namespace {
const std::string _loggerCat = "RenderablePlanetProjection";
}

namespace openspace {

RenderablePlanetProjection::RenderablePlanetProjection(const ghoul::Dictionary& dictionary)
    : Renderable(dictionary)
	, _colorTexturePath("colorTexture", "Color Texture")
	, _projectionTexturePath("colorTexture", "Color Texture")
    , _programObject(nullptr)
    , _texture(nullptr)
	, _textureProj(nullptr)
    , _geometry(nullptr)
{
	std::string name;
	bool success = dictionary.getValue(constants::scenegraphnode::keyName, name);
	assert(success);

    std::string path;
    success = dictionary.getValue(constants::scenegraph::keyPathModule, path);
	assert(success);

    ghoul::Dictionary geometryDictionary;
    success = dictionary.getValue(
		constants::renderableplanet::keyGeometry, geometryDictionary);
	if (success) {
		geometryDictionary.setValue(constants::scenegraphnode::keyName, name);
        geometryDictionary.setValue(constants::scenegraph::keyPathModule, path);
		_geometry = planetgeometryprojection::PlanetGeometryProjection::createFromDictionary(geometryDictionary);
	}

	dictionary.getValue(constants::renderableplanet::keyFrame, _target);

    // TODO: textures need to be replaced by a good system similar to the geometry as soon
    // as the requirements are fixed (ab)
    std::string texturePath = "";
	success = dictionary.getValue("Textures.Color", texturePath);
	if (success){
		_colorTexturePath = path + "/" + texturePath; 
	}
	success = dictionary.getValue("Textures.Project", texturePath);
	if (success){
		_projectionTexturePath = path + "/" + texturePath;
	}
	addPropertySubOwner(_geometry);

	addProperty(_colorTexturePath);
	_colorTexturePath.onChange(std::bind(&RenderablePlanetProjection::loadTexture, this));
	addProperty(_projectionTexturePath);
	_projectionTexturePath.onChange(std::bind(&RenderablePlanetProjection::loadTexture, this));
}

RenderablePlanetProjection::~RenderablePlanetProjection(){
    deinitialize();
}

bool RenderablePlanetProjection::initialize(){
    bool completeSuccess = true;
    if (_programObject == nullptr)
        completeSuccess
              &= OsEng.ref().configurationManager().getValue("projectiveProgram", _programObject);

    loadTexture();
    completeSuccess &= (_texture != nullptr);
	completeSuccess &= (_textureProj != nullptr);

    completeSuccess &= _geometry->initialize(this);

    return completeSuccess;
}

bool RenderablePlanetProjection::deinitialize(){
    _geometry->deinitialize();
    delete _geometry;
    _geometry = nullptr;
    delete _texture;
    _texture = nullptr;
	delete _textureProj;
	_textureProj = nullptr;
    return true;
}

void RenderablePlanetProjection::render(const RenderData& data)
{
	if (!_programObject) return;
	if (!_textureProj) return;

    // activate shader
    _programObject->activate();

    // scale the planet to appropriate size since the planet is a unit sphere
    glm::mat4 transform = glm::mat4(1);
	
	//earth needs to be rotated for that to work.
	glm::mat4 rot = glm::rotate(transform, 90.f, glm::vec3(1, 0, 0));
		
	for (int i = 0; i < 3; i++){
		for (int j = 0; j < 3; j++){
			transform[i][j] = _stateMatrix[i][j];
		}
	}
	transform = transform* rot;
	if (_target == "IAU_JUPITER"){ // tmp scale of jupiterx = 0.935126
		transform *= glm::scale(glm::mat4(1), glm::vec3(1, 0.935126, 1));
	}

	// PROJECTIVE TEXTURING----------------------------------------------------------
	// get fov
	std::string shape, instrument;
	std::vector<glm::dvec3> bounds;
	glm::dvec3 boresight;
	bool found = openspace::SpiceManager::ref().getFieldOfView("NH_LORRI", shape, instrument, boresight, bounds);
	if (!found) LERROR("Could not locate instrument");

	psc position;
	double lightTime = 0.0;
	SpiceManager::ref().getTargetPosition("NEW HORIZONS", "JUPITER BARYCENTER","GALACTIC", "NONE", _time, position, lightTime);
	position[3] += 3;
	glm::vec3 nh_pos = position.vec3();

	//get up-vecto
	//rotate boresight into correct alignment
	glm::vec3 bsight(_instrumentMatrix*boresight); // lookat must be vec3 
	glm::vec3 uptmp(_instrumentMatrix*glm::dvec3(data.camera.lookUpVector()));

	//create view matrix
	glm::vec3 e3 = glm::normalize(bsight);
	glm::vec3 e1 = glm::normalize(glm::cross(uptmp, e3));
	glm::vec3 e2 = glm::normalize(glm::cross(e3, e1));
	
	glm::mat4 projViewMatrix = glm::mat4(     e1.x,                  e2.x,                  e3.x,        0.f,
								              e1.y,                  e2.y,                  e3.y,        0.f,
								              e1.z,                  e2.z,                  e3.z,        0.f, 
								    -glm::dot(e1, nh_pos), -glm::dot(e2, nh_pos), -glm::dot(e3, nh_pos), 1.f); 
	//create perspective projection matrix
	glm::mat4 projProjectionMatrix = glm::perspective(0.2907f, 1.f, 0.2f, 1000000.0f);
	//bias matrix
	glm::mat4 projNormalizationMatrix = glm::mat4(0.5f, 0   , 0   , 0,
												  0   , 0.5f, 0   , 0,
												  0   , 0   , 0.5f, 0,
		                                          0.5f, 0.5f, 0.5f, 1 );

	glm::mat4 m = projNormalizationMatrix*projProjectionMatrix*projViewMatrix;
    // setup the data to the shader
	_programObject->setUniform("ProjectorMatrix", m);
	_programObject->setUniform("ViewProjection", data.camera.viewProjectionMatrix());
	_programObject->setUniform("ModelTransform", transform);
	_programObject->setAttribute("boresight", bsight);
	setPscUniforms(_programObject, &data.camera, data.position);
	
    // Bind texture
    ghoul::opengl::TextureUnit unit;
    unit.activate();
    _texture->bind();
    _programObject->setUniform("texture1", unit); // jupiter

	ghoul::opengl::TextureUnit unit2;
	unit2.activate();
	_textureProj->bind();
	_programObject->setUniform("texture2", unit2); // proj

    // render
    _geometry->render();

    // disable shader
    _programObject->deactivate();
	
	static int callCount = 0;
	callCount++;

	if (callCount > 1000){
		callCount = 0;

		_textureProj->downloadTexture();
		_texture->downloadTexture();

		auto uvToModel = [](float u, float v, float radius[2], float fsegments)->glm::vec4{

			const float fj = u * fsegments;
			const float fi = v * fsegments;

			const float theta = fi * float(M_PI) / fsegments;  // 0 -> PI
			const float phi = fj * float(M_PI) * 2.0f / fsegments;

			const float x = radius[0] * sin(phi) * sin(theta);  //
			const float y = radius[0] * cos(theta);             // up
			const float z = radius[0] * cos(phi) * sin(theta);  //

			return glm::vec4(x, y, z, radius[1]);
		};

		auto uvToIndex = [](const glm::vec2 &uv, int w, int h, int &i, int &j){
			i = static_cast<int>(uv.x * float(w));
			j = static_cast<int>(uv.y * float(h));
		};

		auto inRange = [](int x, int a, int b)->bool{
			return (x >= a && x <= b);
		};

		auto pscToMeter = [](glm::vec4 v1, glm::vec2 v2)->glm::vec4{
			float factor = v2.x * pow(10, v2.y + v1.w);
			return glm::vec4(v1.xyz * factor, 1.0);
		};

		const float w = _texture->width();
		const float h = _texture->height();
		for (int i = 0; i < _texture->width(); ++i) {
			for (int j = 0; j < _texture->height(); ++j) {
				// "Shader code"

				// Texture coordinates
				float u = float(i) / w;
				float v = float(j) / h;

				// Psc scaling
				glm::vec2 scaling = data.camera.scaling();

				// Convert texture coordinates to model coordinates
				float radius[2] = { 0.71492f, 8.f };
				glm::vec4 in_position = uvToModel(u, v, radius, 200);

				// Convert psc to meters
				glm::vec4 raw_pos = pscToMeter(in_position, scaling);

				// Transform model coordinates to world coordinates
				glm::vec4 projected = m * transform  * raw_pos;

				// To do : use bilinear interpolation
				int x, y;
				glm::vec2 uv;
				uv.x = projected.x / projected.w;
				uv.y = projected.y / projected.w;
				uvToIndex(uv, _textureProj->width(), _textureProj->height(), x, y);

				if (inRange(x, 0, _textureProj->width() - 1) && inRange(y, 0, _textureProj->height() - 1)){

					_texture->texel<glm::detail::tvec3<glm::detail::uint8> >(i, j) = _textureProj->texel<glm::detail::tvec3<glm::detail::uint8>>(x, y);
				}
			}
		}

		// Upload textures
		//_textureProj->uploadTexture();
		_texture->uploadTexture();
	}
	

}

void RenderablePlanetProjection::update(const UpdateData& data){
	// set spice-orientation in accordance to timestamp
	_time = data.time;
	openspace::SpiceManager::ref().getPositionTransformMatrix(_target, "GALACTIC", data.time, _stateMatrix);
	openspace::SpiceManager::ref().getPositionTransformMatrix("NH_LORRI", "GALACTIC", data.time, _instrumentMatrix);

}

void RenderablePlanetProjection::loadTexture()
{
    delete _texture;
    _texture = nullptr;
    if (_colorTexturePath.value() != "") {
        _texture = ghoul::opengl::loadTexture(absPath(_colorTexturePath));
        if (_texture) {
            LDEBUG("Loaded texture from '" << absPath(_colorTexturePath) << "'");
			_texture->uploadTexture();

			// Textures of planets looks much smoother with AnisotropicMipMap rather than linear
			_texture->setFilter(ghoul::opengl::Texture::FilterMode::Linear);
        }
    }


	delete _textureProj;
	_textureProj = nullptr;
	if (_colorTexturePath.value() != "") {
		_textureProj = ghoul::opengl::loadTexture(absPath(_projectionTexturePath));
		if (_textureProj) {
			LDEBUG("Loaded texture from '" << absPath(_projectionTexturePath) << "'");
			_textureProj->uploadTexture();

			// Textures of planets looks much smoother with AnisotropicMipMap rather than linear
			_textureProj->setFilter(ghoul::opengl::Texture::FilterMode::Linear);
			_textureProj->setWrapping(ghoul::opengl::Texture::WrappingMode::ClampToBorder);

		}
	}
}

}  // namespace openspace
/*
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
*/