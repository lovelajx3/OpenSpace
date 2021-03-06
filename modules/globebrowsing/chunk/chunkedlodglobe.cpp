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

#include <modules/globebrowsing/chunk/chunkedlodglobe.h>

#include <modules/globebrowsing/meshes/skirtedgrid.h>
#include <modules/globebrowsing/chunk/culling.h>
#include <modules/globebrowsing/chunk/chunklevelevaluator.h>

#include <modules/debugging/rendering/debugrenderer.h>


// open space includes
#include <openspace/engine/openspaceengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/spicemanager.h>
#include <openspace/scene/scenegraphnode.h>

#include <openspace/util/time.h>

// ghoul includes
#include <ghoul/misc/assert.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <ctime>
#include <chrono>
namespace {
    const std::string _loggerCat = "ChunkLodGlobe";
}

namespace openspace {

    const ChunkIndex ChunkedLodGlobe::LEFT_HEMISPHERE_INDEX = ChunkIndex(0, 0, 1);
    const ChunkIndex ChunkedLodGlobe::RIGHT_HEMISPHERE_INDEX = ChunkIndex(1, 0, 1);

    const GeodeticPatch ChunkedLodGlobe::COVERAGE = GeodeticPatch(0, 0, 90, 180);


    ChunkedLodGlobe::ChunkedLodGlobe(
        const Ellipsoid& ellipsoid,
        size_t segmentsPerPatch,
        std::shared_ptr<TileProviderManager> tileProviderManager)
        : _ellipsoid(ellipsoid)
        , _leftRoot(std::make_unique<ChunkNode>(Chunk(this, LEFT_HEMISPHERE_INDEX)))
        , _rightRoot(std::make_unique<ChunkNode>(Chunk(this, RIGHT_HEMISPHERE_INDEX)))
        , minSplitDepth(2)
        , maxSplitDepth(22)
        , _savedCamera(nullptr)
        , _tileProviderManager(tileProviderManager)
        , stats(StatsCollector(absPath("test_stats"), 1, StatsCollector::Enabled::No))
    {

        auto geometry = std::make_shared<SkirtedGrid>(
            (unsigned int) segmentsPerPatch,
            (unsigned int) segmentsPerPatch,
            TriangleSoup::Positions::No,
            TriangleSoup::TextureCoordinates::Yes,
            TriangleSoup::Normals::No);

        _chunkCullers.push_back(std::make_unique<HorizonCuller>());
        _chunkCullers.push_back(std::make_unique<FrustumCuller>(AABB3(vec3(-1, -1, 0), vec3(1, 1, 1e35))));


        _chunkEvaluatorByAvailableTiles = std::make_unique<EvaluateChunkLevelByAvailableTileData>();
        _chunkEvaluatorByProjectedArea = std::make_unique<EvaluateChunkLevelByProjectedArea>();
        _chunkEvaluatorByDistance = std::make_unique<EvaluateChunkLevelByDistance>();

        _renderer = std::make_unique<ChunkRenderer>(geometry, tileProviderManager);

    }

    ChunkedLodGlobe::~ChunkedLodGlobe() {

    }

    bool ChunkedLodGlobe::initialize() {
        return isReady();
    }

    bool ChunkedLodGlobe::deinitialize() {
        return true;
    }

    bool ChunkedLodGlobe::isReady() const {
        bool ready = true;
        return ready;
    }

    std::shared_ptr<TileProviderManager> ChunkedLodGlobe::getTileProviderManager() const {
        return _tileProviderManager;
    }

    bool ChunkedLodGlobe::testIfCullable(const Chunk& chunk, const RenderData& renderData) const {
        if (debugOptions.doHorizonCulling && _chunkCullers[0]->isCullable(chunk, renderData)) {
            return true;
        }
        if (debugOptions.doFrustumCulling && _chunkCullers[1]->isCullable(chunk, renderData)) {
            return true;
        }
        return false;
    }

    const ChunkNode& ChunkedLodGlobe::findChunkNode(const Geodetic2 p) const {
        ghoul_assert(COVERAGE.contains(p), "Point must be in lat [-90, 90] and lon [-180, 180]");
        return p.lon < COVERAGE.center().lon ? _leftRoot->find(p) : _rightRoot->find(p);
    }

    ChunkNode& ChunkedLodGlobe::findChunkNode(const Geodetic2 p) {
        ghoul_assert(COVERAGE.contains(p), "Point must be in lat [-90, 90] and lon [-180, 180]");
        return p.lon < COVERAGE.center().lon ? _leftRoot->find(p) : _rightRoot->find(p);
    }

    int ChunkedLodGlobe::getDesiredLevel(const Chunk& chunk, const RenderData& renderData) const {
        int desiredLevel = 0;
        if (debugOptions.levelByProjAreaElseDistance) {
            desiredLevel = _chunkEvaluatorByProjectedArea->getDesiredLevel(chunk, renderData);
        }
        else {
            desiredLevel = _chunkEvaluatorByDistance->getDesiredLevel(chunk, renderData);
        }


        int desiredLevelByAvailableData = _chunkEvaluatorByAvailableTiles->getDesiredLevel(chunk, renderData);
        if (desiredLevelByAvailableData != ChunkLevelEvaluator::UNKNOWN_DESIRED_LEVEL) {
            desiredLevel = min(desiredLevel, desiredLevelByAvailableData);
        }

        desiredLevel = glm::clamp(desiredLevel, minSplitDepth, maxSplitDepth);
        return desiredLevel;
    }
    
    void ChunkedLodGlobe::render(const RenderData& data){

        stats.startNewRecord();
        
        int j2000s = Time::now().j2000Seconds();

        auto duration = std::chrono::system_clock::now().time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        stats.i["time"] = millis;

        minDistToCamera = INFINITY;

        _leftRoot->updateChunkTree(data);
        _rightRoot->updateChunkTree(data);

        // Calculate the MVP matrix
        dmat4 viewTransform = dmat4(data.camera.combinedViewMatrix());
        dmat4 vp = dmat4(data.camera.projectionMatrix()) * viewTransform;
        dmat4 mvp = vp * _modelTransform;

        // Render function
        std::function<void(const ChunkNode&)> renderJob = [this, &data, &mvp](const ChunkNode& chunkNode) {
            stats.i["chunks"]++;
            const Chunk& chunk = chunkNode.getChunk();
            if (chunkNode.isLeaf()){
                stats.i["chunks leafs"]++;
                if (chunk.isVisible()) {
                    stats.i["rendered chunks"]++;
                    double t0 = Time::now().j2000Seconds();
                    _renderer->renderChunk(chunkNode.getChunk(), data);
                    debugRenderChunk(chunk, mvp);
                }
            }
        };
        
        _leftRoot->reverseBreadthFirst(renderJob);
        _rightRoot->reverseBreadthFirst(renderJob);

        if (_savedCamera != nullptr) {
            DebugRenderer::ref().renderCameraFrustum(data, *_savedCamera);
        }

        //LDEBUG("min distnace to camera: " << minDistToCamera);

        Vec3 cameraPos = data.camera.position().dvec3();
        //LDEBUG("cam pos  x: " << cameraPos.x << "  y: " << cameraPos.y << "  z: " << cameraPos.z);

        //LDEBUG("ChunkNode count: " << ChunkNode::chunkNodeCount);
        //LDEBUG("RenderedPatches count: " << ChunkNode::renderedChunks);
        //LDEBUG(ChunkNode::renderedChunks << " / " << ChunkNode::chunkNodeCount << " chunks rendered");
    }


    void ChunkedLodGlobe::debugRenderChunk(const Chunk& chunk, const glm::dmat4& mvp) const {
        if (debugOptions.showChunkBounds || debugOptions.showChunkAABB) {
            const std::vector<glm::dvec4> modelSpaceCorners = chunk.getBoundingPolyhedronCorners();
            std::vector<glm::vec4> clippingSpaceCorners(8);
            AABB3 screenSpaceBounds;
            for (size_t i = 0; i < 8; i++) {
                const vec4& clippingSpaceCorner = mvp * modelSpaceCorners[i];
                clippingSpaceCorners[i] = clippingSpaceCorner;

                vec3 screenSpaceCorner = (1.0f / clippingSpaceCorner.w) * clippingSpaceCorner;
                screenSpaceBounds.expand(screenSpaceCorner);
            }

            unsigned int colorBits = 1 + chunk.index().level % 6;
            vec4 color = vec4(colorBits & 1, colorBits & 2, colorBits & 4, 0.3);

            if (debugOptions.showChunkBounds) {
                DebugRenderer::ref().renderNiceBox(clippingSpaceCorners, color);
            }

            if (debugOptions.showChunkAABB) {
                auto& screenSpacePoints = DebugRenderer::ref().verticesFor(screenSpaceBounds);
                DebugRenderer::ref().renderNiceBox(screenSpacePoints, color);
            }
        }
    }

    void ChunkedLodGlobe::update(const UpdateData& data) {
        glm::dmat4 translation = glm::translate(glm::dmat4(1.0), data.modelTransform.translation);
        glm::dmat4 rotation = glm::dmat4(data.modelTransform.rotation);
        glm::dmat4 scaling = glm::scale(glm::dmat4(1.0),
            glm::dvec3(data.modelTransform.scale, data.modelTransform.scale, data.modelTransform.scale));

        _modelTransform = translation * rotation * scaling;
        _inverseModelTransform = glm::inverse(_modelTransform);

        _renderer->update();
    }

    const glm::dmat4& ChunkedLodGlobe::modelTransform() {
        return _modelTransform;
    }

    const glm::dmat4& ChunkedLodGlobe::inverseModelTransform() {
        return _inverseModelTransform;
    }

    const Ellipsoid& ChunkedLodGlobe::ellipsoid() const
    {
        return _ellipsoid;
    }

}  // namespace openspace
