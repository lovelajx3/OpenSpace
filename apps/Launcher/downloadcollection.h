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
 *  permit persons to whom the Software is furnished to do so, subject to the following   *
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

#ifndef __DOWNLOADCOLLECTION_H__
#define __DOWNLOADCOLLECTION_H__

#include <openspace/engine/downloadmanager.h>

#include <string>
#include <vector>

class DownloadCollection {
public:
    struct DirectFile {
        std::string module;
        std::string url;
        std::string destination;
    };

    struct FileRequest {
        std::string module;
        std::string identifier;
        std::string destination;
        int version;
    };

    struct TorrentFile {
        std::string module;
        std::string file;
        std::string destination;
    };

    struct Collection {
        std::vector<DirectFile> directFiles;
        std::vector<FileRequest> fileRequests;
        std::vector<TorrentFile> torrentFiles;
    };

    static Collection crawlScenes(const std::vector<std::string>& scenes);

    //static std::vector<openspace::DownloadManager::FileTask> downloadTasks(
    //    Collection collection, const openspace::DownloadManager& manager
    //);

    //static uint64_t id(const Collection& collection, const DirectFile& directFile);
    //static uint64_t id(const Collection& collection, const FileRequest& fileRequest);
    //static uint64_t id(const Collection& collection, const TorrentFile& torrentFile);

    //static const DirectFile& directFile(const Collection& collection, uint64_t id);
    //static const FileRequest& fileRequest(const Collection& collection, uint64_t id);
    //static const TorrentFile& torrentFile(const Collection& collection, uint64_t id);

};

#endif // __DOWNLOADCOLLECTION_H__
