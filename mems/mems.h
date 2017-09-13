#pragma once
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "platform_conio.h"
#include "terminal.h"
#include "mempage.h"
using namespace std;

void printBits(uint64_t upTo64bits, int numbits) {
	for (int i = numbits - 1; i >= 0; i--) { putchar((upTo64bits & (1ull << i)) ? '1' : '0'); }
}
char convertNum(char c) {
	if (c < 10) { return '0' + c; }
	else { return 'A' + (c - 10); }
}
void printc(unsigned int c, unsigned int defaultBackground = 0, unsigned int defaultForeground = 7) {
	int f = defaultForeground, b = defaultBackground;
	if (c != -1) {
		if (c < 32) {
			f = PLATFORM_COLOR_BLUE;
			c = convertNum(c);
		}
		if (c >= 127) {
			f = (c / 64) + PLATFORM_COLOR_INTENSITY;
			c = convertNum(c % 64);
		}
		platform_setColor(f, b);
		putchar(c);
	}
	platform_setColor(defaultForeground, defaultBackground);
}

BOOL EnablePriv(LPWSTR lpszPriv, HANDLE procHandle = ((HANDLE)-1)) {
	HANDLE hToken;
	LUID luid;
	TOKEN_PRIVILEGES tkprivs;
	ZeroMemory(&tkprivs, sizeof(tkprivs));

	if (!OpenProcessToken(GetCurrentProcess(), (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY), &hToken))
		return FALSE;

	if (!LookupPrivilegeValue(NULL, lpszPriv, &luid)) {
		CloseHandle(hToken); return FALSE;
	}

	tkprivs.PrivilegeCount = 1;
	tkprivs.Privileges[0].Luid = luid;
	tkprivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	BOOL bRet = AdjustTokenPrivileges(hToken, FALSE, &tkprivs, sizeof(tkprivs), NULL, NULL);
	CloseHandle(hToken);
	return bRet;
}

size_t to_narrow(const wchar_t * src, char * dest, size_t dest_len) {
	size_t i = 0;
	wchar_t code;
	while (src[i] != '\0' && i < (dest_len - 1)) {
		code = src[i];
		if (code < 128) dest[i] = char(code);
		else {
			dest[i] = '?';
			if (code >= 0xD800 && code <= 0xD8FF) // lead surrogate, skip the next code unit, which is the trail
				i++;
		}
		i++;
	}
	dest[i] = '\0';
	return i - 1;
}

void inPlaceWideCharConvert(char * buffer, SIZE_T size) {
	for (int i = (int)size; i >= 0; --i) {
		buffer[i * 2] = buffer[i];
		buffer[i * 2 + 1] = 0;
	}
}

#include <map>
map<String, ArrayList<int>> __windowsProcTable;
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
	WCHAR class_name[256];
	char buffer[256];
	WCHAR title[1024];
	DWORD pId;
	GetClassName(hwnd, class_name, sizeof(class_name));
	GetWindowText(hwnd, title, sizeof(title));
	GetWindowThreadProcessId(hwnd, &pId);
	to_narrow(title, buffer, sizeof(buffer));
	String n(buffer);
	bool exists = __windowsProcTable.find(n) != __windowsProcTable.end();
	if (!exists) {
		__windowsProcTable[n] = ArrayList<int>();
	}
	if (__windowsProcTable[n].indexOf(pId) < 0) {
		__windowsProcTable[n].Add(pId);
	}
	////char * found = strstr(buffer, "Notepad");
	//if (strlen(buffer) > 0) {
	//	cout << //((found != NULL) ? "--------->" : "          ") << 
	//		buffer 
	//		//<< endl
	//		<< "(" << pId << "), "
	//		;
	//	//wcout << "Class name: " << class_name << endl << endl;
	//}
	return TRUE;
}

static const char * __animation = "-\\|/";
static int __animLen = (int)strlen(__animation);
static int __aninmation_iterations = 0;
void __printAnimation() {
	printf("\r%c ", __animation[++__aninmation_iterations%__animLen]);
}

struct Mems {
	String WINDOW_NAME;
	BYTE * ptr;
	HWND hwnd;
	HANDLE clientHandle;
	uint64_t sizeof_mainBuffer;
	BYTE* mainBuffer;
	SIZE_T bytesRead;
	bool running;
	ArrayList<BYTE*> history;
	int pageIndex;
	MemPage * page;
	int userinput;
	long consoleWidth, consoleHeight = 16;
	Terminal terminal;
	CustomMemPageListing pages;

	struct SearchHit {
		LPCVOID addr;
		int page;
		SearchHit(LPCVOID addr, int page) :addr(addr), page(page) {}
	};

	char searchBuffer[1024];
	int searchBufferUsed;
	int currentSearchResult;
	ArrayList<SearchHit> searchHits;

	void updateBasedOnConsoleSize() {
		long rows, cols;
		platform_consoleSize(rows, cols);
		const int uiHeight = 15;
		if (consoleWidth != cols || (rows > uiHeight&& consoleHeight != (rows- uiHeight))) {
			consoleWidth = cols;
			consoleHeight = rows - uiHeight;
			if (mainBuffer) { delete[] mainBuffer; }
			sizeof_mainBuffer = consoleWidth * consoleHeight;
			mainBuffer = new BYTE[sizeof_mainBuffer];
		}
	}

	Mems() : WINDOW_NAME("Untitled - Notepad"),ptr(0), hwnd(0), clientHandle(0), sizeof_mainBuffer(0),
		mainBuffer(0), bytesRead(0), running(false), pageIndex(0), page(0),
		consoleWidth(0), consoleHeight(0), currentSearchResult(0) { 
		addCommands();
		EnablePriv(SE_DEBUG_NAME); // enable this program to be accessed by other programs
	}
	void addCommands();

	String saveFileName() { return String(WINDOW_NAME) + String(".memtable"); }

	static const int INIT_SUCCESS = 0, INIT_BAD_WINDOW_NAME = 1, INIT_CANT_OPEN_PROCESS = 2;
	int init(const char * windowName = NULL, DWORD pId = 0) {
		// clean things up before populating them again
		pages.clear();
		running = true;
		pageIndex = 0;
		page = NULL;
		ptr = NULL;
		hwnd = NULL;
		if (windowName != NULL) {
			WINDOW_NAME = windowName;
			hwnd = FindWindowA(0, WINDOW_NAME.c_str());
		} else if(pId != 0) {
			_itoa_s(pId, (char*)mainBuffer, sizeof_mainBuffer, 10);
			WINDOW_NAME = String("pId ") + String((char*)mainBuffer);
		}
		updateBasedOnConsoleSize();
		bytesRead = -1;
		if (hwnd == NULL && pId == 0) {
			if (windowName != NULL) {
				memset(mainBuffer, 0, sizeof_mainBuffer);
				sprintf_s((char*)mainBuffer, sizeof_mainBuffer, "Cannot find window \"%s\".\nperhaps you should try 'init' with one of these:\n", WINDOW_NAME.c_str());
			} else if(pId == 0) {
				memset(mainBuffer, 0, sizeof_mainBuffer);
				sprintf_s((char*)mainBuffer, sizeof_mainBuffer, "No window name specified.\nperhaps you should try 'init' with one of these:\n");
			}
			printf((const char*)mainBuffer);
			return INIT_BAD_WINDOW_NAME;
		} else {
			if (pId == 0 && hwnd != NULL) {
				GetWindowThreadProcessId(hwnd, &pId);
				printf("\"%s\" pId: %d\n", WINDOW_NAME.c_str(), pId);
			}
			clientHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pId);

			if (!clientHandle) {
				memset(mainBuffer, 0, sizeof(mainBuffer));
				sprintf_s((char*)mainBuffer, sizeof(mainBuffer), "Could not open process \"%ld\".\n", pId);
				printf((const char*)mainBuffer);
				selectSource();
				return INIT_CANT_OPEN_PROCESS;
			} else {
				//printf("Opened 0x%08x\n", (unsigned int)clientHandle);
				EnablePriv(SE_DEBUG_NAME, clientHandle);
			}
		}
		pages.startGenerate(clientHandle, NULL);// drawUpdate);

		// find an existing memtable file
		String existingMemoryTable = saveFileName();
		std::ifstream ifs(existingMemoryTable.c_str(), std::ifstream::in);
		if (ifs.is_open()) {
			if (ifs.good()) {
				ifs >> pages;
			}
			ifs.close();
		}

		if (pages.count() == 0) {
			printf("searching for a memory page...\n");
			while (pages.count() == 0 && pages.continueGenerate(10000)) { __printAnimation(); }
			saveMemoryPageSearch();
			printf("found a page!");
		}

		memset(mainBuffer, 0, sizeof_mainBuffer);
		running = true;
		pageIndex = 0;
		page = pages.get(pageIndex);
		cout << "using page " << pageIndex << " at " << page << endl;
		ptr = (BYTE*)page->min;
		return INIT_SUCCESS;
	}
	int forceValidInit(const char * windowName = NULL, DWORD pId = 0) {
		int result, count = 0;
		result = init(windowName, pId);
		if (result != INIT_SUCCESS) {
			while (!selectSource()) { count++; };
		}
		return count;
	}
	void draw() {
		// draw
		int missingFromPage = (int)sizeof_mainBuffer;
		int totalPrintable = 0;
		if (page != NULL) {
			missingFromPage = (int)((uint64_t)page->min - (uint64_t)ptr);
			totalPrintable = (int)((uint64_t)page->max - (uint64_t)ptr);
			int amntToGrab = (totalPrintable > sizeof_mainBuffer) ? (int)sizeof_mainBuffer : totalPrintable;
			if (missingFromPage > 0) {
				if (missingFromPage > sizeof_mainBuffer) {
					memset(mainBuffer, 0, amntToGrab);
				} else {
					memset(mainBuffer, 0, missingFromPage);
					ReadProcessMemory(clientHandle, (LPCVOID)(ptr + missingFromPage), (mainBuffer + missingFromPage), (amntToGrab - missingFromPage), &bytesRead);
				}
			} else {
				if (amntToGrab < sizeof_mainBuffer) {
					memset(mainBuffer + amntToGrab, 0, sizeof_mainBuffer - amntToGrab);
				}
				ReadProcessMemory(clientHandle, (LPCVOID)ptr, mainBuffer, amntToGrab, &bytesRead);
			}
		}
		platform_move(0, 0);
		for (int i = 0; i < sizeof_mainBuffer; ++i) {
			//printc(ptr[i]);
			bool isInRange = page != NULL && (uint64_t)(ptr + i) >= (uint64_t)page->min && (uint64_t)(ptr + i) < (uint64_t)page->max;
			int bgcolor = isInRange ? 0 : PLATFORM_COLOR_RED;
			if (isInRange) {
				uint64_t tmpBuffer = 0;
				uint64_t whereThisIsPointing = *((uint64_t*)(mainBuffer + i));
				if (ReadProcessMemory(clientHandle, (LPCVOID)whereThisIsPointing, &tmpBuffer, sizeof(tmpBuffer), &bytesRead)) {
					bgcolor = PLATFORM_COLOR_INTENSITY | PLATFORM_COLOR_RED;
				}
			}
			printc((unsigned int)mainBuffer[i], bgcolor);
		}
		platform_setColor(7, 0);
		if (page != NULL) {
			printc('w', 15, 0); printc(-1); putchar(' ');
			printc('a', 15, 0); printc(-1); putchar(' ');
			printc('s', 15, 0); printc(-1); putchar(' ');
			printc('d', 15, 0); printc(-1); putchar(' ');
			platform_setColor(0, 15);
			printf("pgup"); printc(-1); putchar(' ');
			platform_setColor(0, 15);
			printf("pgdn"); printc(-1); putchar(' ');
		}
		printf("@0x%016llx, %12lld", (uint64_t)ptr, (int64_t)ptr);
		if (page != NULL) {
			printf("%8.2f%% into page %d", (page->percentageOf((LPCVOID)ptr) * 100), pageIndex);
		}putchar('\n');
		if (page != NULL) {
			printf("page "); printc('<', 15, 0); printc(-1); printf(" %3d", pageIndex);
			printf("/%3d ", pages.count()); printc('>', 15, 0); printc(-1);
			printf("  [0x%016llx to ", (uint64_t)page->min);
			printf("0x%016llx", (uint64_t)page->max); printf(") %lldB", page->size());
		}putchar('\n');
		printf("char "); printc((unsigned int)mainBuffer[0]);
		platform_setColor(7, 0);
		uint64_t val = (uint64_t)*((uint64_t*)mainBuffer);//ptr);
		printf(" %3d           int32 0x%08x,%13d\n", *mainBuffer, (uint32_t)val, (int32_t)val);//*ptr);
		printf("             int64 0x%016llx, %20lli\n", (uint64_t)val, (int64_t)val);
		printBits(val, 64); putchar('\n'); 
		if (searchHits.size()) {
			printf("result "); printc('F', 15, 0); printc(-1); printf(" %3d", currentSearchResult);
			printf("/%3d ", (int)searchHits.size()); printc('f', 15, 0); printc(-1);
			printf(" [0x%016llx, page %d] ", (uint64_t)searchHits[currentSearchResult].addr, pageIndex);
		} else {
			for (int i = 0; i < consoleWidth - 1; ++i)putchar(' ');
			putchar('\r');
			platform_setColor(0, 15);
			printf("space"); printc(-1); putchar(' ');
			platform_setColor(0, 15);
			printf("backspace"); printc(-1); putchar(' ');
			platform_setColor(0, 15);
			printf("enter"); printc(-1); putchar(' ');
		}
		printf("\n   (%010.7f%% total memory searched, %d/%d scavenged)", pages.percentageProgress, pages.checked, pages.count());
	}
	void input() {
		userinput = 0;
		int pageCountWhenStartedDraw = pages.count();
		//cout << pages.generator->increment << " " << pages.generator->iterator << "/" << pages.generator->incrementCount << "            \n";
		//cout << "scavenging page " << pages.generator->pageCheckIndex << " " << pages.generator->pageCheckCursor << "/" << pages.get(pages.generator->pageCheckIndex)->size() << endl;
		do {
			if (platform_kbhit()) {
				userinput = platform_getch();
			} else {
				time_t soon = clock() + 50;
				int pageCount = pages.count();
				do { pages.continueGenerate(100); } while (!platform_kbhit() && clock() < soon);
				if (pageCount != pages.count()) { saveMemoryPageSearch(); }
				__printAnimation();
			}
		} while (userinput == 0 && pageCountWhenStartedDraw == pages.count());
	}
	void update() {
		switch (userinput) {
		case 'w':	ptr -= consoleWidth;	break;
		case 'a':	ptr -= 1;	break;
		case 's':	ptr += consoleWidth;	break;
		case 'd':	ptr += 1;	break;
		case 18912: ptr -= consoleWidth*consoleHeight; break;//pageup
		case 20960: ptr += consoleWidth*consoleHeight; break;//pagedown
		case '>': case '.':
			if (pages.count() > 0) {
				pageIndex++;
				if (pageIndex >= pages.count())pageIndex = 0;
				page = pages.get(pageIndex);
				history.push_back(ptr);
				ptr = (BYTE*)page->min;
			}
			break;
		case '<': case ',':
			if (pages.count() > 0) {
				pageIndex--;
				if (pageIndex < 0)pageIndex = pages.count() - 1;
				page = pages.get(pageIndex);
				history.push_back(ptr);
				ptr = (BYTE*)page->min;
			}
			break;
		case 'f': //next
			if (searchHits.size() > 0) {
				// TODO make a "jumpMemory" function, which adjusts the page stuff.
				currentSearchResult++;
				while (currentSearchResult >= searchHits.size()) { currentSearchResult -= (int)searchHits.size(); }
				SearchHit sh = searchHits[currentSearchResult];
				pageIndex = sh.page;
				page = pages.get(pageIndex);
				history.push_back(ptr);
				ptr = (BYTE*)sh.addr;
			}
			break;
		case 'F': //previous
			if (searchHits.size() > 0) {
				currentSearchResult--;
				while (currentSearchResult < 0) { currentSearchResult += (int)searchHits.size(); }
				SearchHit sh = searchHits[currentSearchResult];
				pageIndex = sh.page;
				page = pages.get(pageIndex);
				history.push_back(ptr);
				ptr = (BYTE*)sh.addr;
			}
			break;
		case ' ':
			if(page != NULL){
				// this stack removes ambiguity about scope of whereThisIsPointing
				uint64_t whereThisIsPointing = *((uint64_t*)(mainBuffer));
				uint64_t tmpBuffer;
				if (ReadProcessMemory(clientHandle, (LPCVOID)whereThisIsPointing, &tmpBuffer, sizeof(tmpBuffer), &bytesRead)) {
					history.push_back(ptr);
					ptr = (BYTE*)whereThisIsPointing;
					pageIndex = pages.pageIndexOf((LPCVOID)ptr);
					if (pageIndex == -1) { pages.add((LPCVOID)ptr); pageIndex = pages.count() - 1; }
					page = pages.get(pageIndex);
				}
			}
			break;
		case '\b':
			if (history.size() > 0) {
				ptr = history[history.size() - 1];
				history.pop_back();
				pageIndex = pages.pageIndexOf((LPCVOID)ptr);
				if (pageIndex == -1) { pages.add((LPCVOID)ptr); pageIndex = pages.count() - 1; }
				page = pages.get(pageIndex);
			}
			break;
		case '\n': case '\r':
			cout << "\n                                                                \r$: ";
			cin.getline(searchBuffer, sizeof(searchBuffer));
			int bufferLen = (int)strlen(searchBuffer);
			terminal.Run(string(searchBuffer));
		}
		// post update (pre draw)
		updateBasedOnConsoleSize();
	}
	void release() {
		if (mainBuffer) {
			delete[] mainBuffer;
			mainBuffer = 0;
		}
	}
	~Mems() { release(); }
	static const int SEARCH_BYTES = 0;
	static const int SEARCH_WCHAR = 1;
	static const int SEARCH_HEX = 2;
	static const int SEARCH_BINARY = 3;
	static const int SEARCH_INT8 = 4;
	static const int SEARCH_INT16 = 5;
	static const int SEARCH_INT32 = 6;
	static const int SEARCH_INT64 = 7;

	bool take(ArrayList<String> & args, String s) {
		int i = args.indexOf(s);
		if (i == 1) { args.remove(i); return true; }
		return false;
	}

	void find(ArrayList<String> & args) {
		int searchType = SEARCH_BYTES;
		if (take(args,"wchar") || take(args, "w")) { searchType = SEARCH_WCHAR; }
		else if (take(args, "chars")) { searchType = SEARCH_BYTES; }
		else if (take(args, "binary") || take(args, "bin")) { searchType = SEARCH_BINARY; }
		else if (take(args, "hex") || take(args, "h")) { searchType = SEARCH_HEX; }
		else if (take(args, "int8")) { searchType = SEARCH_INT8; }
		else if (take(args, "int16")) { searchType = SEARCH_INT16; }
		else if (take(args, "int32")) { searchType = SEARCH_INT32; }
		else if (take(args, "int64")) { searchType = SEARCH_INT64; }
		int valueIndex = 1;
		if (args.length() > valueIndex) {
			String s = args[valueIndex];
			searchBufferUsed = (int)s.length();
			memcpy(searchBuffer, s.c_str(), searchBufferUsed);
			searchBuffer[searchBufferUsed] = '\0';
		} else {
			putchar('\n');
			for (int i = 0; i < this->consoleWidth-1; ++i) { putchar(' '); }
			putchar('\r');
			cout << "search term: ";
			cin.getline(searchBuffer, sizeof(searchBuffer));
			searchBufferUsed = (int)strlen(searchBuffer);
		}
		uint64_t value;
		switch (searchType) {
		case Mems::SEARCH_WCHAR:
			inPlaceWideCharConvert(searchBuffer, searchBufferUsed);
			searchBufferUsed *= 2;
			break;
		case Mems::SEARCH_HEX:
		case Mems::SEARCH_BINARY:
			switch (searchType) {
			case Mems::SEARCH_HEX:
				value = strtoul(searchBuffer, NULL, 16); break;
				if(searchBufferUsed % 2 != 0) searchBufferUsed = (searchBufferUsed / 2) + 1;
				else                          searchBufferUsed = searchBufferUsed / 2;
			case Mems::SEARCH_BINARY:
				value = strtoul(searchBuffer, NULL, 2); break;
				if (searchBufferUsed % 8 != 0) searchBufferUsed = (searchBufferUsed / 8) + 1;
				else                           searchBufferUsed = searchBufferUsed / 8;
			}
			memcpy(searchBuffer, &value, searchBufferUsed);
			break;
		case Mems::SEARCH_INT8:
		case Mems::SEARCH_INT16:
		case Mems::SEARCH_INT32:
		case Mems::SEARCH_INT64:
			value = atoll(searchBuffer);
			switch (searchType) {
			case Mems::SEARCH_INT8: searchBufferUsed = 1; break;
			case Mems::SEARCH_INT16: searchBufferUsed = 2; break;
			case Mems::SEARCH_INT32: searchBufferUsed = 4; break;
			case Mems::SEARCH_INT64: searchBufferUsed = 8; break;
			}
			memcpy(searchBuffer, &value, searchBufferUsed);
			break;
		}
		this->find();
	}

	// TODO make this an asynchronous process...
	int find_pageIndex;
	const int find_chunkSize = 4096;
	BYTE* find_chunkStart;
	//void findStart() {
	//	printf("looking for: "); for (int i = 0; i < searchBufferUsed; ++i) { printf("%02x", searchBuffer[i]); } printf("\n");
	//	searchHits.clear();
	//	find_pageIndex = 0;
	//	find_chunkStart = 0;
	//}
	//bool findContinue() {
	//	find_chunkStart += find_chunkSize;
	//	LPCVOID start = NULL, found;
	//	do {
	//		// TODO 
	//		found = pages.get(find_pageIndex)->indexOf(clientHandle, searchBuffer, searchBufferUsed, start, );
	//		if (found != NULL) {
	//			searchHits.Add(Mems::SearchHit(found, find_pageIndex));
	//			start = (LPCVOID)((BYTE*)found + 1);
	//		}
	//	} while (found != NULL);
	//	if (searchHits.length() > 0) {
	//		bool thisPageWasHit = false;
	//		for (int h = 0; h < searchHits.length(); h++) {
	//			if (searchHits[h].page == page) { thisPageWasHit = true; break; }
	//		}
	//		if (thisPageWasHit) {
	//			// TODO when a page has positive search results, mark that memory page as more intersting, and move it closer to the front of the list.
	//		}
	//	}
	//	return true;
	//}

	void find() {
		printf("looking for: "); for (int i = 0; i < searchBufferUsed; ++i) { printf("%02x", searchBuffer[i]); } printf("\n");
		searchHits.clear();
		for (int page = 0; page < pages.count(); ++page) {
			printf("."); // TODO remove search output
			LPCVOID start = NULL, found;
			do {
				found = pages.get(page)->indexOf(clientHandle, searchBuffer, searchBufferUsed, start);
				if (found != NULL) {
					searchHits.Add(Mems::SearchHit(found, page));
					start = (LPCVOID)((BYTE*)found + 1);
				}
			} while (found != NULL);
			if (searchHits.length() > 0) {
				bool thisPageWasHit = false;
				for (int h = 0; h < searchHits.length(); h++) {
					if (searchHits[h].page == page) { thisPageWasHit = true; break; }
				}
				if (thisPageWasHit) {
					// TODO when a page has positive search results, mark that memory page as more intersting, and move it closer to the front of the list.
				}
			}
		}
		// TODO if memory page interst has changed sufficiently to require a re-shuffling, do the reshuffle!
			// Adjust searchHits and current page as needed
		if (searchHits.length() == 0) { cout << "not found." << endl; } // TODO remove output
	}
	void saveMemoryPageSearch() {
		String filename = saveFileName();
		ofstream myfile;
		myfile.open(filename.c_str());
		myfile << pages;
		myfile.close();
	}
	bool selectSource() {
		__windowsProcTable.clear();
		EnumWindows(EnumWindowsProc, NULL);
		platform_sleep(500);
		map<String, ArrayList<int>>::iterator it = __windowsProcTable.begin();
		//for (it = __windowsProcTable.begin(); it != __windowsProcTable.end(); it++) {
		//	platform_setColor(15, 0);
		//	cout << it->first;
		//	platform_setColor(8, 0);
		//	for (int i = 0; i < it->second.size(); ++i) {
		//		if (i > 0) { cout << ","; }
		//		cout << it->second.at(i);
		//	}
		//	cout << " ";
		//}
		//platform_setColor(7, 0);
		printc('w', 15, 0); printc(-1); putchar(' ');
		printc('a', 15, 0); printc(-1); putchar(' ');
		printf("prev   ");
		printc('s', 15, 0); printc(-1); putchar(' ');
		printc('d', 15, 0); printc(-1); putchar(' ');
		printf("next   ");
		platform_setColor(0, 15);
		printf("enter"); printc(-1); printf(" select   ");
		platform_setColor(0, 15);
		printf("escape"); printc(-1); printf(" cancel   ");
		putchar('\n');
		int userInput = 0;
		String selected("");
		bool hasTitle = false;
		DWORD pId = 0;
		int index = 0;
		long cols, rows;
		char buffer[40];
		do {
			platform_consoleSize(rows, cols);
			putchar('\r'); for (int i = 0; i < cols - 1; ++i) { putchar(' '); }
			if (!hasTitle) {
				sprintf_s(buffer, "\rchoice: %%.%ds", cols - 10);
				printf(buffer, it->first.c_str());
				if (it->first.length() == 0) {
					platform_setColor(8, 0); printf("unnamed process"); platform_setColor(7, 0);
				}
				printf("\rchoice: ");
				userInput = platform_getch();
				switch (userInput) {
				case 'a':
					if (it == __windowsProcTable.begin()) { it = __windowsProcTable.end(); } it--; break;
				case 'd':
					it++; if (it == __windowsProcTable.end()) { it = __windowsProcTable.begin(); } break;
				case '\n': case '\r': selected = it->first; hasTitle = true; break;
				}
			} else {
				printf("\r%ld\r", it->second[index]);
				userInput = platform_getch();
				switch (userInput) {
				case 'a':
					if (index == 0) { index = it->second.length(); } index--; break;
				case 'd':
					index++; if (index == it->second.length()) { index = 0; } break;
				case 27: hasTitle = false;  selected = ""; userInput = 0; break;
				case '\n': case '\r':
					pId = it->second[index];
					userInput = 27;
					init(selected.c_str(), pId);
					break;
				}
			}
		} while (userInput != 27);
		return hasTitle;
	}
};

void Mems::addCommands() {
	terminal.add(Terminal::CMD("quit", "summary: quits this program", [this](ArrayList<String> & args)->int {
		running = false;
		return 0;
	}));
	terminal.add(Terminal::CMD("find", "usage: find [chars|wchars|int8|int16|int32|int64|hex|binary] <value>\n"
		"summary: searches known memory pages for the specified value.\nexample: find wchars \"Hello World!\"\nexample: find hex baadf00d", 
		[this](ArrayList<String> & args)->int {
		find(args);
		return 0;
	}));
	terminal.add(Terminal::CMD("init", "usage: init\nusage: init named <program name>\nusage: init pid <integer value>"
		"sumary: initializes memory space to given program name. if no program is given, prints a list of valid program names.", [this](ArrayList<String> & args)->int {
		if (args.length() == 1) {
			printf((const char*)mainBuffer);
			selectSource();
		} else if (args.length() == 3) {
			if (args[1] == "named") {
				init(args[2].c_str());
			} else if (args[1] == "pid") {
				init(NULL, atoi(args[2].c_str()));
			}
		} else {
			printf("type \"help init\" for usage information");
		}
		return 0;
	}));
	terminal.add(Terminal::CMD("memstat", "summary: debug", [this](ArrayList<String> & args)->int {
		platform_clearScreen();
		MEMORYSTATUSEX status;
		status.dwLength = sizeof(status);
		GlobalMemoryStatusEx(&status);
		cout << "        dwMemoryLoad " << status.dwMemoryLoad << endl;
		cout << "AvailExtendedVirtual " << status.ullAvailExtendedVirtual << endl;
		cout << "       AvailPageFile " << status.ullAvailPageFile << endl;
		cout << "           AvailPhys " << status.ullAvailPhys << endl;
		cout << "        AvailVirtual " << status.ullAvailVirtual << endl;
		cout << "       TotalPageFile " << status.ullTotalPageFile << endl;
		cout << "           TotalPhys " << status.ullTotalPhys << endl;
		cout << "        TotalVirtual " << status.ullTotalVirtual << endl;
		if (this->pages.generator != NULL) {
			cout << "Page Generator Increment " << this->pages.generator->increment << endl;
			cout << "  Progress: " << this->pages.generator->iterator << "/" << this->pages.generator->incrementCount << endl;
			if (this->pages.checked < this->pages.count()) {
				MemPage * p = this->pages.get(this->pages.checked);
				int64_t delta = (int64_t)p->max - (int64_t)p->min, pos = (int64_t)this->pages.generator->pageCheckCursor - (int64_t)p->min;
				cout << "Scavenging: " << pos << "/" << delta << endl;
			}
		}
		platform_getch();
		platform_clearScreen();
		return 0;
	}));
	terminal.add(Terminal::CMD("save", "summary: forces current state of searched memory pages to be saved to a file local to the program.", [this](ArrayList<String> & args)->int {
		saveMemoryPageSearch();
		return 0;
	}));
	terminal.add(Terminal::CMD("clear", "summary: clears the screen", [this](ArrayList<String> & args)->int {
		platform_clearScreen();
		return 0;
	}));
	// TODO ^|xor
	// TODO &|and
	// TODO ||or
}