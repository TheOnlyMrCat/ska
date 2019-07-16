//
//  main.cpp
//  Ska interpreter
//
//  Created by Maxwell Guppy on 15/7/19.
//  Copyright © 2019 Dockdev. All rights reserved.
//

#include "cxxopts.hpp"

#include <fstream>
#include <iostream>
#include <stack>
#include <unordered_map>

std::string UnicodeToUTF8(unsigned int codepoint)
{
	std::string out;
	
	if (codepoint <= 0x7f)
		out.append(1, static_cast<char>(codepoint));
	else if (codepoint <= 0x7ff)
	{
		out.append(1, static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
		out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
	}
	else if (codepoint <= 0xffff)
	{
		out.append(1, static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
		out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
		out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
	}
	else
	{
		out.append(1, static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07)));
		out.append(1, static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
		out.append(1, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
		out.append(1, static_cast<char>(0x80 | (codepoint & 0x3f)));
	}
	return out;
}

class input {
public:
	virtual ~input() {}
	
	virtual char get() {return 0;};
	virtual bool good() {return false;}
};

class fsinput : public input {
	std::ifstream *stream;
public:
	fsinput(std::ifstream *stream) : stream(stream) {}
	~fsinput() override {delete stream;}
	
	char get() override
	{
		return stream->get();
	}
	
	bool good() override
	{
		return stream->good();
	}
};

class cinput : public input {
public:
	char get() override
	{
		return std::cin.get();
	}
	
	bool good() override
	{
		return std::cin.good();
	}
};

class fninput : public input {
	std::string function;
	int pointer = 0;
public:
	fninput(std::string data) : function(data) {}
	
	char get() override
	{
		return function[pointer++]; // Yes, I am aware that this is a postfix increment
	}
	
	bool good() override
	{
		return pointer < function.length();
	}
};

void error(int line, int col, std::string msg)
{
	std::cerr << "Ska (" << line << ":" << col << "): " << msg << std::endl;
	throw "this ball";
}

int main(int argc, char *argv[])
{
	cxxopts::Options parser = cxxopts::Options("Ska interpreter");
	
	parser.add_options()
	("f,file", "Name of the file to be run", cxxopts::value<std::string>())
	("e,explain", "Prints an explanation of the program to the standard output")
	;
	
	auto options = parser.parse(argc, argv);
	
	std::stack<input*> inputStack;
	
	if (options.count("file")) {
		std::ifstream *ifs = new std::ifstream(options["file"].as<std::string>());
		if (ifs->fail()) {
			delete ifs;
			std::cerr << "Could not open file \"" << options["file"].as<std::string>() << "\"" << std::endl;
			exit(EXIT_FAILURE);
		}
		inputStack.push(new fsinput(ifs));
	} else {
		inputStack.push(new cinput());
	}
	
#define in inputStack.top()
	
	//Declaration of interpreter variables
	//- Data about position and token
	char c = in->get();
	
	int line = 1, col = 1;
	std::stack<std::pair<int, int>> locStack;
	
//	std::stack<std::pair<int, int>> loopStack;
	
	//Memory
	std::unordered_map<std::string, int> memory;
	std::unordered_map<std::string, std::pair<std::string, std::pair<int, int>>> functions;
	
	//"Registers"
	int accumulator = 0;
	int counter = 0;
	
	std::string stringSrc;
	std::string stringDest;
	
	try {
		while (!inputStack.empty()) {
			if (!in->good()) {
				delete in;
				inputStack.pop();
				if (inputStack.empty()) break;
				std::pair<int, int> newLoc = locStack.top();
				line = newLoc.first;
				col = newLoc.second;
				locStack.pop();
			}
			switch (c) {
				//Memory storage
				case 's': // Stores the accumulator into the location specified by stringDest
					memory.emplace(stringDest, accumulator);
					break;
				case 'l': // Loads the value at stringSrc into the accumulator
					if (stringSrc.length() < 1) error(line, col, "No string passed to load");
					try {
						accumulator = memory.at(stringSrc);
					} catch (std::out_of_range) {
						error(line, col, "No memory at specified location");
					}
					break;
				
				//Numeric values
				case '0': // Multiplies the counter by 10, then adds 0
				case '1': // --------------------------------------- 1
				case '2': // --------------------------------------- 2
				case '3': // --------------------------------------- 3
				case '4': // --------------------------------------- 4
				case '5': // --------------------------------------- 5
				case '6': // --------------------------------------- 6
				case '7': // --------------------------------------- 7
				case '8': // --------------------------------------- 8
				case '9': // --------------------------------------- 9
					counter *= 10;
					counter += c - 48;
					break;
					
				//Printing
				case 'p': // Prints the current value of stringSrc
					std::cout << stringSrc << std::flush;
					break;
				case 'o': // Prints the current value of the accumulator
					std::cout << accumulator << std::flush;
					break;
				case 'i': // Prints a newline
					std::cout << std::endl;
					break;
				
				//Strings
				case '"': // Loads a string into stringSrc
					{
						std::string buffer;
						do {
							c = in->get();
							col++;
							switch (c) {
								case '\\':
									c = in->get();
									col++;
									switch (c) {
										case 'a':
											buffer += '\a';
											break;
										case 'b':
											buffer += '\b';
											break;
										case 'e':
											buffer += '\e';
											break;
										case 'f':
											buffer += '\f';
											break;
										case 'n':
											buffer += '\n';
											break;
										case 'r':
											buffer += '\r';
											break;
										case 't':
											buffer += '\t';
											break;
										case 'u':
											try {
												int unicode = stoi(std::string()+={in->get(), in->get(), in->get(), in->get()}, nullptr, 16);
												std::string utf8 = UnicodeToUTF8(unicode);
												buffer += utf8;
											} catch (std::invalid_argument) {
												error(line, col, "Invalid hexadecimal value for \\u");
											}
											col += 4;
											break;
										case 'U':
											try {
												int unicode = stoi(std::string()+={in->get(), in->get(), in->get(), in->get(), in->get(), in->get(), in->get(), in->get()}, nullptr, 16);
												std::string utf8 = UnicodeToUTF8(unicode);
												buffer+= utf8;
											} catch (std::invalid_argument) {
												error(line, col, "Invalid hexadecimal value for \\U");
											}
											col += 8;
											break;
										case 'v':
											buffer += '\v';
											break;
										case 'x':
											try {
												int unicode = stoi(std::string()+={in->get(), in->get()}, nullptr, 16);
												std::string utf8 = UnicodeToUTF8(unicode);
												buffer += utf8;
											} catch (std::invalid_argument) {
												error(line, col, "Invalid hexadecimal value for \\x");
											}
											col += 2;
											break;
										case '\\':
											buffer += '\\';
											break;
										case '"':
											buffer += '"';
											break;
										default:
											error(line, col, "Invalid escape sequence");
									}
									break;
								case '"':
									break;
								default:
									buffer += c;
							}
						} while (c != '"' && in->good());
						stringSrc = buffer;
					}
					break;
				case '\'': // Loads a string into stringDest
					{
					std::string buffer;
					do {
						c = in->get();
						col++;
						switch (c) {
							case '\\':
								c = in->get();
								col++;
								switch (c) {
									case 'a':
										buffer += '\a';
										break;
									case 'b':
										buffer += '\b';
										break;
									case 'e':
										buffer += '\e';
										break;
									case 'f':
										buffer += '\f';
										break;
									case 'n':
										buffer += '\n';
										break;
									case 'r':
										buffer += '\r';
										break;
									case 't':
										buffer += '\t';
										break;
									case 'u':
										try {
											int unicode = stoi(std::string()+={in->get(), in->get(), in->get(), in->get()}, nullptr, 16);
											std::string utf8 = UnicodeToUTF8(unicode);
											buffer += utf8;
										} catch (std::invalid_argument) {
											error(line, col, "Invalid hexadecimal value for \\u");
										}
										col += 4;
										break;
									case 'U':
										try {
											int unicode = stoi(std::string()+={in->get(), in->get(), in->get(), in->get(), in->get(), in->get(), in->get(), in->get()}, nullptr, 16);
											std::string utf8 = UnicodeToUTF8(unicode);
											buffer+= utf8;
										} catch (std::invalid_argument) {
											error(line, col, "Invalid hexadecimal value for \\U");
										}
										col += 8;
										break;
									case 'v':
										buffer += '\v';
										break;
									case 'x':
										try {
											int unicode = stoi(std::string()+={in->get(), in->get()}, nullptr, 16);
											std::string utf8 = UnicodeToUTF8(unicode);
											buffer += utf8;
										} catch (std::invalid_argument) {
											error(line, col, "Invalid hexadecimal value for \\x");
										}
										col += 2;
										break;
									case '\\':
										buffer += '\\';
										break;
									case '\'':
										buffer += '\'';
										break;
									default:
										error(line, col, "Invalid escape sequence");
								}
								break;
							case '\'':
								break;
							default:
								buffer += c;
						}
					} while (c != '\'' && in->good());
					stringDest = buffer;
				}
					break;
				case 't': // Swaps stringSrc and stringDest
					{
						std::string temp = stringSrc;
						stringSrc = stringDest;
						stringDest = temp;
					}
					break;
					
				//Arithmetic
				case 'a':
					accumulator += counter;
					break;
				case 'm':
					accumulator -= counter;
					break;
				case 'x':
					accumulator *= counter;
					break;
				case 'd':
					accumulator /= counter;
					break;
				case 'r':
					accumulator %= counter;
					break;
				case 'z':
					counter = 0;
					break;
				case 'c':
					{
						int temp = counter;
						counter = accumulator;
						accumulator = temp;
					}
					break;
					
				//Flow control
				case '[':
//					loopStack.push(std::pair<int, int>(line, col));
					break;
				case ']':
					error(line, col, "Unmatched closing bracket");
					break;
				case '{':
					{
						c = in->get();
						col++;
						std::string fndata;
						std::pair<int, int> location = std::make_pair(line, col - 1 /*Can someone tell me why I need to subtract 1?*/);
						while (c != '}') {
							fndata += c;
							c = in->get();
							col++;
						}
						functions[stringDest] = std::make_pair(fndata, location);
					}
					break;
				case '}':
					error(line, col, "Unmatched closing brace");
					break;
				case 'n':
					if (accumulator != 0) {
						c = in->get();
						col++;
					}
					break;
				case 'b':
					if (accumulator == 0) {
						c = in->get();
						col++;
					}
					break;
				case 'g':
					if (accumulator <= 0) {
						c = in->get();
						col++;
					}
					break;
				case 'h':
					if (accumulator >= 0) {
						c = in->get();
						col++;
					}
					break;
				case 'q':
					{
						try {
							auto f = functions.at(stringSrc);
							inputStack.push(new fninput(f.first));
							locStack.push(std::make_pair(line, col));
							
							line = f.second.first;
							col = f.second.second;
						} catch (std::out_of_range) {
							error(line, col, "No function with specified name");
						}
					}
					break;
				
				//Miscellaneous
				case '(':
					while ((c = in->get()) != ')') col++;
					break;
				case ' ':
					break;
				case '\n':
					line++;
					col = 0;
					break;
				default:
					error(line, col, "Unrecognised token");
			}
			c = in->get();
			col++;
		}
	} catch (const char*) {}
	
	while (!inputStack.empty()) {
		delete in;
		inputStack.pop();
	}
}
