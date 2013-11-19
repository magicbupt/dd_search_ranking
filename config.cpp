/*
anthor:maorui
get the cinfig from file
*/
#include "config.h"

//split str with sp to out
void split(const string& str, const string& sp, vector<string>& out) {
    out.clear();
    string s = str;
    size_t beg, end;
    while (!s.empty()) {
        beg = s.find_first_not_of(sp);
        if (beg == string::npos) {
            break;
        }
        end = s.find(sp, beg);
        out.push_back(s.substr(beg, end - beg));
        if (end == string::npos) {
            break;
        }
        s = s.substr(end, s.size() - end);
    }
}

 
/*   删除左边的空格   */
char *l_trim(char * szOutput, const char *szInput)
{
	assert(szInput != NULL);
	assert(szOutput != NULL);
	assert(szOutput != szInput);
	for( ; *szInput != '\0' && isspace(*szInput); ++szInput){
		;
	}
	return strcpy(szOutput, szInput);
}
 
/*   删除右边的空格   */
char *r_trim(char *szOutput, const char *szInput)
{
	char *p = NULL;
	assert(szInput != NULL);
	assert(szOutput != NULL);
	assert(szOutput != szInput);
	strcpy(szOutput, szInput);
	for(p = szOutput + strlen(szOutput) - 1; p >= szOutput && isspace(*p); --p){
		;
	}
	*(++p) = '\0';
	return szOutput;
}
 
/*   删除两边的空格   */
char *a_trim(char * szOutput, const char * szInput)
{
	char *p = NULL;
	assert(szInput != NULL);
	assert(szOutput != NULL);
	l_trim(szOutput, szInput);
	for   (p = szOutput + strlen(szOutput) - 1;p >= szOutput && isspace(*p); --p){
		;
	}
	*(++p) = '\0';
	return szOutput;
}
 
 
int GetProfileString(FILE *fp, char *AppName, char *KeyName, char *KeyVal )
{
	char appname[MAX_SECTION_LEN],keyname[MAX_KEYWORD_LEN];
	char *buf,*c;
	char buf_i[KEYVALLEN], buf_o[KEYVALLEN];
	int found=0; /* 1 AppName 2 KeyName */
	fseek( fp, 0, SEEK_SET);
	memset( appname, 0, sizeof(appname) );
	snprintf( appname,MAX_SECTION_LEN,"[%s]", AppName );
 
	while( !feof(fp) && fgets( buf_i, KEYVALLEN, fp )!=NULL ){
		l_trim(buf_o, buf_i);
		if( strlen(buf_o) <= 0 )
			continue;
		buf = NULL;
		buf = buf_o;
		if( found == 0 ){
			if( buf[0] != '[' ) {
				continue;
			} else if ( strncmp(buf,appname,strlen(appname))==0 ){
				found = 1;
				continue;
			}
 
		} else if( found == 1 ){
			if( buf[0] == '#' ){
				continue;
			} else if ( buf[0] == '[' ) {
				break;
			} else {
				if( (c = (char*)strchr(buf, '=')) == NULL )
					continue;
				memset( keyname, 0, sizeof(keyname) );
				sscanf( buf, "%[^=|^ |^\t]", keyname );
				if( strcmp(keyname, KeyName) == 0 ){
					sscanf( ++c, "%[^\n]", KeyVal );
					char *KeyVal_o = (char *)malloc(strlen(KeyVal) + 1);
					if(KeyVal_o != NULL){
						memset(KeyVal_o, 0, sizeof(KeyVal_o));
						a_trim(KeyVal_o, KeyVal);
						if(KeyVal_o && strlen(KeyVal_o) > 0)
							strcpy(KeyVal, KeyVal_o);
						free(KeyVal_o);
						KeyVal_o = NULL;
					}
					found = 2;
					break;
				} else {
					continue;
				}	
			}
		}
	}
	if( found == 2 )
		return 0;
	else
		return -1;
}


int GetProfileInt(FILE *fp, char *AppName, char *KeyName, int *KeyNum)
{
	char keyval[KEYVALLEN] = {'\0'};
	if(GetProfileString(fp, AppName, KeyName, keyval) < 0)
		return -1;
	if(0 == strlen(keyval))
		return -1;
	*KeyNum = atoi(keyval);

	return 0;
} 

int GetProfileDouble(FILE *fp, char *AppName, char *KeyName, double *KeyNum)
{
	char keyval[KEYVALLEN] = {'\0'};
	if(GetProfileString(fp, AppName, KeyName, keyval) < 0)
		return -1;
	if(0 == strlen(keyval))
		return -1;
	*KeyNum = atof(keyval);

	return 0;
}

int GetProfileIntArray(FILE *fp, char *AppName, char *KeyName, int *arr)
{
	char keyval[KEYVALLEN] = {'\0'};
    if(GetProfileString(fp, AppName, KeyName, keyval) < 0)
        return -1;
    if(0 == strlen(keyval))
        return -1;
	string str = keyval;
	string sp = ",";
	vector<string> out;
	split(str, sp, out);
	
	for(int i = 0; i <out.size(); i++){
		arr[i] = atoi(out[i].c_str());
	}
	return 0;
}

int GetProfileStringVector(FILE *fp, char *AppName, char *KeyName, vector<string>& vec)
{
	char keyval[KEYVALLEN] = {'\0'};
	if(GetProfileString(fp, AppName, KeyName, keyval) < 0)
		return -1;
    if(0 == strlen(keyval))
        return -1;
	string str = keyval;
	string sp = ",";
	split(str, sp, vec);
	return 0;	
}
 
int example()
{
	char *profile = "./cls.conf"; 
	FILE *fp;
	int port = 0;
	char host[32] = {'\0'};
	if( (fp=fopen( profile,"r" ))==NULL ){
		printf( "openfile [%s] error [%s]\n",profile,strerror(errno) );
		return -1;
	}
	if (GetProfileInt(fp, "server", "port", &port)<0)
		return -1;
	if(GetProfileString(fp, "redis", "host",host)<0)
		return -1;
	fclose( fp );
	
	return 0;
}
