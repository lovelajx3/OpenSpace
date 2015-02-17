/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2015                                                               *
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

#ifndef SYNCBUFFER_H
#define SYNCBUFFER_H

#include <vector>
#include <sgct.h>

namespace openspace {

class SyncBuffer {
public:

	SyncBuffer(size_t n);

	template<typename T>
	void encode(const T& v) {
		const size_t size = sizeof(T);
		assert(_encodeOffset + size < _n);

		memcpy(_dataStream.data() + _encodeOffset, &v, size);
		_encodeOffset += size;
	}

	template<typename T>
	T decode() {
		const size_t size = sizeof(T);
		assert(_decodeOffset + size < _n);
		T value;
		memcpy(&value, _dataStream.data() + _decodeOffset, size);
		_decodeOffset += size;
		return value;
	}


	template<typename T>
	void decode(T& value) {
		const size_t size = sizeof(T);
		assert(_decodeOffset + size < _n);
		memcpy(&value, _dataStream.data() + _decodeOffset, size);
		_decodeOffset += size;
	}

	void write();

	void read();

private:
	size_t _n;
	size_t _encodeOffset;
	size_t _decodeOffset;
	std::vector<char> _dataStream;
	sgct::SharedVector<char> _synchronizationBuffer;
};

} // namespace openspace

#endif // SYNCBUFFER_H