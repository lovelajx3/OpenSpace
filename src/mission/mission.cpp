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

#include <openspace/mission/mission.h>

#include <assert.h>
#include <ghoul/filesystem/filesystem.h>
#include <openspace/util/spicemanager.h>
#include <openspace/mission/missionmanager.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/scripting/scriptengine.h>

#include <openspace/documentation/verifier.h>

namespace {
    const std::string _loggerCat = "MissionPhaseSequencer";

    const std::string KeyName = "Name";
    const std::string KeyDescription = "Description";
    const std::string KeyPhases = "Phases";
    const std::string KeyTimeRange = "TimeRange";
}

namespace openspace {


Documentation MissionPhase::Documentation() {
    using namespace documentation;

    return {
        "Missions and Mission Phases",
        "core_mission_mission",
        {
            {
                KeyName,
                new StringVerifier,
                "The human readable name of this mission or mission phase that is "
                "displayed to the user.",
                Optional::No
            },
            {
                KeyDescription,
                new StringVerifier,
                "A description of this mission or mission phase.",
                Optional::Yes
            },
            {
                KeyTimeRange,
                new ReferencingVerifier("core_util_timerange"),
                "The time range for which this mission or mission phase is valid. If no "
                "time range is specified, the ranges of sub mission phases are used "
                "instead.",
                Optional::Yes
            },
            {
                KeyPhases,
                new ReferencingVerifier("core_mission_mission"),
                "The phases into which this mission or mission phase is separated.",
                Optional::Yes
            }
        },
        Exhaustive::Yes
    };
}

MissionPhase::MissionPhase(const ghoul::Dictionary& dict) {
    const auto byPhaseStartTime = [](const MissionPhase& a, const MissionPhase& b)->bool{
        return a.timeRange().start < b.timeRange().start;
    };

    _name = dict.value<std::string>(KeyName);
    if (!dict.getValue(KeyDescription, _description)) {
        // If no description specified, just init to empty string
        _description = "";
    }
    
    ghoul::Dictionary childDicts;
    if (dict.getValue(KeyPhases, childDicts)) {
        // This is a nested mission phase
        _subphases.reserve(childDicts.size());
        for (size_t i = 0; i < childDicts.size(); ++i) {
            std::string key = std::to_string(i + 1);
            _subphases[i] = MissionPhase(childDicts.value<ghoul::Dictionary>(key));
        }

        // Ensure subphases are sorted
        std::stable_sort(_subphases.begin(), _subphases.end(), byPhaseStartTime);

        // Calculate the total time range of all subphases
        TimeRange timeRangeSubPhases;
        timeRangeSubPhases.start = _subphases[0].timeRange().start;
        timeRangeSubPhases.end = _subphases.back().timeRange().end;

        // user may specify an overall time range. In that case expand this timerange.
        ghoul::Dictionary timeRangeDict;
        if (dict.getValue(KeyTimeRange, timeRangeDict)) {
            TimeRange overallTimeRange(timeRangeDict);
            ghoul_assert(overallTimeRange.includes(timeRangeSubPhases),
                "User specified time range must at least include its subphases'");
            _timeRange.include(overallTimeRange);
        }
        else {
            // Its OK to not specify an overall time range, the time range for the 
            // subphases will simply be used. 
            _timeRange.include(timeRangeSubPhases);
        }
    }
    else {
        ghoul::Dictionary timeRangeDict;
        if (dict.getValue(KeyTimeRange, timeRangeDict)) {
            _timeRange = TimeRange(timeRangeDict); // throws exception if unable to parse
        }
        else {
            throw std::runtime_error("Must specify key: " + KeyTimeRange);
        }
    }
}

const std::string & MissionPhase::name() const {
    return _name;
}

const TimeRange & MissionPhase::timeRange() const {
    return _timeRange;
}

const std::string & MissionPhase::description() const {
    return _description;
}

/**
* Returns all subphases sorted by start time
*/

const std::vector<MissionPhase>& MissionPhase::phases() const {
    return _subphases;
}

/**
* Returns the i:th subphase, sorted by start time
*/

const MissionPhase& MissionPhase::phase(size_t i) const {
    return _subphases[i];
}

std::vector<const MissionPhase*> MissionPhase::phaseTrace(double time, int maxDepth) const {
    std::vector<const MissionPhase*> trace;
    if (_timeRange.includes(time)) {
        trace.push_back(this);
        phaseTrace(time, trace, maxDepth);
    }
    return std::move(trace);
}

bool MissionPhase::phaseTrace(double time, std::vector<const MissionPhase*>& trace, int maxDepth) const {
    if (maxDepth == 0) {
        return false;
    }

    for (int i = 0; i < _subphases.size(); ++i) {
        if (_subphases[i]._timeRange.includes(time)) {
            trace.push_back(&_subphases[i]);
            _subphases[i].phaseTrace(time, trace, maxDepth - 1);
            return true; // only add the first one
        }
        // Since time ranges are sorted we can do early termination
        else if (_subphases[i]._timeRange.start > time) {
            return false;
        }
    }
    return true;
}

Mission missionFromFile(std::string filename) {
    ghoul_assert(!filename.empty(), "filename must not be empty");
    ghoul_assert(!FileSys.containsToken(filename), "filename must not contain tokens");
    ghoul_assert(FileSys.fileExists(filename), "filename must exist");

    ghoul::Dictionary missionDict;
    ghoul::lua::loadDictionaryFromFile(filename, missionDict);

    documentation::testSpecificationAndThrow(Documentation(), missionDict, "Mission");

    return MissionPhase(missionDict);
}

}  // namespace openspace
