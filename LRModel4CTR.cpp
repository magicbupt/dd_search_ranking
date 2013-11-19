#include "LRModel4CTR.h"

#include "GlobalDef.h"
#include "SearchGlobal.h"

#include<iostream>

using namespace std;

bool LRModel::init(const string config_file)
{
	//init conf
    const char *profile = config_file.c_str(); // default "./model.conf";
    FILE *fp;
    if ((fp = fopen(profile, "r")) == NULL) {
        return false;
    }
	
	// read n_feature
    if (GetProfileInt(fp, "ctr_model", "feature_num", &(this->n_feature)) < 0)
        return false;
    char filepath[256] = {0};
    if (GetProfileString(fp, "ctr_model", "model_file", filepath) < 0)
        return false;
    this->model_file = filepath;

	double alpha = 0;
	if(GetProfileDouble(fp, "ctr_model", "alpha", &alpha) < 0)
		return false;
	this->alpha = alpha;

	double max_price = 5000;
	if(GetProfileDouble(fp, "ctr_model", "max_price", &max_price) < 0)
		return false;
	this->max_price = max_price;
	
	return true;
}

double LRModel::normalizedFeature(double val, double max_val = 2209, bool is_log = true)
{
	if(val >= max_val)
	{
		return 1;
	}
	if(val <= 0)
	{
		return 0;
	}

	if(is_log)
	{
		val = log(val + 1);
		max_val = log(max_val + 1);
	}
	
	return val/max_val;
}	

bool LRModel::loadModel()
{
	ifstream fin(this->model_file.c_str());
	if(!fin)
	{
		return false;
	}
	
	string line;
	while(getline(fin, line)){
		double w = atof(line.c_str());
		this->model.push_back(w);
	}

	if(this->model.size() != (this->n_feature + 1))
	{
		return false;
	}

	return true;
}


double LRModel::estimateCTR(Sample sample)
{
	double wx = this->model[0];
	int size = sample.feature_vec.size();
	for(int i = 0; i < size; i ++){
		int index = sample.feature_vec[i].index;
		wx += this->model[index]*sample.feature_vec[i].value;
	}

	double ctr = 1.0/(1 + exp(0 -wx));
	return ctr;
}

Sample LRModel::featuresProcess(int pid, double feature[])
{
	Sample sample;
	sample.id = pid;

	//is_new
	Feature is_new_node;
	is_new_node.index = 1;
	is_new_node.value = feature[0];
	sample.feature_vec.push_back(is_new_node);
	//is_promo
	Feature is_promo_node;
	is_promo_node.index = 2;
	is_promo_node.value = feature[1];
	sample.feature_vec.push_back(is_promo_node);
	//score
	Feature score_node;
	score_node.index = 3;
	score_node.value = 0;
	if(feature[2] >= 8)
		score_node.value = 1;
	sample.feature_vec.push_back(score_node);
	//d_sale
	Feature d_sale_node;
	d_sale_node.index = 4;
	d_sale_node.value = normalizedFeature(feature[3], 50, false);
	sample.feature_vec.push_back(d_sale_node);
	//w_sale
	Feature w_sale_node;
	w_sale_node.index = 5;
	w_sale_node.value = normalizedFeature(feature[4]);
	sample.feature_vec.push_back(w_sale_node);
	//m_sale
	Feature m_sale_node;
	m_sale_node.index = 6;
	m_sale_node.value = normalizedFeature(feature[5]);
	sample.feature_vec.push_back(m_sale_node);
	//n_keep
	Feature keep_node;
	keep_node.index = 7;
	keep_node.value = normalizedFeature(feature[6]);
	sample.feature_vec.push_back(keep_node);
	//n_comm
	Feature comm_node;
	comm_node.index = 8;
	comm_node.value = normalizedFeature(feature[7]);
	sample.feature_vec.push_back(comm_node);

	return sample;
}


//int main()
//{
//	LRModel model;
//	if(!model.init("main.cfg"))
//	{
//		cout<<"error: init"<<endl;
//		return -1;
//	}
//	cout<<model.getN_feature()<<endl;
//
//	if(!model.loadModel())
//	{
//		cout<<"error: loadModel"<<endl;
//		return -2;
//	}
//
//	vector<double> sam = model.getModel();
//	for(int i = 0; i < model.getN_feature() + 1; i ++){
//		cout<<sam[i]<<endl;
//	}
//
//	double feature[8] = {1,1,1,1,1,1,1,1};
//	Sample sample = model.featuresProcess(911, feature);
//	double ctr = model.estimateCTR(sample);
//	cout<<"ctr = "<<ctr<<endl;
//	return 0;
//}



















