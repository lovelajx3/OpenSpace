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

#include <openspace/engine/logfactory.h>

#include <ghoul/misc/dictionary.h>
#include <ghoul/misc/exception.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/htmllog.h>
#include <ghoul/logging/textlog.h>

namespace {
    const std::string keyType = "Type";
    const std::string keyFilename = "File";
    const std::string keyAppend = "Append";
    const std::string keyTimeStamping = "TimeStamping";
    const std::string keyDateStamping = "DateStamping";
    const std::string keyCategoryStamping = "CategoryStamping";
    const std::string keyLogLevelStamping = "LogLevelStamping";

    const std::string valueHtmlLog = "html";
    const std::string valueTextLog = "Text";
}

namespace openspace {

std::unique_ptr<ghoul::logging::Log> createLog(const ghoul::Dictionary& dictionary) {
    std::string type;
    bool typeSuccess = dictionary.getValue(keyType, type);
    if (!typeSuccess) {
        throw ghoul::RuntimeError(
            "Requested log did not contain key '" + keyType + "'", "LogFactory"
        );
    }

    std::string filename;
    bool filenameSuccess = dictionary.getValue(keyFilename, filename);
    if (!filenameSuccess) {
        throw ghoul::RuntimeError(
            "Requested log did not contain key '" + keyFilename + "'", "LogFactory"
        );
    }
    filename = absPath(filename);

    bool append = true;
    dictionary.getValue(keyAppend, append);
    bool timeStamp = true;
    dictionary.getValue(keyTimeStamping, timeStamp);
    bool dateStamp = true;
    dictionary.getValue(keyDateStamping, dateStamp);
    bool categoryStamp = true;
    dictionary.getValue(keyCategoryStamping, categoryStamp);
    bool logLevelStamp = true;
    dictionary.getValue(keyLogLevelStamping, logLevelStamp);

    using Append = ghoul::logging::TextLog::Append;
    using TimeStamping = ghoul::logging::Log::TimeStamping;
    using DateStamping = ghoul::logging::Log::DateStamping;
    using CategoryStamping = ghoul::logging::Log::CategoryStamping;
    using LogLevelStamping = ghoul::logging::Log::LogLevelStamping;

    if (type == valueHtmlLog) {
        return std::make_unique<ghoul::logging::HTMLLog>(
            filename,
            append ? Append::Yes : Append::No,
            timeStamp ? TimeStamping::Yes : TimeStamping::No,
            dateStamp ? DateStamping::Yes : DateStamping::No,
            categoryStamp ? CategoryStamping::Yes : CategoryStamping::No,
            logLevelStamp ? LogLevelStamping::Yes : LogLevelStamping::No
        );
    }
    else if (type == valueTextLog) {
        return std::make_unique<ghoul::logging::TextLog>(
            filename,
            append ? Append::Yes : Append::No,
            timeStamp ? TimeStamping::Yes : TimeStamping::No,
            dateStamp ? DateStamping::Yes : DateStamping::No,
            categoryStamp ? CategoryStamping::Yes : CategoryStamping::No,
            logLevelStamp ? LogLevelStamping::Yes : LogLevelStamping::No
        );
    }
    else {
        throw ghoul::RuntimeError(
            "Log with type '" + type + "' did not name a valid log", "LogFactory"
        );
    }
}
    
} // namespace openspace
