#include <iostream>  
#include <assert.h>
#include <cstring>
#include <fstream>
#include <regex>
#include <set>
#include <map>
#include <vector>
#include <dirent.h>
#include <sys/types.h>

using namespace std;

// Matrix of source file, line and testcase id
map<string, map<int, set<int> > > lcountInfo;

// Get file contents
void readFile(string filename) {
	// Regex match
	regex patternLineNumber("lcount:([0-9]*),1");
	regex patternTestcaseId(".*test_([0-9]*).bin.gcov");
	regex patternSourceFile("file:(.*)");
	smatch matchLineNumber;
	smatch matchId;
	smatch matchSourceFile;

	int currentTestId;
	int lineNumber;
	string currentSourceFile;
	string tempLine;
	string suffixStr = ".real.origin.c";

	// Open the input file
	fstream file(filename.c_str(), ios::in | ios::out);	//ios::in | ios::out represent open file in write and read mod
	if (file.fail()) {
		cout << "Error: The file doesn't exit!" << endl;
	}

	// Get testcase id
    if (regex_match(filename, matchId, patternTestcaseId)) {
		currentTestId = stoi(matchId[1]);
    }
	else {
	    cout << "Cannot identify testcase id!" << endl;
	    exit(1);
	}
	
	// Record the testcase id into the corresponding file and line
	while (getline(file, tempLine)) {
		if (regex_match(tempLine, matchSourceFile, patternSourceFile)) {			
			currentSourceFile = matchSourceFile[1];
			// Strip fixed suffixes from filenames (if needed)
			int suffixStrPos = currentSourceFile.length() - suffixStr.length();
			if (currentSourceFile.rfind(suffixStr) == suffixStrPos ) 
				currentSourceFile = currentSourceFile.substr(0, suffixStrPos);
		}
		if (regex_match(tempLine, matchLineNumber, patternLineNumber)) {
			lineNumber = stoi(matchLineNumber[1]);
			lcountInfo[currentSourceFile][lineNumber].insert(currentTestId);
		}
	}
	
	file.close();
}

// Get all fileNames
vector<string> getFiles(string cate_dir) {
	vector<string> files;	// Save the filenames

	DIR *pDir;
    struct dirent* ptr;
    if ((pDir = opendir(cate_dir.c_str()))) {
    	while((ptr = readdir(pDir))!=0) {
           	if (strcmp(ptr->d_name, ".") != 0 && strcmp(ptr->d_name, "..") != 0)
		    	files.push_back(cate_dir + "/" + ptr->d_name);
    	}
	}
	else {
	    cout << cate_dir << " not found!!!" << endl;
	}
    closedir(pDir);

	sort(files.begin(), files.end());	// Sort by the filename
	return files;
}


int main() {
	int num = 0;
	string pathname = "bin.gcov";
	vector<string> filenames = getFiles(pathname);
	for (auto name : filenames) {
		readFile(name);
		num++;
	}

	// Output lcount information in Json format
	cout << "{" << endl;
	bool fileFirst = true;
	for(auto source : lcountInfo) {
		if (fileFirst) 
			fileFirst = false;
		else 
			cout << "," << endl;
		cout << "\t\"" << source.first << "\": {" << endl;
		map<int, set<int> >::iterator mapi;
		bool mapFirst = true;
		for (mapi = source.second.begin(); mapi != source.second.end(); mapi++) {
			if (mapFirst)
				mapFirst = false;
			else
				cout << "," << endl;
			cout << "\t\t\"" << mapi->first << "\": [";
			set<int>::iterator seti;
			bool setFirst = true;
			for(seti = mapi->second.begin(); seti != mapi->second.end(); seti++) {
				if (setFirst)
					setFirst = false;
				else
					cout << ", ";
				cout << *seti;
			}			
			cout << "]";
		}
		cout << endl << "\t}";
	}
	cout << endl << "}" << endl;

	return 0;
}

