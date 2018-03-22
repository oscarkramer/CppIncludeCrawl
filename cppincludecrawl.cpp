#include <iostream>
#include <unistd.h>
#include <glob.h>
#include <vector>
#include <map>
#include <fstream>
#include <sys/stat.h>
#include <libgen.h>

using namespace std;

bool mineCMake (const string& build_dir, string& ossimDevHome, vector<string>& includePaths);
void searchSet (const string& pattern, map<string, string>& headerMap);
void searchFile(const string& file,    map<string, string>& headerMap);
void matchHeadersToPaths(const vector<string>& includePaths, map<string, string>& headerMap);
bool copyHeaders(const map<string, string>& headerMap);

int main(int argc, char** argv)
{
   // Usage: <argv[0]> <path/to/OSSIM_DEV_HOME> <path-to-output.txt>
   if (argc < 3)
   {
      cout << "\nUsage:  " << argv[0] << " <path/to/OSSIM_BUILD_DIR> <path-to-output.txt>\n" << endl;
      return 1;
   }

   string buildPath = argv[1];
   string destFile = argv[2];

   // Resurrect if options needed:
   //char *nvalue = "World";
   //int c=0;
   //int tvalue = -1;
   //while ((c = getopt (argc, argv, "nt:")) != -1)
   //   switch(c)
   //   {
   //   case 'n':
   //      nvalue = optarg;
   //      break;
   //   case 't':
   //      tvalue = atoi(optarg);
   //      break;
   //   }

   string cppFilePattern ("*.cpp");
   string cFilePattern ("*.c");
   vector<string> includePaths;
   map<string, string> headerMap;
   string ossimDevHome;

   if (!mineCMake(buildPath, ossimDevHome, includePaths))
      return -1;

   char cwd[1024];
   getcwd(cwd, 1023);
   if (chdir(ossimDevHome.c_str()) < 0)
   {
      cout<<"Failed to open OSSIM_DEV_HOME directory: "<<ossimDevHome<<". Check permissions."<<end;
      return -1;
   }

   searchSet(cppFilePattern, headerMap);
   searchSet(cFilePattern, headerMap);
   matchHeadersToPaths(includePaths, headerMap);
   copyHeaders(headerMap);

   chdir(cwd);
   return 0;
}

bool mineCMake(const string& build_dir, string& ossimDevHome, vector<string>& includePaths)
{
   static const string OSSIM_DEV_HOME_STR = "OSSIM_DEV_HOME:";
   static const int OSSIM_DEV_HOME_STR_SIZE = OSSIM_DEV_HOME_STR.length();

   string cmakeCacheFile = build_dir + "/CMakeCache.txt";
   ifstream f(cmakeCacheFile);
   if (f.fail())
   {
      cout << "Failed file open for CMake file: " << cmakeCacheFile << end;
      return false;
   }

   // Loop to read read one line at a time to find OSSIM_DEV_HOME:
   string line;
   while (std::getline(f, line))
   {
      if (line.empty())
         continue;

      // Line begins with include?
      string label = line.substr(0, OSSIM_DEV_HOME_STR_SIZE);
      if (label != OSSIM_DEV_HOME_STR)
         continue;

      // This is the OSSIM_DEV_HOME definition:
      size_t equalPos = label.find_first_of('=');
      if (equalPos == 0)
      {
         cout << "! Could not find OSSIM_DEV_HOME in CMakeCache file!" << endl;
         break;
      }
      string ossimDevHome = line.substr(equalPos + 1);
   }
   f.close();
   if (ossimDevHome.empty())
      return false;

   // Now scan for include path specs:
   f.open(cmakeCacheFile);
   while (std::getline(f, line))
   {
      if (line.empty())
         continue;

      size_t pos = line.find_first_of("_INCLUDE_");
      if (pos == 0)
         continue;

      // Found include spec:
      pos = line.find_first_of("PATH=");
      if (pos == 0)
         continue;
      string includePath = line.substr(pos + 5);
      cout << "includePath=" << includePath;
      includePaths.emplace_back(includePath);
   }
   f.close();
   return true;
}


// Called recursively
void searchSet(const string& pattern, map<string, string>& headerMap)
{
   // First find all files that match pattern:
   glob_t glob_result;
   glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
   vector<string> files;
   for (unsigned int i = 0; i < glob_result.gl_pathc; ++i)
   {
      files.push_back(string(glob_result.gl_pathv[i]));
   }
   globfree(&glob_result);

   // With list of files, open each and look for includes:
   for (auto &xfile : files)
   {
      searchFile(xfile, ossimDevHomeheaderMap);
   }
}

void searchFile(const string& xfile, map<string, string>& headerMap)
{
   static const string INCLUDE_STRING = "#include ";
   static const int SIZE_INCLUDE_STR = INCLUDE_STRING.length();

   // Open one file:
   ifstream f (xfile);
   if (f.fail())
   {
      cout<<"Failed file open for: "<<xfile<<end;
   }

   bool foundInclude;
   int noIncludeCount = 0;
   string lineOfCode;

   // Loop to read read one line at a time to check for includes:
   while ((std::getline(f, lineOfCode) && (noIncludeCount < 10))
   {
      if ((lineOfCode.empty() || lineOfCode[0] != '#'))
      {
         if (foundInclude)
            noIncludeCount++;
         continue;
      }

      // Line begins with include?
      string includeStr = lineOfCode.substr(0, SIZE_INCLUDE_STR + 1);
      if (includeStr != INCLUDE_STRING)
      {
         if (foundInclude)
            noIncludeCount++;
         continue;
      }

      // This is an include statement parse and save if not done already:
      string includePathX = lineOfCode.substr(SIZE_INCLUDE_STR);
      size_t closePos = includePathX.find_last_of('>');
      if (closePos == 0)
         closePos = includePathX.find_last_of('"');
      if (closePos == 0)
      {
         cout << "! Could not find closing character!" << endl;
         continue;
      }

      // Fetch the filename part of the include path:
      string includePath = includePathX.substr(0, closePos);
      cout << "includePath = " << includePath << endl;
      size_t lastSlashPos = includePath.find_last_of('/');
      string includeFile = includePath.substr(lastSlashPos + 1);
      cout << "includeFile = " << includeFile << endl;
      noIncludeCount = 0;

      // Search the map if already entered:
      auto mapEntry = headerMap.find(includeFile);
      if (mapEntry == headerMap.end())
         headerMap.emplace(includeFile, includePath);
   }

   f.close();

}

void matchHeadersToPaths(const vector<string>& includePaths, map<string, string>& headerMap)
{
   // headerMap is updated to reflect full paths to all headers
   for (auto &header : headerMap)
   {
      for (auto &includePath : includePaths)
      {
         string fullPath = includePath + header.second;
         ifstream f(fullPath);
         if (f.good())
            header.second = fullPath;
         f.close();
      }
   }
}

bool copyHeaders(const map<string, string>& headerMap)
{
   for (auto &header : headerMap)
   {
      string fullPath = header.second;
      size_t pos = fullPath.find_last_of('/');
      char* pathOnly = dirname(fullPath.c_str());
      mkdir(pathOnly, 0x777);
   }

}