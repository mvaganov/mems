#pragma once

#include <vector>
#include <algorithm>
#include "string.h"

template <typename T>
class ArrayList : public vector<T> {
public:
	ArrayList() {}
	T get(const int i) const { return (*this)[i]; }
	const int length() const { return (int)this->size(); }
	const int Length() const { return length(); }
	void add(T t) { this->push_back(t); }
	void Add(T t) { this->add(t); }
	void add(const int index, T t) { this->insert(this->begin() + index, t); }
	int indexOf(T t) {
		int i = (int)(find(this->begin(), this->end(), t) - this->begin());
		if (i >= size()) i = -1;
		return i;
	}
	void removeAt(int index) { this->erase(this->begin() + index); }
	void insertAt(int index, T& element) { this->insert(begin() + index, element); }
	ArrayList(initializer_list<T> l) : vector<T>(l) {}
	void add(std::initializer_list<T> l) { this->insert(this->end(), l.begin(), l.end()); }
	std::pair<const T*, std::size_t> c_arr() const {
		return std::pair(&(*this)[0], this->size());// copy list-initialization in return statement
											 // this is NOT a use of std::initializer_list
	}
	String toString() const {
		String result = "["; result += join(", "); result += "]"; return result;
	}
	String join(String delimiter) const {
		String result = "";
		for (int i = 0; i<this->size(); ++i) {
			if (i>0) { result = result + delimiter; }
			result = result + get(i).toString();
		}
		return result;
	}
};