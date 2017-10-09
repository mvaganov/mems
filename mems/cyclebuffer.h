#pragma once
#include <windows.h>
#include "vector.h"
using namespace std;

class CycleBuffer {
public:
	static const int DEFAULT_SEGMENT_SIZE = 1000;
	struct BufferSegment {
		BYTE * buffer;
		int bufferSize, bufferUsed;
		BufferSegment * next;

		BufferSegment() : buffer(nullptr), bufferSize(DEFAULT_SEGMENT_SIZE), bufferUsed(0), next(nullptr) { buffer = new BYTE[bufferSize]; }
		void clear() { if (buffer) { delete[] buffer; buffer = 0; bufferSize = 0; } }
		~BufferSegment() { clear(); }
		int write(BYTE * b, int byteCount) {
			int i = 0;
			if (b != nullptr) while (bufferUsed < bufferSize) { buffer[bufferUsed++] = b[i++]; }
			else while (bufferUsed < bufferSize) { bufferUsed++; i++; }
			return i;
		}
		bool isFilled() { return bufferUsed >= bufferSize; }
	};
	ArrayList<BufferSegment*> segments;
	ArrayList<BufferSegment*> opensegments;

	struct BuffLoc {
		int index;
		BufferSegment* segment;
		void set(int index, BufferSegment* segment) { this->index = index; this->segment = segment; }
		BuffLoc() : segment(nullptr), index(0) {}
	};
	BuffLoc cursor;

	CycleBuffer() { }

	void clean() {
		cursor.set(0, nullptr);
		for (int i = 0; i < segments.size(); ++i) {
			opensegments.add(segments[i]);
		}
		segments.clear();
	}

	void clear() {
		for (int i = 0; i < opensegments.size(); ++i) {
			opensegments[i]->clear();
			delete opensegments[i];
		}
		opensegments.clear();
		for (int i = 0; i < segments.size(); ++i) {
			segments[i]->clear();
			delete segments[i];
		}
		segments.clear();
	}

	void Add(BYTE * bytes, int byteCount) {
		BuffLoc currentEnd;
		if (segments.length() > 0) {
			currentEnd.set(segments.last()->bufferUsed, segments.last());
		}
		do {
			// grab the currently-written-to segment. if there isnt one
			if (currentEnd.segment == nullptr) {
				if (opensegments.size() > 0) {
					currentEnd.set(0, opensegments.pop());
					currentEnd.segment->next = nullptr;
				} else {
					// create a new segment, 
					currentEnd.set(0, new BufferSegment());
					if (currentEnd.segment == nullptr) { int i = 0; i = 1 / i; } // ran out of memory?
				}
				// add it to the end of the segment list, update cursor
				if (segments.length() > 0) {
					segments.last()->next = currentEnd.segment;
				}
				segments.Add(currentEnd.segment);
			}
			// write to the current segment
			int written = currentEnd.segment->write(bytes, byteCount);
			if (bytes != nullptr) {
				bytes += written;
			}
			byteCount -= written;
			// if the segment gets filled up, stop... restart at the top, with fewer bytes to write
			if (currentEnd.segment->isFilled()) {
				currentEnd.set(0, nullptr);
			}
		} while (byteCount > 0);
	}
	int BytesLeftToRead() {
		int total = 0;
		if (cursor.segment != nullptr) {
			total = cursor.segment->bufferUsed - cursor.index;
		}
		BufferSegment* iter = cursor.segment->next;
		while (iter) {
			total += iter->bufferUsed;
			iter = iter->next;
		}
		return total;
	}

	static const int NO_EQUALITY = 0;
	static const int SUCCESS = 1;
	static const int NOT_ENOUGH_DATA_IN_BUFFER = 3;
	static const int NO_DATA_IN_BUFFER = 4;
	// read from the buffer, but don't lose all of it after reading... advance the buffered reader only a specified amount. -1 means just keep advancing like a normal stream would
	// @param buffer what is being searched for
	// @param buffer_size how much is being searched for
	// @param advanceAmount if -1, advance the stream while reading against the buffer
	// @param in_out_differenceBetweenSearchAndFound if not null, points to the maximum amount of difference between the searched value and found value. is set to the actual difference between the two when done.
	// @return an error code, 0 for success (match found)
	int ReadCompare(BYTE * buffer, int buffer_size, int advanceAmount = -1, long * in_out_differenceBetweenSearchAndFound = nullptr) {
		if (cursor.segment == nullptr) {
			if (segments.length() == 0)
				return NO_DATA_IN_BUFFER;
			cursor.segment = segments[0];
			cursor.index = 0;
		}
		int red = 0;
		BuffLoc tempCursor = cursor;
		long compareBuffer = 0;
		BYTE * searchcursor = buffer;
		int acceptableError = *in_out_differenceBetweenSearchAndFound;
		while (red < buffer_size) {
			while (tempCursor.index < tempCursor.segment->bufferSize && red < buffer_size) {
				//if (writeToBufferElseCompare) {
				//	*buffer = tempCursor.segment->buffer[tempCursor.index++];
				//} else {
				BYTE valueHere = tempCursor.segment->buffer[tempCursor.index];
				if (in_out_differenceBetweenSearchAndFound != nullptr) {
					if (red < sizeof(long)) {
						((BYTE*)(&compareBuffer))[red] = valueHere;
						// if we have read the first 4 bytes, or we have read all the bytes we're going toread
						if (red + 1 >= sizeof(long) || red + 1 >= buffer_size) {
							*in_out_differenceBetweenSearchAndFound = *((long*)buffer) - compareBuffer;
							if (abs(*in_out_differenceBetweenSearchAndFound) > acceptableError) {
								return NO_EQUALITY;
							}
							in_out_differenceBetweenSearchAndFound = nullptr;
						}
					}
				} else if (valueHere != *searchcursor) {
					return NO_EQUALITY;
				}
				//}
				tempCursor.index++;
				if (advanceAmount == -1 || tempCursor.index < advanceAmount) { cursor.index++; }
				red++;
				searchcursor++;
			}
			if (tempCursor.index >= tempCursor.segment->bufferSize) {
				tempCursor.index = 0;
				tempCursor.segment = tempCursor.segment->next;
			}
			if (cursor.index >= cursor.segment->bufferSize) {
				segments.removeAt(0);
				opensegments.add(cursor.segment);
				cursor.segment = cursor.segment->next;
				cursor.index = 0;
			}
			if (tempCursor.segment == nullptr || cursor.segment == nullptr) {
				return NOT_ENOUGH_DATA_IN_BUFFER;
			}
		}
		return SUCCESS;
	}
};