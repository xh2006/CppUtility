

#include "SoftwareUtility.h"

int CSoftwareUtility::VersionCompare(const string& strVerA, const string& strVerB)
{
    regex pattern_version("\\d+(\\.\\d+){1,}[a-z]");    // pattern like "2.23.5n"
    regex pattern_number("\\d+");
    regex pattern_char("[a-zA-Z]");
    smatch mA, mB;

    if (!regex_match(strVerA.c_str(), pattern_version)
        || !regex_match(strVerB.c_str(), pattern_version))
        return -1;

    string strVa = strVerA;
    string strVb = strVerB;
    bool ret_va, ret_vb;

    do
    {
        ret_va = regex_search(strVa, mA, pattern_number);
        ret_vb = regex_search(strVb, mB, pattern_number);

        if(!ret_va || !ret_vb)
            break;

        int nA = atoi(mA.str().c_str());
        int nB = atoi(mB.str().c_str());

        if( nA == nB )
        {
            strVa = mA.suffix();
            strVb = mB.suffix();
        }
        else
        {
            return nA > nB ? 1 : 2;
        }
    }while(ret_va && ret_vb);

    if(ret_va) return 1;
    if(ret_vb) return 2;
    regex_search(strVerA, mA, pattern_char);   
    regex_search(strVerB, mB, pattern_char);
    if (mA.str()[0] == mB.str()[0]) return 0;
    return mA.str()[0] > mB.str()[0] ? 1 : 2;
}