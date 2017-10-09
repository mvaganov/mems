#pragma once
#include "vector.h"
#include "cyclebuffer.h"

uint64_t getTotalSystemMemory() {
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	//cout << "0 " << status.dwMemoryLoad << endl;
	//cout << "1 " << status.ullAvailExtendedVirtual<< endl;
	//cout << "2 " << status.ullAvailPageFile<< endl;
	//cout << "3 " << status.ullAvailPhys<< endl;
	//cout << "4 " << status.ullAvailVirtual<< endl;
	//cout << "5 " << status.ullTotalPageFile<< endl;
	//cout << "6 " << status.ullTotalPhys<< endl;
	//cout << "7 " << status.ullTotalVirtual<< endl;
	return status.ullAvailVirtual;
	//return status.ullTotalPhys;
}

struct MemPage {
	LPCVOID min, max;
	String name;
	uint32_t searchHits;
	uint64_t size() const { return (uint64_t)max - (uint64_t)min; }
	MemPage(HANDLE clientHandle, LPCVOID startingPoint);
	bool has(LPCVOID point) {
		return (uint64_t)min <= (uint64_t)point && (uint64_t)point < (uint64_t)max;
	}
	float percentageOf(LPCVOID point) {
		int64_t total = size(), progress = (int64_t)point - (int64_t)min;
		//if (progress < 0) return 0;
		//if (progress > total) return 1;
		return (float)progress / total;
	}
	MemPage(LPCVOID start, LPCVOID end) :min(start), max(end), searchHits(0) {}
	MemPage(MemPage & copy) :min(copy.min), max(copy.max), searchHits(copy.searchHits) { }

	// rename to something like 'memsearch'
	LPCVOID indexOf(HANDLE clientHandle, String & metaData, const char * memory, SIZE_T size, 
		LPCVOID startSearch = NULL, LPCVOID endSearch = NULL, int acceptableError = 0, CycleBuffer * cycleBuffer = nullptr) {
		static BYTE*buffer = NULL;
		static SIZE_T allocatedSize = 0;
		if (startSearch == NULL) startSearch = min;
		if (endSearch == NULL) endSearch = max;
		BYTE* ptr = (BYTE*)startSearch, *end = (BYTE*)endSearch;
		if (size > allocatedSize) {
			delete[] buffer;
			buffer = NULL;
			buffer = new BYTE[size];
			allocatedSize = size;
		}
		SIZE_T bytesRead = 0;
		while (ptr < end) {
			if (cycleBuffer == nullptr) {
				ReadProcessMemory(clientHandle, ptr, buffer, size, &bytesRead);
			} else {
				printf("DOING thE ThiNG\n");
				// TODO rotating-buffer check that rather than re-reading the same blocks over and over.
				// if there are less than size bytes in the buffer, get CycleBuffer::DEFAULT_SEGMENT_SIZE-cycleBuffer->BytesLeftToRead() more bytes. unless size is more than CycleBuffer::DEFAULT_SEGMENT_SIZE, get size-BytesLeftToRead() in that case.
				if (cycleBuffer->BytesLeftToRead() < size) {
					if (cycleBuffer->BytesLeftToRead() == 0) {
						printf("segments before %d\n", cycleBuffer->segments.length());
						cycleBuffer->Add(nullptr, 0); // force a new end buffer segment
						printf("segments after %d\n", cycleBuffer->segments.length());
					}
					int bytesToGet = (int)((size < CycleBuffer::DEFAULT_SEGMENT_SIZE) 
						? (CycleBuffer::DEFAULT_SEGMENT_SIZE - cycleBuffer->BytesLeftToRead())
						: (size - cycleBuffer->BytesLeftToRead()));
					CycleBuffer::BufferSegment * bufseg = cycleBuffer->segments.last();
					BYTE* directBuffer = bufseg->buffer + bufseg->bufferUsed;
					int bytesHere = bufseg->bufferSize - bufseg->bufferUsed;
					int bytesPossible = (int)(end - ptr);
					if (bytesHere > bytesPossible) { bytesHere = bytesPossible; }
					ReadProcessMemory(clientHandle, ptr, directBuffer, bytesHere, &bytesRead);
					cycleBuffer->Add(nullptr, bytesHere);
				}
				long difference = (long)acceptableError;
				int result = cycleBuffer->ReadCompare((BYTE*)memory, (int)size, 1, &difference);
				switch (result) {
				case CycleBuffer::NO_EQUALITY: break;
				case CycleBuffer::SUCCESS: return (LPCVOID)ptr;
				case CycleBuffer::NOT_ENOUGH_DATA_IN_BUFFER: { int i = 0; i = 1 / i; }break;
				case CycleBuffer::NO_DATA_IN_BUFFER: { int i = 0; i = 1 / i; }break;
				}
			}
			//// TODO put this in cycleBuffer->Read, and get the acceptableError code to deal with non-contiguous blocks in the segments by reading into a 4-byte buffer
			//if (acceptableError == 0) {
			//	if (bytesRead == size && memcmp(memory, buffer, size) == 0) { return (LPCVOID)ptr; }
			//} else {
			//	int64_t * buffPtr = (int64_t*)buffer;
			//	int64_t * memPtr = (int64_t*)memory;
			//	int64_t diff = (*buffPtr - *memPtr);
			//	if(abs(diff) < acceptableError) {
			//		char itoabuffer[30];
			//		_itoa_s((int)diff, itoabuffer, 10);
			//		metaData = String("D:") + String(itoabuffer);
			//		return (LPCVOID)ptr;
			//	}
			//}
			ptr++;
		}
		return NULL;
	}
};

// if it returns true, that should signal that we're done.
typedef bool(*FPTR_foundOne)(MemPage * mb, double percentageDone);

struct CustomMemPageListing {
	HANDLE clientHandle;
	ArrayList<MemPage*> mempages;
	int pageIndexOf(LPCVOID point) {
		for (int i = 0; i < mempages.size(); ++i) {
			if (mempages[i]->has(point)) return i;
		}
		return -1;
	}
	bool add(LPCVOID ptr) {
		if (pageIndexOf(ptr) == -1) {
			MemPage * mb = new MemPage(clientHandle, ptr);
			if (mb->size() > 0) {
				mempages.push_back(mb);
				return true;
			}
		}
		return false;
	}
	int count() { return (int)mempages.size(); }
	MemPage * get(int index) { return mempages[index]; }

	struct GenerationState {
		HANDLE clientHandle;
		FPTR_foundOne foundOneCallback;
		uint64_t increment, incrementCount, lastIncrement, maxChecks, iterator;
		uint64_t pageCheckCursor;
		static const uint64_t stdPageSize = 4096;
		GenerationState(HANDLE clientHandle, FPTR_foundOne foundOneCallback)
			:clientHandle(clientHandle), pageCheckCursor(0) {
			this->foundOneCallback = foundOneCallback;
			uint64_t allmem = getTotalSystemMemory();
			maxChecks = allmem / stdPageSize + 1;
			increment = maxChecks / 2;
			incrementCount = 2;
			lastIncrement = 0;
			iterator = 0;
		}
		bool iterateScavengerhunt(CustomMemPageListing * pageset, uint64_t checksToDoThisIteration = 0) {
			if (pageset->count() == 0) return true;
			if (pageset->checked >= pageset->count()) return false;
			MemPage * page = pageset->get(pageset->checked);
			int checksDoneThisIteration = 0;
			if (pageCheckCursor == 0) {
				pageCheckCursor = (uint64_t)page->min;
			}
			uint64_t end = (uint64_t)page->max - sizeof(LPCVOID);
			uint64_t memoryLocation, bytesRead = 0, goDeeper;
			while (pageCheckCursor < end) {
				ReadProcessMemory(clientHandle, (LPCVOID)pageCheckCursor, &memoryLocation, sizeof(uint64_t), &bytesRead);
				if (bytesRead > 0) {
					LPCVOID actualPointer = (LPCVOID)memoryLocation;
					goDeeper = 0;
					bool worked = ReadProcessMemory(clientHandle, actualPointer, &goDeeper, sizeof(uint64_t), &bytesRead);
					if (worked && bytesRead > 0 && bytesRead <= sizeof(uint64_t) && (uint64_t)actualPointer != pageCheckCursor) {
						//printf("found one at 0x%016llx?  at 0x%016llx  %d   0x%016llx\n", actualPointer, pageCheckCursor, bytesRead, goDeeper);
						//platform_getch();
						if (!iThinkIFoundOne(pageset, actualPointer)) { return false; }
					} else {
						memoryLocation &= 0xffffffff; // mask 64 bit pointer to a 32 bit pointer
						worked = ReadProcessMemory(clientHandle, actualPointer, &goDeeper, sizeof(uint64_t), &bytesRead);
						if (worked && bytesRead > 0 && bytesRead <= sizeof(uint64_t) && (uint64_t)actualPointer != pageCheckCursor) {
							if (!iThinkIFoundOne(pageset, actualPointer)) { return false; }
						}
					}
				}
				pageCheckCursor++;
				if (checksToDoThisIteration > 0) {
					checksDoneThisIteration++;
					if (checksDoneThisIteration >= checksToDoThisIteration) {
						return true;
					}
				}
			}
			pageset->checked++;
			pageCheckCursor = 0;
			return true;
		}
		/// <returns>true to keep going</returns>
		bool iThinkIFoundOne(CustomMemPageListing * pageset, LPCVOID actualPointer) {
			if (pageset->add(actualPointer)) {
				//printf("added 0x%016llx (%d)\n", actualPointer, pageset->get(pageset->count()-1)->size());
				if (this->foundOneCallback != NULL) {
					if (foundOneCallback(pageset->get(pageset->count() - 1), pageset->percentageProgress)) {
						increment = 0;
						return false;
					}
				}
			}
			return true;
		}
		/// <returns>true to keep going</returns>
		bool iterateBruteForce(CustomMemPageListing * pageset, uint64_t checksToDoThisIteration = 0) {
			SIZE_T buffer, bytesRead;
			uint64_t ptr = iterator*increment;
			double thisChunk = ((float)increment / maxChecks);
			double nextChunk = (increment > 1) ? ((float)(increment / 2) / maxChecks) : 0;
			double delta = thisChunk - nextChunk;
			pageset->percentageProgress = (float)(1 - thisChunk + delta * ((float)iterator / incrementCount));
			int checksDoneThisIteration = 0;
			for (; iterator < incrementCount; ++iterator) {
				ptr += increment;
				if (!lastIncrement || ptr % lastIncrement != 0) {
					LPCVOID actualPointer = (LPCVOID)(ptr*stdPageSize);
					ReadProcessMemory(clientHandle, actualPointer, &buffer, sizeof(SIZE_T), &bytesRead);
					if (bytesRead > 0) {
						//if (pageset->add(clientHandle, actualPointer)) {
						//	if (this->foundOneCallback != NULL) {
						//		if (foundOneCallback(pageset->get(pageset->count() - 1), pageset->percentageProgress)) {
						//			increment = 0;
						//			return false;
						//		}
						//	}
						//}
						if (!iThinkIFoundOne(pageset, actualPointer)) { return false; }
					}
					if (checksToDoThisIteration > 0) {
						checksDoneThisIteration++;
						if (checksDoneThisIteration >= checksToDoThisIteration) {
							return true;
						}
					}
				}
				pageset->percentageProgress = (float)(1 - thisChunk + delta * ((float)iterator / incrementCount));
			}
			iterator = 0;
			lastIncrement = increment;
			increment /= 2;
			if (increment == 0 || (foundOneCallback != NULL && foundOneCallback(0, pageset->percentageProgress))) {
				increment = 0;
				return false;
			}
			incrementCount *= 2;
			return true;
		}
	};
	GenerationState * generator;
	float percentageProgress;
	int checked;

	void clear() {
		this->clientHandle = NULL;
		percentageProgress = 0;
		mempages.clear();
		checked = 0;
		if (generator != NULL) {
			delete generator;
			generator = NULL;
		}
	}
	void startGenerate(HANDLE clientHandle, FPTR_foundOne foundOneCallback = NULL) {
		this->clientHandle = clientHandle;
		percentageProgress = 0;
		mempages.clear();
		checked = 0;
		uint64_t allmem = getTotalSystemMemory(), stdPageSize = 4096;
		uint64_t maxChecks = allmem / stdPageSize + 1;
		if (generator != NULL) {
			delete generator;
		}
		generator = new GenerationState(clientHandle, foundOneCallback);
	}
	/// <param name="iterationsThisTime">if 0, generate a full iteration (might be a very logn blocking call)</param>
	/// <returns>true if generation is not yet complete</returns>
	bool continueGenerate(int iterationsThisTime = 0) {
		if (generator == NULL) return false;
		bool stillNeedsToGenerate = generator->iterateBruteForce(this, iterationsThisTime);
		stillNeedsToGenerate |= generator->iterateScavengerhunt(this, iterationsThisTime);
		if (!stillNeedsToGenerate) {
			delete generator;
			generator = NULL;
		}
		return stillNeedsToGenerate;
	}
	void fullyGenerate() { while (continueGenerate(0)); }
	CustomMemPageListing() :generator(NULL),clientHandle(NULL) {}
	void destroy() {
		for (int i = 0; i < mempages.size(); ++i) {
			delete mempages[i];
		}
		mempages.clear();
	}
	~CustomMemPageListing() { destroy(); }
	void copy(CustomMemPageListing & copy) {
		for (int i = 0; i < copy.mempages.size(); ++i) {
			mempages.push_back(new MemPage(*mempages[i]));
		}
	}
	CustomMemPageListing(CustomMemPageListing & copy) {
		this->copy(copy);
	}
	CustomMemPageListing & operator=(CustomMemPageListing & copy) {
		destroy();
		this->copy(copy);
	}
};
MemPage::MemPage(HANDLE clientHandle, LPCVOID startingPoint):min(NULL),max(NULL),searchHits(0) {
	BYTE buffer[1 << 10];
	SIZE_T bytesRead;
	SIZE_T increment = sizeof(buffer);
	BYTE* ptr = (BYTE*)startingPoint, *areaChecked;
	bool searchingForMax = true;
	while (increment > 0) {
		areaChecked = searchingForMax ? ptr : ptr - increment;
		ReadProcessMemory(clientHandle, (LPCVOID)areaChecked, buffer, increment, &bytesRead);
		if (bytesRead > 0 && bytesRead <= increment) {
			if (min == NULL || (uint64_t)areaChecked < (uint64_t)min) { min = (LPCVOID)areaChecked; }
			if (max == NULL || (uint64_t)(areaChecked + increment) >(uint64_t)max) {
				max = (LPCVOID)(areaChecked + increment);
				//printf("-------------\n");
			}
			//printf("%u/%d %s 0x%016lx %li\n", bytesRead, increment, searchingForMax ? "max" : "min", (uint64_t)(searchingForMax ? max : min));
			if (searchingForMax) { ptr += increment; }
			else { ptr -= increment; }
			if (increment < sizeof(buffer)) {
				increment *= 2;
			}
		} else {
			increment /= 2;
			if (searchingForMax) {
				//printf("....%d max 0x%016lx %li\n", increment, (uint64_t)max);
			}
			if (increment == 0 && searchingForMax) {
				searchingForMax = false;
				ptr = (BYTE*)min;
				increment = 1;
			}
		}
	}
}

ostream & operator<<(ostream& o, ArrayList<MemPage*> & listing) {
	o << listing.size() << endl;
	for (int i = 0; i < listing.size(); ++i) {
		o << std::hex << (uint64_t)listing[i]->min << std::dec << " " << listing[i]->name << endl;
	}
	return o;
}

istream & operator>>(istream& in, CustomMemPageListing & listing) {
	uint64_t count;
	in >> count;
	cout << "Loading " << count << endl;
	uint64_t addr = 0;
	char buffer[1024];
	String text;
	int i = 0;
	for (i = 0; i < count && in.good(); ++i) {
		in >> std::hex >> addr;
		in.get(); // eat the one space
		in.getline(buffer, sizeof(buffer));
		if (listing.add((LPCVOID)addr)) {
			MemPage * m = listing.get(listing.count() - 1);
			m->name = String(buffer);
		}
	}
	in >> std::dec;
	cout << "attempted " << i << ", " << listing.count() << " successful!" << endl;
	return in;
}