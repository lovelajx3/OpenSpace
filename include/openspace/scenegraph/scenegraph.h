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
 
#ifndef __SCENEGRAPH_H__
#define __SCENEGRAPH_H__

// std includes
#include <vector>
#include <map>
#include <set>
#include <mutex>

#include <openspace/util/camera.h>
#include <openspace/util/updatestructures.h>
#include <openspace/scripting/scriptengine.h>

#include <ghoul/opengl/programobject.h>
#include <ghoul/misc/dictionary.h>

namespace openspace {


class SceneGraphNode;

// Notifications:
// SceneGraphFinishedLoading
class SceneGraph {
public:
    // constructors & destructor
    SceneGraph();
    ~SceneGraph();

    /**
     * Initalizes the SceneGraph by loading modules from the ${SCENEPATH} directory
     */
    bool initialize();

    /*
     * Clean up everything
     */
    bool deinitialize();

    /*
     * Load the scenegraph from the provided folder
     */
    void scheduleLoadSceneFile(const std::string& sceneDescriptionFilePath);
	void clearSceneGraph();

    void loadModule(const std::string& modulePath);

    /*
     * Updates all SceneGraphNodes relative positions
     */
    void update(const UpdateData& data);

    /*
     * Evaluates if the SceneGraphNodes are visible to the provided camera
     */
    void evaluate(Camera* camera);

    /*
     * Render visible SceneGraphNodes using the provided camera
     */
    void render(const RenderData& data);

    /*
     * Returns the root SceneGraphNode
     */
    SceneGraphNode* root() const;

    /**
     * Return the scenegraph node with the specified name or <code>nullptr</code> if that
     * name does not exist
     */
    SceneGraphNode* sceneGraphNode(const std::string& name) const;

	std::vector<SceneGraphNode*> allSceneGraphNodes() const;

	/**
	 * Returns the Lua library that contains all Lua functions available to change the
	 * scene graph. The functions contained are
	 * - openspace::luascriptfunctions::property_setValue
	 * - openspace::luascriptfunctions::property_getValue
	 * \return The Lua library that contains all Lua functions available to change the
	 * scene graph
	 */
	static scripting::ScriptEngine::LuaLibrary luaLibrary();

private:
	bool loadSceneInternal(const std::string& sceneDescriptionFilePath);

    std::string _focus;

    // actual scenegraph
    SceneGraphNode* _root;
    std::vector<SceneGraphNode*> _nodes;
    std::map<std::string, SceneGraphNode*> _allNodes;

	std::string _sceneGraphToLoad;

	std::mutex _programUpdateLock;
	std::set<ghoul::opengl::ProgramObject*> _programsToUpdate;
	std::vector<ghoul::opengl::ProgramObject*> _programs;

    typedef std::map<std::string, ghoul::Dictionary> NodeMap;
    typedef std::multimap<std::string, std::string> DependencyMap;

    struct LoadMaps {
        NodeMap nodes;
        DependencyMap dependencies;
    };

    void loadModules(const std::string& directory, const ghoul::Dictionary& dictionary);
    void loadModule(LoadMaps& m,const std::string& modulePath);
    void loadNodes(const std::string parentName, const LoadMaps& m);
    void loadNode(const ghoul::Dictionary& dictionary);
};

} // namespace openspace

#endif // __SCENEGRAPH_H__
