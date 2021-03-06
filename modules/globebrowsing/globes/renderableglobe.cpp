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

#include <modules/globebrowsing/globes/renderableglobe.h>

#include <ghoul/misc/threadpool.h>
#include <modules/globebrowsing/tile/tileselector.h>

#include <modules/globebrowsing/chunk/chunkedlodglobe.h>
#include <modules/globebrowsing/tile/tileprovidermanager.h>

// open space includes
#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/spicemanager.h>
#include <openspace/scene/scenegraphnode.h>

// ghoul includes
#include <ghoul/misc/assert.h>


namespace {
    const std::string _loggerCat = "RenderableGlobe";

    // Keys for the dictionary
    const std::string keyFrame = "Frame";
    const std::string keyRadii = "Radii";
    const std::string keyInteractionDepthBelowEllipsoid = "InteractionDepthBelowEllipsoid";
    const std::string keyCameraMinHeight = "CameraMinHeight";
    const std::string keySegmentsPerPatch = "SegmentsPerPatch";
    const std::string keyTextureInitData = "TextureInitData";
    const std::string keyTextures = "Textures";
    const std::string keyColorTextures = "ColorTextures";
    const std::string keyHeightMaps = "HeightMaps";
}



namespace openspace {


    RenderableGlobe::RenderableGlobe(const ghoul::Dictionary& dictionary)
        : _isEnabled(properties::BoolProperty("Enabled", "Enabled", true))
        , _toggleEnabledEveryFrame(properties::BoolProperty("Toggle enabled every frame", "Toggle enabled every frame", false))
        , _saveOrThrowCamera(properties::BoolProperty("saveOrThrowCamera", "saveOrThrowCamera"))
        , _resetTileProviders(properties::BoolProperty("resetTileProviders", "resetTileProviders"))
        , _cameraMinHeight(properties::FloatProperty("cameraMinHeight", "cameraMinHeight", 100.0f, 0.0f, 1000.0f))
        , lodScaleFactor(properties::FloatProperty("lodScaleFactor", "lodScaleFactor", 10.0f, 1.0f, 50.0f))
        , debugSelection(ReferencedBoolSelection("Debug", "Debug"))
        , atmosphereEnabled(properties::BoolProperty("Atmosphere", "Atmosphere", false))
    {
        setName("RenderableGlobe");
        
        dictionary.getValue(keyFrame, _frame);

        // Read the radii in to its own dictionary
        Vec3 radii;
        dictionary.getValue(keyRadii, radii);
        _ellipsoid = Ellipsoid(radii);
        setBoundingSphere(pss(_ellipsoid.averageRadius(), 0.0));

        // Ghoul can't read ints from lua dictionaries
        double patchSegmentsd;
        dictionary.getValue(keySegmentsPerPatch, patchSegmentsd);
        int patchSegments = patchSegmentsd;
        
        dictionary.getValue(keyInteractionDepthBelowEllipsoid, _interactionDepthBelowEllipsoid);
        float cameraMinHeight;
        dictionary.getValue(keyCameraMinHeight, cameraMinHeight);
        _cameraMinHeight.set(cameraMinHeight);

        // Init tile provider manager
        ghoul::Dictionary textureInitDataDictionary;
        ghoul::Dictionary texturesDictionary;
        dictionary.getValue(keyTextureInitData, textureInitDataDictionary);
        dictionary.getValue(keyTextures, texturesDictionary);

        _tileProviderManager = std::make_shared<TileProviderManager>(
            texturesDictionary, textureInitDataDictionary);

        _chunkedLodGlobe = std::make_shared<ChunkedLodGlobe>(
            _ellipsoid, patchSegments, _tileProviderManager);
        _distanceSwitch.addSwitchValue(_chunkedLodGlobe, 1e12);

        addProperty(_isEnabled);
        addProperty(_toggleEnabledEveryFrame);


        // Add debug options - must be after chunkedLodGlobe has been created as it 
        // references its members
        addProperty(debugSelection);
        debugSelection.addOption("Show chunk edges", &_chunkedLodGlobe->debugOptions.showChunkEdges);
        debugSelection.addOption("Show chunk bounds", &_chunkedLodGlobe->debugOptions.showChunkBounds);
        debugSelection.addOption("Show chunk AABB", &_chunkedLodGlobe->debugOptions.showChunkAABB);
        debugSelection.addOption("Show height resolution", &_chunkedLodGlobe->debugOptions.showHeightResolution);
        debugSelection.addOption("Show height intensities", &_chunkedLodGlobe->debugOptions.showHeightIntensities);

        debugSelection.addOption("Culling: Frustum", &_chunkedLodGlobe->debugOptions.doFrustumCulling);
        debugSelection.addOption("Culling: Horizon", &_chunkedLodGlobe->debugOptions.doHorizonCulling);

        debugSelection.addOption("Level by proj area (else distance)", &_chunkedLodGlobe->debugOptions.levelByProjAreaElseDistance);

        // Add all tile layers as being toggleable for each category
        for (int i = 0; i < LayeredTextures::NUM_TEXTURE_CATEGORIES;  i++){
            LayeredTextures::TextureCategory category = (LayeredTextures::TextureCategory) i;
            std::string categoryName = LayeredTextures::TEXTURE_CATEGORY_NAMES[i];
            auto selection = std::make_unique<ReferencedBoolSelection>(categoryName, categoryName);
            
            auto& categoryProviders = _tileProviderManager->getTileProviderGroup(category);
            for (auto& provider : categoryProviders.tileProviders) {
                selection->addOption(provider.name, &provider.isActive);
            }
            selection->addOption(" - Blend tile levels - ", &_tileProviderManager->getTileProviderGroup(i).levelBlendingEnabled);

            addProperty(selection.get());
            _categorySelections.push_back(std::move(selection));
        }

        addProperty(atmosphereEnabled);
        addProperty(_saveOrThrowCamera);
        addProperty(_resetTileProviders);
        addProperty(lodScaleFactor);
        addProperty(_cameraMinHeight);
    }

    RenderableGlobe::~RenderableGlobe() {

    }

    bool RenderableGlobe::initialize() {
        for (auto& selection : _categorySelections) {
            selection->initialize();
        }
        debugSelection.initialize();

        return _distanceSwitch.initialize();
    }

    bool RenderableGlobe::deinitialize() {
        return _distanceSwitch.deinitialize();
    }

    bool RenderableGlobe::isReady() const {
        return _distanceSwitch.isReady();
    }

    void RenderableGlobe::render(const RenderData& data) {
        if (_toggleEnabledEveryFrame.value()) {
            _isEnabled.setValue(!_isEnabled.value());
        }
        if (_isEnabled.value()) {
            if (_saveOrThrowCamera.value()) {
                _saveOrThrowCamera.setValue(false);

                if (_chunkedLodGlobe->getSavedCamera() == nullptr) { // save camera
                    LDEBUG("Saving snapshot of camera!");
                    _chunkedLodGlobe->setSaveCamera(std::make_shared<Camera>(data.camera));
                }
                else { // throw camera
                    LDEBUG("Throwing away saved camera!");
                    _chunkedLodGlobe->setSaveCamera(nullptr);
                }
            }
            _distanceSwitch.render(data);
        }
    }

    void RenderableGlobe::update(const UpdateData& data) {
        _time = data.time;
        _distanceSwitch.update(data);

        _chunkedLodGlobe->lodScaleFactor = lodScaleFactor.value();
        _chunkedLodGlobe->atmosphereEnabled = atmosphereEnabled.value();

        if (_resetTileProviders) {
            _tileProviderManager->reset();
            _resetTileProviders = false;
        }
        _tileProviderManager->update();
        _chunkedLodGlobe->update(data);
    }

    glm::dvec3 RenderableGlobe::projectOnEllipsoid(glm::dvec3 position) {
        return _ellipsoid.geodeticSurfaceProjection(position);
    }

    const Ellipsoid& RenderableGlobe::ellipsoid() {
        return _ellipsoid;
    }

    float RenderableGlobe::getHeight(glm::dvec3 position) {
        // Get the tile provider for the height map
        const auto& heightMapProviders = _tileProviderManager->getTileProviderGroup(LayeredTextures::HeightMaps).getActiveTileProviders();
        if (heightMapProviders.size() == 0)
            return 0;
        const auto& tileProvider = heightMapProviders[0];

        // Get the uv coordinates to sample from
        Geodetic2 geodeticPosition = _ellipsoid.cartesianToGeodetic2(position);
        int chunkLevel = _chunkedLodGlobe->findChunkNode(geodeticPosition).getChunk().index().level;
        
        ChunkIndex chunkIdx = ChunkIndex(geodeticPosition, chunkLevel);
        GeodeticPatch patch = GeodeticPatch(chunkIdx);
        Geodetic2 geoDiffPatch = patch.getCorner(Quad::NORTH_EAST) - patch.getCorner(Quad::SOUTH_WEST);
        Geodetic2 geoDiffPoint = geodeticPosition - patch.getCorner(Quad::SOUTH_WEST);
        glm::vec2 patchUV = glm::vec2(geoDiffPoint.lon / geoDiffPatch.lon, geoDiffPoint.lat / geoDiffPatch.lat);

        // Transform the uv coordinates to the current tile texture
        TileAndTransform tileAndTransform = TileSelector::getHighestResolutionTile(tileProvider.get(), chunkIdx);
        const auto& tile = tileAndTransform.tile;
        const auto& uvTransform = tileAndTransform.uvTransform;
        const auto& depthTransform = tileProvider->depthTransform();
        if (tile.status != Tile::Status::OK) {
            return 0;
        }
        glm::vec2 transformedUv = uvTransform.uvOffset + uvTransform.uvScale * patchUV;

        // Sample and do linear interpolation (could possibly be moved as a function in ghoul texture)
        glm::uvec3 dimensions = tile.texture->dimensions();
        
        glm::vec2 samplePos = transformedUv * glm::vec2(dimensions);
        glm::uvec2 samplePos00 = samplePos;
        samplePos00 = glm::clamp(samplePos00, glm::uvec2(0, 0), glm::uvec2(dimensions) - glm::uvec2(1));
        glm::vec2 samplePosFract = samplePos - glm::vec2(samplePos00);

        glm::uvec2 samplePos10 = glm::min(samplePos00 + glm::uvec2(1, 0), glm::uvec2(dimensions) - glm::uvec2(1));
        glm::uvec2 samplePos01 = glm::min(samplePos00 + glm::uvec2(0, 1), glm::uvec2(dimensions) - glm::uvec2(1));
        glm::uvec2 samplePos11 = glm::min(samplePos00 + glm::uvec2(1, 1), glm::uvec2(dimensions) - glm::uvec2(1));

        float sample00 = tile.texture->texelAsFloat(samplePos00).x;
        float sample10 = tile.texture->texelAsFloat(samplePos10).x;
        float sample01 = tile.texture->texelAsFloat(samplePos01).x;
        float sample11 = tile.texture->texelAsFloat(samplePos11).x;

        float sample0 = sample00 * (1.0 - samplePosFract.x) + sample10 * samplePosFract.x;
        float sample1 = sample01 * (1.0 - samplePosFract.x) + sample11 * samplePosFract.x;

        float sample = sample0 * (1.0 - samplePosFract.y) + sample1 * samplePosFract.y;

        // Perform depth transform to get the value in meters
        float height = depthTransform.depthOffset + depthTransform.depthScale * sample;
        
        // Return the result
        return height;
    }

    double RenderableGlobe::interactionDepthBelowEllipsoid() {
        return _interactionDepthBelowEllipsoid;
    }

    float RenderableGlobe::cameraMinHeight() {
        return _cameraMinHeight.value();
    }

    std::shared_ptr<ChunkedLodGlobe> RenderableGlobe::chunkedLodGlobe() {
        return _chunkedLodGlobe;
    }

}  // namespace openspace

