#define UNICODE
#include <stdio.h>
#include "platform_conio.h"
#include <windows.h>
#include <iostream>
using namespace std;
#include "mems.h"

int main(int argc, char ** argv) {
	Mems mems;
	// TODO make it work with no arguments, getting the user to search for the window/process
	mems.forceValidInit();// "Untitled - Notepad");
	while (mems.running) {
		mems.draw();
		mems.input();
		mems.update();
	}
	mems.release();
}
