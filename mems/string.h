#pragma once
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
using namespace std;

// THIS IS WHERE 'system' IS USED. BE CAREFUL WITH THIS CALL.
void SYSTEM(string s) {
	cout << "COMMAND-LINE CALL: " << s << endl;
	system(s.c_str());
}

class String : public string {
public:
	inline static String whitespace() { return " \t\n\r"; }
	inline static String quotes() { return "\"\'"; }
	String() {}
	String(string const & s) :string(s) {}
	String(const char* c) :string(c) {}
	char charAt(int index) const { return (*this)[index]; }
	int parseInt() const { return atoi(this->c_str()); }
	String substring(const int startindex) const { return this->substr(startindex); }
	String substring(const int startindex, const int endindex) const { return this->substr(startindex, endindex - startindex); }
	bool beginsWith(String & s) {
		if (length() < s.length()) { return false; }
		for (int i = 0; i < s.length(); ++i) {
			if (s[i] != charAt(i)) return false;
		}
		return true;
	}
	static String fromInt(int a) { ostringstream temp; temp << a; return temp.str(); }
	static String fromInt(int a, int maxLeadingZeros) {
		String s = fromInt(a);
		String result;
		for (int i = (int)s.length(); i < maxLeadingZeros; ++i) { result += "0"; }
		return result + s;
	}
	bool equals(String & s) const {
		if (s.length() != length()) return false;
		for (int i = 0; i < length(); ++i) {
			if (charAt(i) != s.charAt(i)) return false;
		}
		return true;
	}
	template<typename T> static String from(T a) { ostringstream temp; temp << a; return temp.str(); }
	String toString() const { return *this; }
	int indexOf(char const & c, const int startIndex) const {
		for (int i = startIndex; i < length(); ++i) { if (charAt(i) == c) return i; } return -1;
	}
	int indexOf(char const & c) const { return indexOf(c, 0); }
	int lastIndexOf(char const & c, const int endIndex) const {
		for (int i = endIndex - 1; i >= 0; --i) { if (charAt(i) == c) return i; } return -1;
	}
	int lastIndexOf(char const & c) const { return lastIndexOf(c, (int)this->length()); }
	bool startswith(string const & prefix) const {
		if (prefix.length() > this->length()) return false;
		for (int i = 0; i < prefix.length(); ++i) {
			if (this->charAt(i) != prefix[i]) { return false; }
		}
		return true;
	}
	static String fromFile(string const & filename) {
		ifstream t(filename.c_str());
		String str;
		if (t.good()) {
			t.seekg(0, ios::end);
			str.reserve(t.tellg());
			t.seekg(0, ios::beg);
			str.assign(istreambuf_iterator<char>(t), istreambuf_iterator<char>());
			t.close();
		}
		return str;
	}
	bool toFile(string const & filename) const {
		ofstream t(filename);
		if (t.good()) {
			t.write(this->c_str(), length());
			t.close();
			return true;
		}
		return false;
	}
	static String fromWeb(string const & url) {
		String command = "curl " + url + " > file.txt";
		SYSTEM(command);
		return String::fromFile("file.txt");
	}
	static String trim(String const & s, String const & whitespace = String::whitespace()) {
		int i = 0, e = (int)s.length() - 1;
		for (; i < s.length() && whitespace.indexOf(s[i]) >= 0; ++i);
		for (; e >= 0 && whitespace.indexOf(s[e]) >= 0; --e);
		//		cout << "(" << s << ") becomes (" << s.substring(i,e+1) << ")" << endl;
		return s.substring(i, e + 1);
	}
	String Trim(String whitespace = String::whitespace()) const { return String::trim(*this, whitespace); }
	String trimStart(String const & s, String const & whitespace = String::whitespace()) {
		int i = 0;
		for (; i < s.length() && whitespace.indexOf(s[i]) >= 0; ++i);
		return s.substring(i, (int)s.length());
	}
	String TrimStart(const String & whitespace = String::whitespace()) { return String::trimStart(*this, whitespace); }
	String toLowerCase() const {
		string s = *this;
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		return s;
	}
	String ToLower() const { return toLowerCase(); }
private: static bool EscapeMap_TryGetValue(const char c, char & out) {
		switch (c) {
		case '0': out = '\0'; return true;
		case 'a': out = '\a'; return true;
		case 'b': out = '\b'; return true;
		case 'f': out = '\f'; return true;
		case 'n': out = '\n'; return true;
		case 'r': out = '\r'; return true;
		case 't': out = '\t'; return true;
		case 'v': out = '\v'; return true;
		}
		return false;
	}
public:
	static String Unescape(String escaped) {
		stringstream sb;
		bool inEscape = false;
		int startIndex = 0;
		for (int i = 0; i < escaped.length(); i++) {
			if (!inEscape) {
				inEscape = escaped[i] == '\\';
			} else {
				char c;
				if (!EscapeMap_TryGetValue(escaped[i], c)) {
					c = escaped[i]; // unknown escape sequences are literals
				}
				sb << escaped.substring(startIndex, i - startIndex - 1);
				sb << c;
				startIndex = i + 1;
				inEscape = false;
			}
		}
		sb << (escaped.substring(startIndex));
		return sb.str();
	}
};

