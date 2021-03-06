/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014 - 2016                                                             *
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

#include <modules/volume/volumeutils.h>

namespace openspace {

template <typename VoxelType>
RawVolume<VoxelType>::RawVolume(const glm::ivec3& dimensions)
    : _dimensions(dimensions)
    , _data(static_cast<size_t>(dimensions.x) *
            static_cast<size_t>(dimensions.y) *
            static_cast<size_t>(dimensions.z)) {}

template <typename VoxelType>
glm::ivec3 RawVolume<VoxelType>::dimensions() const {
    return _dimensions;
}

template <typename VoxelType>
void RawVolume<VoxelType>::setDimensions(const glm::ivec3& dimensions) {
    _dimensions = dimensions;
    _data.resize(static_cast<size_t>(dimensions.x) *
                 static_cast<size_t>(dimensions.y) *
                 static_cast<size_t>(dimensions.z));
}

template <typename VoxelType>
VoxelType RawVolume<VoxelType>::get(const glm::ivec3& coordinates) const {
    return get(coordsToIndex(coordinates, dimensions()));
}

template <typename VoxelType>
VoxelType RawVolume<VoxelType>::get(size_t index) const {
    return _data[index];
}

template <typename VoxelType>
void RawVolume<VoxelType>::set(const glm::ivec3& coordinates, const VoxelType& value) {
    return set(coordsToIndex(coordinates, dimensions()), value);
}

template <typename VoxelType>
void RawVolume<VoxelType>::set(size_t index, const VoxelType& value) {
    _data[index] = value;
}

template <typename VoxelType>
void RawVolume<VoxelType>::forEachVoxel(
    const std::function<void(const glm::ivec3&, const VoxelType&)>& fn)
{
    
    glm::ivec3 dims = dimensions();
    size_t nVals = static_cast<size_t>(dims.x) *
        static_cast<size_t>(dims.y) *
        static_cast<size_t>(dims.z);
    
    for (size_t i = 0; i < nVals; i++) {
        glm::ivec3 coords = indexToCoords(i);
        fn(coords, _data[i]);
    }
}

template <typename VoxelType>
size_t RawVolume<VoxelType>::coordsToIndex(const glm::ivec3& cartesian) const {
    return volumeutils::coordsToIndex(cartesian, dimensions());
}

template <typename VoxelType>
glm::ivec3 RawVolume<VoxelType>::indexToCoords(size_t linear) const {
    return volumeutils::indexToCoords(linear, dimensions());
}

template <typename VoxelType>
VoxelType* RawVolume<VoxelType>::data() {
    return _data.data();
}
    
}
