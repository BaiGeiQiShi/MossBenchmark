#include <iostream>  
#include <assert.h>
#include <cstring>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <regex>
#include <dirent.h>
#include <sys/types.h>
#include <set>

using namespace std;

//Save the used lines
std::unordered_map<int, string> lcount;

//Get file Contents
void readFile(string filename)
{
	//Regex match
	regex pattern("lcount:([0-9]*),1");
	regex testId(".*test_([0-9]*).bin.gcov");
	smatch match;
	smatch matchid;

	//open the input file
	string tempLine;
	fstream file(filename.c_str(), ios::in | ios::out);//ios::in | ios::out represent open file in write and read mod

	if (file.fail())
	{
		cout << "Error: The file doesn't exit!" << endl;
	}

	//get testcase id
	string num;
        if (regex_match(filename,matchid,testId)) {
                num = matchid[1];
        }else{
	    cout << "Cannot identify testcase id!" << endl;
	    exit(1);
	}

	while (getline(file, tempLine)){
		if (regex_match(tempLine,match,pattern)) {
			int lines = stoi(match[1]);
	
			//if the line doesn't exits
			if (lcount.find(lines) == lcount.end()) {
				lcount[lines] = num;
			}
			//Else if the line exits
			else {
				lcount[lines] = lcount[lines] + "," + num;
			}
		}
	}
	file.close();
}

//Get all fileNames
vector<string> getFiles(string cate_dir)
{
	vector<string> files;//save the filenames

	DIR *pDir;
    	struct dirent* ptr;
    	if((pDir = opendir(cate_dir.c_str()))){
    	    while((ptr = readdir(pDir))!=0) {
            	if (strcmp(ptr->d_name, ".") != 0 && strcmp(ptr->d_name, "..") != 0)
		    files.push_back(cate_dir + "/" + ptr->d_name);
    	    }
	}else{
	    cout << cate_dir << " not found!!!" << endl;
	}
    	closedir(pDir);

	//sort by the filename
	sort(files.begin(), files.end());
	return files;
}


int main()
{
	int num = 0;
	string pathname = "bin.gcov";
	vector<string> filenames = getFiles(pathname);
	for (auto name : filenames) {
		readFile(name);
		num++;
	}

	//save the used partitions
//	std::unordered_multimap<string, int> partition;
	//save partition name
//	set<string> part;

	//save the elements
//	for (auto iter = lcount.begin(); iter != lcount.end(); ++iter) {
//		if (part.find(iter->second) == part.end()) {
//			part.insert(iter->second);
//		}
//		partition.insert(pair<string, int>(iter->second,iter->first));
//	}

	//print the partition
//	for (auto it = part.begin(); it != part.end(); ++it){
//		cout << *it <<" Size:"<< partition.count(*it) << endl;
//	}

	for(auto line : lcount){
		cout << line.first << "-" << line.second << endl;
	}

	return 0;
}
