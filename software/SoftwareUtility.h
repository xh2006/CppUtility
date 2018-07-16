

#include <string>
#include <regex>
using namespace std;

class CSoftwareUtility
{

public:
    /**
     *   Description: Version compare function. The version like "X.X.X" , such as "1.2", "1.23.1a" ...
     *   Return int  -1 : parameter error; 0 : equal; 1 : A larger than B; 2 : A smaller than B
     *   Note: We agree that the version "1.2.3.3c" is larger than "1.2.3c"
     */
    static int VersionCompare(const string& strVerA, const string& strVerB);
};