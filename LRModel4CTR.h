#ifndef _LRMODEL4CTR_H_
#define _LRMODEL4CTR_H_

#include<cmath>
#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include <stdlib.h>

#include "config.h"
#include "Module.h"


using namespace std;

struct Feature{
	int index;
	double value;
};

struct Sample{
	int id;
	vector<Feature> feature_vec;
};

class LRModel
{
public:
	bool init(const string config_file);
	
	vector<double> getModel(){return model;};
	void setModel(vector<double> model){this->model = model;};
	int getN_feature(){return this->n_feature;};
	void setN_feature(int n){this->n_feature = n;}
	double getMax_price(){return this->max_price;}
	void setMax_price(double price){this->max_price = price;}
	double getAlpha(){return this->alpha;}
	void setAlpha(double a){this->alpha = a;}
	string getModel_file(){return this->model_file;}
	void setModel_file(string filepath){this->model_file = filepath;}
	
	bool loadModel();
	double normalizedFeature(double val, double max_val, bool is_log);
	Sample featuresProcess(int pid, double features[]);
	double estimateCTR(Sample sample);
private:
	vector<double> model;
	int n_feature;
	string model_file;
	double max_price;
	double alpha;
};

#endif
