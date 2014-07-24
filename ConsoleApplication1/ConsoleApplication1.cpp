// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <direct.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <utility>
#include <memory>

using namespace std;

string fileName, poisonArg;

string GetFileName() {
	char buf[MAX_PATH+1] = {0};
	if (!GetModuleFileNameA(NULL, buf, MAX_PATH)) {
		return string();
	}
	char* lastBackslash = strrchr(buf, '\\');
	char* lastDot = strrchr(buf, '.');
	if (!lastBackslash) {
		return string();
	}
	if (lastDot > lastBackslash) {
		*lastDot = 0;
	}
	return lastBackslash + 1;
}

void WriteArgsToFile(int argc, char* argv[], const char* tmpName, const char* cwd)
{
	FILE* f;
	fopen_s(&f, tmpName, "w");
	fprintf(f, "Current working directory: %s\n", cwd);
	for (int i = 0; i < argc; ++i) {
		bool hasSpecial = strchr(argv[i], ' ');
		fprintf(f, "%s%s%s%s", hasSpecial ? "\"" : "", argv[i],
			    hasSpecial ? "\"" : "", i == (argc - 1) ? "" : " ");
	}
	fclose(f);
}

unique_ptr<char[]> GetCommandLineString(int argc, char* argv[], vector<int>& poison)
{
	vector<string> v;
	bool isDashI = false;
	for (int i = 1; i < argc; ++i) {
		string str;
		bool hasSpecial = false;
		if (isDashI) {
			isDashI = false;
			str = "-I ";
			hasSpecial = true;
		}
		if (!poisonArg.empty() && !!strstr(argv[i], poisonArg.c_str())) {
			poison.push_back(i);
		}
		char* s = argv[i];
		while (*s) {
			switch(*s) {
			case '\\':
			case '"':
				str += '\\';
			case ' ':
				hasSpecial = true;
			default:
				str += *s;
			}
			++s;
		}
		// cl-specific hack!
		if (fileName == "cl" && str == "-I") {
			isDashI = true;
			continue;
		}
		if (hasSpecial) {
			str = "\"" + str + "\"";
		}
		v.push_back(str);
	}
	ostringstream cmdline;
	copy(v.begin(), v.end(), ostream_iterator<string>(cmdline, " "));
	unique_ptr<char[]> cmdline_str(new char[cmdline.str().size() + 1]);
	strcpy_s(cmdline_str.get(), cmdline.str().size() + 1, cmdline.str().c_str());
	return cmdline_str;
}

int main(int argc, char* argv[])
{
	fileName = GetFileName();
	char realPath[MAX_PATH+1] = {0};
	if (!GetEnvironmentVariableA((fileName + "_cmd").c_str(), realPath, MAX_PATH)) {
		fprintf(stderr, "Could not locate environment variable %s\n", fileName.c_str());
		return 1;
	}
	SetEnvironmentVariableA((fileName + "_cmd").c_str(), NULL);

	char poison[1024];
	if (GetEnvironmentVariableA((fileName + "_poison").c_str(), poison, 1024) < 1024) {
		poisonArg = poison;
	}

	char* tmpName = _tempnam("", fileName.c_str());
	char cwd[1024] = {0};
	_getcwd(cwd, 1024);

	WriteArgsToFile(argc, argv, tmpName, cwd);

	vector<int> poisoned;
	auto cmdline_str = GetCommandLineString(argc, argv, poisoned);
	if (poisoned.size()) {
		fprintf(stderr, "Poisoned args found at index ");
		for (auto& i : poisoned) {
			fprintf(stderr, "%d ", i);
		}
		fprintf(stderr, "\nArguments: %s\n", cmdline_str.get());
		return 254;
	}

	STARTUPINFOA si = {0};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {0};
	HANDLE stdoutr, stdoutw;
	HANDLE stderrr, stderrw;
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), 0, 1 };
	CreatePipe(&stdoutr, &stdoutw, &sa, 0);
	SetHandleInformation(stdoutr, HANDLE_FLAG_INHERIT, 0);
	CreatePipe(&stderrr, &stderrw, &sa, 0);
	SetHandleInformation(stderrr, HANDLE_FLAG_INHERIT, 0);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = stdoutw;
	si.hStdError = stderrw;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	CreateProcessA(realPath,
		           cmdline_str.get(),
				   0, 0,
				   TRUE,
				   0,
				   0,
				   cwd,
				   &si, &pi);
	CloseHandle(pi.hThread);
	CloseHandle(si.hStdOutput);
	CloseHandle(si.hStdError);
	CloseHandle(si.hStdInput);
	typedef pair<char*, DWORD> Buf;
	vector<Buf> bufs_out, bufs_err;
	for (;;) {
		DWORD read = 0;
		char* buf = new char[10240];
		bool res = ReadFile(stdoutr, buf, 10240, &read, NULL);
		if (!res || !read) break;
		bufs_out.push_back(make_pair(buf, read));
	}
	for (;;) {
		DWORD read = 0;
		char* buf = new char[10240];
		bool res = ReadFile(stderrr, buf, 10240, &read, NULL);
		if (!res || !read) break;
		bufs_err.push_back(make_pair(buf, read));
	}
	for (auto& b : bufs_out) {
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), b.first, b.second, &b.second, NULL);
	}
	for (auto& b : bufs_err) {
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), b.first, b.second, &b.second, NULL);
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exitCode;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hProcess);
	return exitCode;
}

