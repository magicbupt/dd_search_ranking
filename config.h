#ifndef DDC_CONFIG_H_
#define DDC_CONFIG_H_ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
 
#define KEYVALLEN 256
#define MAX_SECTION_LEN  32
#define MAX_KEYWORD_LEN      32

#include<iostream>
#include<vector>
#include<string>

using namespace std;


char *l_trim(char * szOutput, const char *szInput);

char *r_trim(char *szOutput, const char *szInput);
char *a_trim(char * szOutput, const char * szInput);
int GetProfileString(FILE *fp, char *AppName, char *KeyName, char *KeyVal );

int GetProfileDouble(FILE *fp, char *AppName, char *KeyName, double *KeyNum);
int GetProfileInt(FILE *fp, char *AppName, char *KeyName, int *KeyNum);
int GetProfileIntArray(FILE *fp, char *AppName, char *KeyName, int *arr);
int GetProfileStringVector(FILE *fp, char *AppName, char *KeyName, vector<string>& vec);

void split(const string& str, const string& sp, vector<string>& out); 

#endif
