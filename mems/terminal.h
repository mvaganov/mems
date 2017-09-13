#pragma once
#include "string.h"
#include "vector.h"
#include <functional>
using namespace std;

struct Terminal {
	struct CMD {
		String command;
		String helptext;
		function<int(ArrayList<String> & args)> func;
		CMD(String command, String helptext, function<int(ArrayList<String>)> f) : command(command), helptext(helptext), func(f) {}
		bool operator==(CMD const & other) { return command == other.command; }
		// TODO put code to parse arguments string into arg list in this class
	};
	ArrayList<CMD> commands;

	bool add(CMD cmd) {
		int i = commands.indexOf(cmd);
		if (i < 0) {
			commands.add(cmd);
			return true;
		}
		return false;
	}
	bool removeCommand(String commandName) {
		int i = commands.indexOf(CMD(commandName,"",NULL));
		if (i < 0) { return false; }
		commands.remove(i);
		return true;
	}

	int command_help(ArrayList<String> & args) {
		if (args.length() == 1) {
			cout << "Possible commands: ";
			for (int i = 0; i < commands.size(); ++i) {
				if (i > 0) cout << ", ";
				platform_setColor(15, 0);
				cout << commands[i].command;
				platform_setColor(7, 0);
			}
		} else {
			CMD cmd("", "", NULL);
			for (int i = 1; i < args.length(); ++i) {
				platform_setColor(15, 0);
				cout << args[i] << endl;
				platform_setColor(7, 0);
				cmd.command = args[i];
				int idx = commands.indexOf(cmd);
				if (idx >= 0) {
					cout << commands[idx].helptext << endl;
				} else {
					platform_setColor(8, 0);
					cout << "unknown" << endl;
					platform_setColor(7, 0);
				}
			}
		}
		return 0;
	}

	static int FindEndArgumentToken(String str, int i) {
		bool isWhitespace;
		do {
			isWhitespace = String::whitespace().indexOf(str[i]) >= 0;
			if (isWhitespace) { ++i; }
		} while (isWhitespace && i < str.length());
		int index = String::quotes().indexOf(str[i]);
		char startQuote = (index >= 0) ? String::quotes()[index] : '\0';
		if (startQuote != '\0') { ++i; }
		while (i < str.length()) {
			if (startQuote != '\0') {
				if (str[i] == '\\') {
					i++; // skip the next character for an escape sequence. just leave it there.
				}
				else {
					index = String::quotes().indexOf(str[i]);
					bool endsQuote = index >= 0 && String::quotes()[index] == startQuote;
					if (endsQuote) { i++; break; }
				}
			}
			else {
				isWhitespace = String::whitespace().indexOf(str[i]) >= 0;
				if (isWhitespace) { break; }
			}
			i++;
		}
		if (i >= (int)str.length()) { i = (int)str.length(); }
		return i;
	}


	static ArrayList<String> * ParseArguments(String commandLineInput) {
		int index = 0;
		String token;
		ArrayList<String> * tokens = new ArrayList<String>();
		while (index < commandLineInput.length()) {
			int end = FindEndArgumentToken(commandLineInput, index);
			if (index != end) {
				token = commandLineInput.substring(index, end).TrimStart();
				token = String::Unescape(token);
				int qi = String::quotes().indexOf(token[0]);
				if (qi >= 0 && token[token.length() - 1] == String::quotes()[qi]) {
					token = token.substring(1, (int)token.length() - 1);
				}
				tokens->Add(token);
			}
			index = end;
		}
		return tokens;
	}

	/// <param name="command">Command.</param>
	/// <param name="args">Arguments. [0] is the name of the command, with [1] and beyond being the arguments</param>
	void Run(String command, ArrayList<String> * args) {
		CMD cmd(command,"",NULL);
		int commandIndex = commands.indexOf(cmd);
		// try to find the given command. or the default command. if we can't find either...
		if (commandIndex < 0) {
			commandIndex = commands.indexOf(CMD("","",NULL));
			if (commandIndex < 0) {
				// error!
				String error = "Unknown command '" + command + "'";
				if (args->Length() > 1) { error += " with arguments "; }
				for (int i = 1; i < args->Length(); ++i) {
					if (i > 1) { error += ", "; }
					error = error + "'" + (*args)[i] + "'";
				}
				cout << (error);
			}
		}
		// if we have a command
		if (commandIndex >= 0) {
			cmd = commands[commandIndex];
			// execute it if it has valid code
			if (cmd.func != NULL) {
				cmd.func(*args);
			} else {
				cout << "Null command '" << command << "'";
			}
		}
	}

	function<void(String & args)> waitingToReadLine;
	function<void(String & args)> onInput;
	function<void(ArrayList<String> * args)> onCommand;
	void Run(String commandWithArguments) {
		if (waitingToReadLine != NULL) {
			waitingToReadLine(commandWithArguments);
			waitingToReadLine = NULL;
		} else if (onInput != NULL) {
			onInput(commandWithArguments);
		} else {
			if (commandWithArguments.length() == 0) { return; }
			String s = commandWithArguments.Trim(); // cut leading & trailing whitespace
			ArrayList<String> * args = ParseArguments(s);
			//cout << "\"" << commandWithArguments << "\" becomes  args [" << args->join(", ") << "]" << endl;
			if (args->Length() < 1) { return; }
			if (onCommand) { onCommand(args); }
			Run((*args)[0].ToLower(), args);
			delete args;
		}
	}
	Terminal() {
		function<int(ArrayList<String> & args)> f = [this](ArrayList<String> & args)->int {this->command_help(args); return 0; };
		String helptext = "usage: help\nusage: help <command>\nsummary: show list of commands, or help information about a command";
		add(CMD("?", helptext, f));
		add(CMD("help", helptext, f));
	}
};