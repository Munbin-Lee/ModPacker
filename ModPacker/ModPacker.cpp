#include "ModPacker.h"
using namespace std;

int extensioncheck(string mypath, string& outpath) {
	int idx = mypath.find_last_of(".");
	if (idx == -1) {
		if (!filesystem::exists(mypath)) return 0;
		outpath = mypath + ".mod";
		return 2;
	}

	string ext = mypath.substr(idx + 1), base = mypath.substr(0, idx);
	if (ext == "mod") {
		outpath = base;
		return 1;
	}
	return 0;
}

int getlength(unsigned char*& data, int& idx) {
	int plen = 0, f = 1;

	while (data[++idx] & 128) {
		plen += (data[idx] & 127) * f;
		f *= 128;
	}
	plen += (data[idx++] & 127) * f;

	return plen;
}

string blockparser(unsigned char*& data, int& idx, int nxt) {
	string res = "";

	while (idx < nxt) {
		if (data[idx] != 18) res += data[idx++];
		else {
			int cur = getlength(data, idx);
			res += blockparser(data, idx, idx + cur);
			res += (unsigned char)0;
		}
	}

	return res;
}

string specialparser(unsigned char*& data, int& idx, int nxt) {
	string res = "";

	while (idx < nxt) {
		if (data[idx] != 18 && data[idx] != 26) res += data[idx++];
		else {
			res += (unsigned char)0;
			int cur = getlength(data, idx);
			res += specialparser(data, idx, idx + cur);
			res += (unsigned char)0;
			if (data[idx] == 34) getlength(data, idx);
		}
	}

	return res;
}

void modextract(string mypath, string outpath) {
	ifstream fin(mypath, ios_base::in | ios_base::binary);
	if (!fin) {
		cout << "Invalid File... Quitting Program...";
		exit(1);
	}

	fin.seekg(0, fin.end);
	int len = (int)fin.tellg();
	fin.seekg(0, fin.beg);

	unsigned char* data = (unsigned char*)malloc(len);
	fin.read((char*)data, len);
	fin.close();

	filesystem::remove_all(outpath);
	
	int depth = 0;
	int idx = 2;
	
	while (idx < len) {
		string resstr = "";
		int plen = getlength(data, idx);
		int nxt = idx + plen;
		
		while (data[idx] != 18) resstr += data[idx++];
		resstr += (unsigned char)0;

		int tmp = getlength(data, idx);
		while (data[idx] != 26) resstr += data[idx++];
		resstr += (unsigned char)0;

		int scriptlen = getlength(data, idx);
		string scriptid = "";
		while (scriptlen--) scriptid += data[idx++];
		resstr += scriptid;
		resstr += (unsigned char)0;

		int pathlen = getlength(data, idx);
		string innerpath = "";
		while (pathlen--) innerpath += data[idx++];
		resstr += innerpath;
		resstr += (unsigned char)0;
		
		string category = scriptid.substr(0, scriptid.find(":"));
		string entryid = scriptid.substr(scriptid.find_last_of("/") + 1);

		tmp = getlength(data, idx);
		if (data[idx] == 8) {
			tmp = getlength(data, idx);
			resstr += specialparser(data, idx, nxt);
		}
		else resstr += blockparser(data, idx, nxt);

		string newpath = outpath + "\\" + category;
		filesystem::create_directories(newpath);

		string newfile = newpath + "\\" + category + "-" + entryid + ".txt";
		ofstream fout(newfile, ios::binary | std::ios::trunc);
		fout << resstr;
		fout.close();
	}
}

string getbyte(int len) {
	string res = "";

	while (len) {
		unsigned char cur = len % 128;
		len /= 128;
		if (len) cur |= 128;
		res += cur;
	}

	return res;
}

string blockconcat(vector<string>& v) {
	string res = v[0];
	res += (unsigned char)18;
	res +=(unsigned char)32;
	res += v[1];
	res += (unsigned char)26;
	res += getbyte(v[2].size());
	res += v[2];
	res += (unsigned char)34;
	res += getbyte(v[3].size());
	res += v[3];
	
	string inner = "";
	inner += (unsigned char)18;
	inner += getbyte(v[4].size());
	inner += v[4];
	inner = '2' + getbyte(inner.size()) + inner;

	res += inner + v[5];
	res = getbyte(res.size()) + res;
	res.insert(0, 1, (unsigned char)18);

	return res;
}

string specialconcat(vector<string>& v) {
	string res = v[0];
	res += (unsigned char)18;
	res += (unsigned char)32;
	res += v[1];
	res += (unsigned char)26;
	res += getbyte(v[2].size());
	res += v[2];
	res += (unsigned char)34;
	res += getbyte(v[3].size());
	res += v[3];

	int idx = 4;
	string inner = "";
	inner += (unsigned char)8;
	inner += (unsigned char)1;
	while (idx < v.size() - 1) {
		string tmp = v[idx++];
		tmp += (unsigned char)18;
		tmp += getbyte(v[idx].size());
		tmp += v[idx++];
		tmp += (unsigned char)26;
		tmp += getbyte(v[idx].size());
		tmp += v[idx++];
		if (v[idx][0] != 26 && v[idx][0] != 88 && v[idx][1] != 1) {
			tmp += (unsigned char)34;
			tmp += getbyte(v[idx].size());
			tmp += v[idx++];
		}
		tmp = getbyte(tmp.size()) + tmp;
		tmp.insert(0, 1, (unsigned char)26);
		inner += tmp;
	}

	inner = '2' + getbyte(inner.size()) + inner;

	res += inner + v.back();
	res = getbyte(res.size()) + res;
	res.insert(0, 1, (unsigned char)18);

	return res;
}

void modpack(string mypath, string outpath) {
	filesystem::remove(outpath);
	ofstream fout(outpath, ios::binary);
	fout << (unsigned char)10 << (unsigned char)0;

	for (const auto& file : filesystem::recursive_directory_iterator(mypath)) {
		if (file.is_directory()) continue;
		string pn = file.path().string();
		string category = file.path().filename().string();
		category = category.substr(0,category.find("-"));

		ifstream fin(pn, ios_base::in | ios_base::binary);
		if (!fin) {
			cout << "Invalid File... Quitting Program...";
			exit(1);
		}

		fin.seekg(0, fin.end);
		int len = (int)fin.tellg();
		fin.seekg(0, fin.beg);

		unsigned char* data = (unsigned char*)malloc(len);
		fin.read((char*)data, len);
		fin.close();

		vector<string> v;
		string cur = "";
		for (int i = 0; i < len; i++) {
			if (data[i] != 0) cur += data[i];
			else if (cur.size()) {
				v.push_back(cur);
				cur = "";
			}
		}
		if (cur.size()) v.push_back(cur);

		if (category == "gamelogic" || category == "map" || category == "ui") fout << specialconcat(v);
		else fout << blockconcat(v);
	}
	fout.close();
}

int main(int argc, char** argv) {
	string mypath, outpath;
	if (argc >= 2) mypath = argv[1];
	else {
		cout << "Input Path to .mod or Directory and Press Enter...\n";
		getline(cin, mypath);
	}

	int ext = extensioncheck(mypath, outpath);
	if (!ext) {
		cout << "Invalid Extension or Path... Quitting Program...";
		return 1;
	}

	if (ext == 1) modextract(mypath, outpath);
	else modpack(mypath, outpath);

	cout << "Complete!";
}
