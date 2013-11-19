#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include "SearchKeyRanking.h"
#include "effective_wtree.h"
#include "Class.h"
#include "BitMap.h"
#include "GlobalDef.h"
#include "../../segment_new/gbk_encode_convert.h"
#include "SearchGlobal.h"
#include "TimeUtil.h"
#include "delete_elem_from_vect.h"


CSearchKeyRanking::~CSearchKeyRanking()
{
	//ÐèÒªÊÖ¶¯ÊÍ·ÅµÄ¶ÔÏóÇëÊÖ¶¯ÇåÀí
	fprintf(stderr, "CEgKeyRanking  leave\n");
	if(m_ip_location != NULL)
	{
		ddip_free(m_ip_location);
		m_ip_location = NULL;
	}
}

/*
 *³õÊ¼»¯º¯ÊýÐèÒª¾ßÌåÄ£¿éÖØÔØ
 *Parameter psdi ËÑË÷Êý¾Ý´«µÝ
 *Parameter strConf Ä£¿é¸÷×ÔµÄÅäÖÃÎÄ¼þµØÖ·
 *return true³õÊ¼»¯³É¹¦£¬false³õÊ¼»¯Ê§°Ü 
 */

static inline bool filt_zero_score(const SResult& rs)
{
	return rs.nScore <= 0;
}

static inline void split(vector<string>& vec, string& str, string seperator)
{
	vec.clear();
	if(str.empty())
		return;
	int i = 0, pos = 0;
	string tmp;
	while((pos = str.find_first_of(seperator, i)) != string::npos)
	{
		tmp = str.substr(i, pos-i);
		vec.push_back(tmp);
		i = pos + 1;
	}
	tmp = str.substr(i);
	vec.push_back(tmp);
}

void CSearchKeyRanking::merge_field(vector<int>& termId, vector<vector<int> >& termCharaToField, vector<int>& fieldVec)
{
	fieldVec.clear();
	int j, fid, count = 0;
	vector<int> pos(termId.size(), 0);
	vector<bool> isEnd(termId.size(), false);
	while(true)
	{
		for(int i = 0; i < termId.size(); i++)
		{
			if(!isEnd[i])
			{
				//×÷Õß´Ê Ê×ÏÈÐèÒªÔÚAUTHOR,SINGER,DIRECTOR,ACTORËÄ¸ö×Ö¶Î½øÐÐ²éÕÒ
				if(termId[i] == AUTNAME && pos[i] == 0)
				{
					for(int time = 0; time < 4; ++time)
					{
						fid = termCharaToField[termId[i]][pos[i]];
						for(j = 0; j < fieldVec.size(); j++)
						{
							if(fieldVec[j] == fid)
								break;
						}
						if(j == fieldVec.size())
							fieldVec.push_back(fid);
						++pos[i];
					}
					if(pos[i] == termCharaToField[termId[i]].size()-1)
					{
						++count;
						isEnd[i] = true;
					}
				}
				else
				{
					fid = termCharaToField[termId[i]][pos[i]];
					for(j = 0; j < fieldVec.size(); j++)
					{
						if(fieldVec[j] == fid)
							break;
					}
					if(j == fieldVec.size())
						fieldVec.push_back(fid);
					if(pos[i] == termCharaToField[termId[i]].size()-1)
					{
						++count;
						isEnd[i] = true;
					}
					else
						++pos[i];
				}
			}
		}
		if(termId.size() == count)
			break;
	}
}

const FieldToId CSearchKeyRanking::field_id[] = {
	{"subname", 1},
	{"title_sub", 2},
	{"cat_paths_name", 3},
	{"title_synonym", 6},
	{"actors", 5},
	{"director", 5},
	{"singer", 5},
	{"author_name", 5},
	{"product_name", 6},
	{"publisher", 7},
	{"series_name", 8},
	{"title_primary", 9},
	{"isbn_search", 10},
	{"brand_name", 11}
};

const int CSearchKeyRanking::TermCharaToFieldNum[] = {6,3,6,5,4,2,1};
const char* CSearchKeyRanking::TermCharaToName[] = {
	"BKNAME",
	"BRDNAME",
	"AUTNAME",
	"PDTWORD",
	"CATNAME",
	"MODELCODE",
	"PUBLISHER"
};

const int CSearchKeyRanking::ScoreBitMap[] = {
	0x0,
	0x1,
	0x3,
	0x7,
	0xf,
	0x1f,
	0x3f,
	0x7f,
	0xff
};

const char* CSearchKeyRanking::field[] = {
	"product_id",
	"is_share_product",
	"sale_day",
	"sale_week",
	"sale_month",
	//"sale_day_amt",
	"sale_week_amt",
	//"sale_month_amt",
	"modifytime",
	"first_input_date",
	"dd_sale_price",
	"pre_sale",
	"num_images",
	"publish_date",
	"total_review_count",
	"city_stock_status",
	"stock_status",
	"product_medium",
	"promo_saleprice",
	"promo_start_date",
	"promo_end_date",
	"score"
};

static inline int GetMaxValFromVect(vector<SResult>& vect)
{
	int max = 0;
	for(size_t i = 0; i < vect.size(); i++)
	{
		if(vect[i].nDocId > max)
		{
			max = vect[i].nDocId;
		}
	}
	return max;
}

bool CSearchKeyRanking::Init(SSearchDataInfo* psdi, const string& strConf)
{
	CModule::Init(psdi, strConf);//always called 

	if(!InitCommon(psdi, strConf) || !LoadFile())
	{
		COMMON_LOG(SL_ERROR, "CSearchKeyRanking init failed");
		return false;
	}
	//whj
	if(!InitPersonalityLocationStockDict())
	{
#ifdef DEBUG
		cout << "init ip locat dict error!" << endl;
#endif
		COMMON_LOG(SL_ERROR, "load Ip 2 Loaction file failed");
	}
	if(!InitPersonalitySexDict())
	{
#ifdef DEBUG
		cout << "init sex dict error!" << endl;
#endif
		COMMON_LOG(SL_ERROR, "load Personality Sex file failed");
	}
	COMMON_LOG(SL_NOTICE, "CSearchKeyRanking init ok");
	return true;
}

bool CSearchKeyRanking::InitData()
{
	for(int i = 0; i < 10000; i++)
	{
		m_salAmtScr.push_back((5 + (int)floor(1/(1 + exp((-1) * log10(0.1 * i) + 2)) * 120)) / 10);
	}
	for(int i = 0; i < 1000; i++)
	{
		m_salNumScr.push_back((5 + (int)floor(1/(1 + exp((-1) * log10(0.5 * i) + 1)) * 100)) / 10);
	}
	for(int i = 0; i < 100; i++)
	{
		m_commentScr.push_back((5 + (int)floor(1/(1 + exp((-1) * log10(0.5 * i) + 1)) * 100)) / 10);
	}
	return true;
}


/////////////////////////////
//caiting add 2013-10-25
////////////////////////////
/*
int isNewProduct(int nDocId)
{
	time_t now_time = time(NULL);
	int input_time =  m_funcFrstInt(m_inputDateProfile, nDocId);
	int interval = (now_time - input_time)/86400;
	if(interval >= 15)
		return 0;
	return 1;
}

int isPromoProduct(int nDocId)
{
	time_t now_time = time(NULL);
	int start_time = m_funcFrstInt(m_promoStartDateProfile, nDocId);
	int end_time = m_funcFrstInt(m_promoEndDateProfile, nDocId);
	if((start_time < now_time) && (now_time < end_time))
		return 1;
	return 0;
}
*/

double CSearchKeyRanking::ComputerLRCtrOnline(int nDocId)
{
	double feature[8] = {0};
	
	time_t now_time = time(NULL);
	
	int is_new = 1;
	int input_time =  m_funcFrstInt(m_inputDateProfile, nDocId);
	int interval = (now_time - input_time)/86400;
	if(interval >= 15)
		is_new = 0;
	feature[0] = is_new;

	int is_promo = 0;
	int start_time = m_funcFrstInt(m_promoStartDateProfile, nDocId);
	int end_time = m_funcFrstInt(m_promoEndDateProfile, nDocId);
	if((start_time < now_time) && (now_time < end_time))
		is_promo = 1;
	feature[1] = is_promo; 

	double score = m_funcFrstInt(m_scoreProfile, nDocId); 
	feature[2] = score;

	int d_sale = m_funcFrstInt(m_saleDayProfile, nDocId); 
	feature[3] = d_sale;

	int w_sale = m_funcFrstInt(m_saleWeekProfile, nDocId);
	feature[4] = w_sale;

	int m_sale = m_funcFrstInt(m_saleMonthProfile, nDocId);
	feature[5] = m_sale;

	int n_keep = m_funcFrstInt(m_totalReviewCountProfile, nDocId);
	feature[6] = n_keep;

	int n_comm = n_keep;
	feature[7] = n_comm;

	Sample sample = this->lr_model.featuresProcess(nDocId, feature);
	double ctr = this->lr_model.estimateCTR(sample);
	
	return ctr;
}

double CSearchKeyRanking::ComputeLRCTR(int nDocId)
{
	int pid = m_funcFrstInt64(m_isPidProfile, nDocId);
	
	double price = m_funcFrstInt(m_salePriceProfile, nDocId)/100.0;
	if(price > this->lr_model.getMax_price()) /// @todo should be configured
	{
		price = this->lr_model.getMax_price();
	}
	if( price < 0 ) price = 0;
	price = log(price + 1);
	
	double ctr = m_pid2value[pid];
	if(ctr == 0)
	{
	//	ctr = m_pid2value[-1];
		ctr = ComputerLRCtrOnline(nDocId);
	}

	//return ctr;
	double alpha = this->lr_model.getAlpha();	/// @todo should be configured
	return pow(price, alpha)*pow(ctr, 1-alpha);
}


bool CSearchKeyRanking::ComputerLRCTR4DocIdList(vector<SResult>&  vRes)
{
	int n_size = vRes.size();
	vector<double> lrctr_vec;	
	double max_ctr = 0;
	double min_ctr = 100000;
	for(int i = 0; i < n_size; i ++){
		int nDocId = vRes[i].nDocId;
		double ctr = ComputeLRCTR(nDocId);
		lrctr_vec.push_back(ctr);

		max_ctr = ctr > max_ctr ? ctr : max_ctr;
		min_ctr = ctr < min_ctr ? ctr : min_ctr;
		
	}
	
	if(max_ctr != min_ctr)
	{
		//normalize lr_ctr to 0~127
		double score = 127/(max_ctr - min_ctr);
		for(int j = 0; j < n_size; j ++){
			double lr_ctr = lrctr_vec[j];
			double tmp = (lr_ctr - min_ctr)*score;
			vRes[j].nScore += int(tmp);
		}
	}

	return true;	
}

bool CSearchKeyRanking::LoadFile()
{
	//¼ÓÔØ·´À¡Êý¾Ý
	m_key2Cate.read(m_strModulePath + "query_cate_direction");
	m_key2Pid.read(m_strModulePath + "query_pid_direction");
	m_pid2Core.read(m_strModulePath + "pid_core_word");
	m_key_pid_weight_hash.read(m_strModulePath + "ctr_data");

	////////////////////////////////////////////////////////////////	
	// caiting add 2013-10-22 for loading the static parts of wx
	COMMON_LOG(SL_INFO, "loading pid2rankvalue ... ... ... ... ");
	string pid2value_path = m_strModulePath + "pid2rankvalue";
	int ret = m_pid2value.load_serialized_hash_file(pid2value_path.c_str(), 0);
	/// @todo check if load file failed
	string lr_model_config = m_strModulePath + "lr_model_conf.cfg";
	if(!this->lr_model.init(lr_model_config))
	{
		COMMON_LOG(SL_ERROR, "can't find file: %s", lr_model_config.c_str());
		cout<<" can't find file: "<<lr_model_config<<endl;
		return false;
	}
	
	string model_file_name = this->lr_model.getModel_file();
	this->lr_model.setModel_file(m_strModulePath + model_file_name);
	cout<<"model_file = "<<this->lr_model.getModel_file()<<endl;	
	if(!this->lr_model.loadModel())
	{
		
		COMMON_LOG(SL_ERROR, "loading lr model failed !");
		cout<<"loading lr model failed!"<<endl;
		return false;
	}
	cout<<"i am here~~~"<<endl;
	///////////////////////////////////////////////////////////////

	printf("init: load feedback data\n");

	//¼ÓÔØ´Êµä
	string files = m_strModulePath + "loadfilename.txt";
	ifstream ifs(files.c_str());
	if(!ifs)
	{
		COMMON_LOG(SL_ERROR, "can't find file: %s", files.c_str());
		return false;
	}
	string line;
	vector<string> filenames;
	while(getline(ifs, line))
	{
		boost::trim(line);
		filenames.push_back(line);
	}
	ifs.close();
	ifs.clear();
	for(int i = 0; i < filenames.size(); i++)
	{
		string path = m_strModulePath + filenames[i];
		m_dict[i].load_serialized_hash_file(path.c_str(), -1);
	}

	//¼ÓÔØ¼Û¸ñ´òÉ¢Êý¾Ý
	string fname = m_strModulePath + "price_scatter.txt";
	ifs.open(fname.c_str());
	if(!ifs)
	{
		COMMON_LOG(SL_ERROR, "can't find file: %s", fname.c_str());
	}
	vector<string> vec, tmp;
	while(getline(ifs, line))
	{
		boost::trim(line);
		split(vec, line, " ");
		if(vec.size() < 2)
		{
			COMMON_LOG(SL_WARN, "can't find file: %s", fname.c_str());
			continue;
		}
		u64 cid = TranseClsPath2ID(vec[0].c_str(), vec[0].size());
		for(int i = 1; i < vec.size(); ++i)
		{
			split(tmp, vec[i], "|");
			if(tmp.size() != 2)
			{
				COMMON_LOG(SL_WARN, "can't find file: %s", fname.c_str());
				continue;
			}
			int p = atoi(tmp[0].c_str());
			int r = atoi(tmp[1].c_str());
			m_price_info_map[cid].push_back(make_pair(p, r));
		}
	}
	ifs.close();
	ifs.clear();

	//¼ÓÔØ»ìÅÅÊý¾Ý
	string key_cate_rate_path(m_strModulePath + "key_cate_rate");
	if(false == m_percentPub2Mall.read(key_cate_rate_path.c_str()))
	{
		printf("failed to load key_cate_rate\n");
		return false;
	}
	printf("init merge data: load key_cate_rate\n");
	return true;
}

bool CSearchKeyRanking::InitCommon(SSearchDataInfo* psdi, const string& strConf) 
{
	int fieldInfo[] = {
	  TITLEPRI, TITLENAME, SERIES, TITLESYN, PUBNAME, SUBNAME,
	  BRAND, TITLENAME, TITLESYN,
	  AUTHOR, SINGER, DIRECTOR, ACTOR, TITLEPRI, TITLENAME,
	  TITLEPRI, TITLENAME, TITLESYN, BOTTOMCATE, SUBNAME,
	  TITLEPRI, TITLENAME, BOTTOMCATE, TITLESYN,
	  TITLEPRI, TITLENAME,
	  PUBNAME
	};
	int* start = fieldInfo;
	TermCharaToField.resize(sizeof(TermCharaToFieldNum)/sizeof(int));
	for(int i = 0; i < sizeof(TermCharaToFieldNum)/sizeof(int); i++)
	{
		TermCharaToField[i].resize(TermCharaToFieldNum[i]);
		memcpy(&TermCharaToField[i][0], start, TermCharaToField[i].size()*sizeof(int));
		start += TermCharaToField[i].size();
	}
	int pubField[] = {AUTHOR, SINGER, DIRECTOR, ACTOR, TITLEPRI, TITLENAME,
		TITLESYN, PUBNAME, BOTTOMCATE, SERIES, ISBN, SUBNAME, TITLESUB};
	int mallField[] = {BRAND, TITLENAME, TITLESYN, BOTTOMCATE, SUBNAME};
	pAllField.insert(pubField, pubField + sizeof(pubField)/sizeof(int));
	mAllField.insert(mallField, mallField + sizeof(mallField)/sizeof(int));

	m_vProfile.push_back(make_pair("product_sexual", &m_product_sex_profile));
	m_vProfile.push_back(make_pair("city_stock_status", &m_stockStatusProfile));
	m_vProfile.push_back(make_pair("stock_status", &m_stockProfile));
	m_vProfile.push_back(make_pair("cat_paths", &m_clsProfile));
	m_vProfile.push_back(make_pair("sale_day", &m_saleDayProfile));
	m_vProfile.push_back(make_pair("sale_week", &m_saleWeekProfile));
	m_vProfile.push_back(make_pair("sale_month", &m_saleMonthProfile));
	//m_vProfile.push_back(make_pair("sale_day_amt", &m_saleDayAmtProfile));
	m_vProfile.push_back(make_pair("sale_week_amt", &m_saleWeekAmtProfile));
	//m_vProfile.push_back(make_pair("sale_month_amt", &m_saleMonthAmtProfile));
	m_vProfile.push_back(make_pair("first_input_date", &m_inputDateProfile));
	m_vProfile.push_back(make_pair("modifytime", &m_modifyTime));
	m_vProfile.push_back(make_pair("is_share_product", &m_isShareProductProfile));
	m_vProfile.push_back(make_pair("product_id", &m_isPidProfile));
	m_vProfile.push_back(make_pair("dd_sale_price", &m_salePriceProfile));
	m_vProfile.push_back(make_pair("pre_sale", &m_preSaleProfile));
	m_vProfile.push_back(make_pair("num_images", &m_numImagesProfile));
	m_vProfile.push_back(make_pair("publish_date", &m_pubDateProfile));
	m_vProfile.push_back(make_pair("total_review_count", &m_totalReviewCountProfile));
	m_vProfile.push_back(make_pair("is_publication", &m_isPublicationProfile));
	m_vProfile.push_back(make_pair("brand_id", &m_isBrdidProfile));//new add
	m_vProfile.push_back(make_pair("product_medium", &m_proMedProfile));//new add

	m_vProfile.push_back(make_pair("promo_saleprice", &m_promoPriceProfile));
	m_vProfile.push_back(make_pair("promo_start_date", &m_promoStartDateProfile));
	m_vProfile.push_back(make_pair("promo_end_date", &m_promoEndDateProfile));
	m_vProfile.push_back(make_pair("score", &m_scoreProfile));

	for(vector<pair<string, void**> >::iterator it = m_vProfile.begin(); it != m_vProfile.end(); it++)
	{
		string keyStr = it->first;
		*it->second = FindProfileByName(keyStr.c_str());
		if(*it->second == NULL)
		{
			COMMON_LOG(SL_ERROR, "%s profile does not exist", keyStr.c_str());
			return false;
		}
	}

	m_vFieldIndex.resize(sizeof(field_id) / sizeof(FieldToId));
	m_fid2fi.resize(m_vFieldInfo.size());

	int index;
	for(int i = 0; i < m_vFieldIndex.size(); i++)
	{
		index = GetFieldId(field_id[i].field);
		m_vFieldIndex[i] = index;
		if(index == -1)
		{
			COMMON_LOG(SL_ERROR, "%s field does not exist", field_id[i].field);
			return false;
		} 
		else 
		{
			m_fid2fi[index] = field_id[i].id;
		}
	}

	for(int i = 0; i < sizeof(field)/sizeof(char*); i++)
	{
		index = GetFieldId(field[i]);
		if (index == -1)
		{
			COMMON_LOG(SL_ERROR, "%s field does not exist", field[i]);
			return false;
		}
	}

	//commerce use
	void* m_commScoreLow = FindProfileByName("comm_score_low");
	if(!m_custom_weighter.Init(psdi, "search_ranking", m_strModulePath) || (m_commScoreLow == NULL))
	{
		COMMON_LOG(SL_ERROR, "Fail to init custom weighter");
		ifHavComMod = false;
		InitData();
	}
	else
	{
		ifHavComMod = true;
	}
	return true;
}

IAnalysisData* CSearchKeyRanking::QueryAnalyse(SQueryClause& qc)
{
	//ofstream ofs("queChara.txt",ios::app);
	//ofstream ofs1("queOther.txt",ios::app);
	CDDAnalysisData* pa = new CDDAnalysisData;
	pa->m_hmUrlPara = qc.hmUrlPara;			//±£´æÈ«²¿URL²ÎÊý 
	pa->m_otherSortField = qc.firstSortId;	//È¡ÆäËûÅÅÐò×Ö¶Î

	pa->bAdvance = qc.bAdvance;
	pa->m_vTerms.resize(qc.vTerms.size());
	pa->m_vTerms.assign(qc.vTerms.begin(), qc.vTerms.end());
	if(qc.cat == 0)
	{
		pa->m_searchType = CDDAnalysisData::FULL_SITE_SEARCH;
	}
	else if(!isMall(qc.cat))
	{
		pa->m_searchType = CDDAnalysisData::PUB_SEARCH;
	}
	else
	{
		pa->m_searchType = CDDAnalysisData::MALL_SEARCH;
	}
	
	vector<QueryItem> queryItems;
	QueryParse(qc.key, qc.vTerms, queryItems);
	//commerce use
	/*pa->m_AnalysisPart.cloth_cat_id = m_custom_weighter.GetWeightCat(TranseClsPath2ID("58.64", 5));
	pa->m_AnalysisPart.c3_cat_id = m_custom_weighter.GetWeightCat(TranseClsPath2ID("58.80", 5));
	pa->m_AnalysisPart.other_cat_id = m_custom_weighter.GetWeightCat(TranseClsPath2ID("58", 2));
	pa->m_AnalysisPart.pub_cat_id = m_custom_weighter.GetWeightCat(TranseClsPath2ID("01", 2));
	*/
	size_t queryKeyCnt = queryItems.size();
	pa->m_AnalysisPart.type.resize(queryKeyCnt, -1);
	pa->m_AnalysisPart.dis.resize(queryKeyCnt, 0);
	pa->m_AnalysisPart.vCluster.resize(4);
	pa->m_AnalysisPart.queryStr = qc.key;

	pa->m_AnalysisPart.ifBrdQuery = false;
	pa->m_AnalysisPart.ifPdtQuery = false;
	pa->m_AnalysisPart.ifAutPubSearch = -1;
	pa->m_AnalysisPart.needJudgeDis = true;
	pa->m_AnalysisPart.tDocCnt = 0;
	pa->m_AnalysisPart.tDocCnt_stock = 0;
	for(int i = 0; i < queryKeyCnt; ++i)
	{
		if(false == queryItems[i].field.empty())
		{
			pa->m_AnalysisPart.ifLimitSearch = true;
		}
		pa->m_AnalysisPart.dis[i] = queryItems[i].fwdDis;
	}
	
	const int tmSize = sizeof(TermCharaToName)/sizeof(char*); 
	int comCnt = 0, termChara = 0, tmCharaCnt[tmSize] = {0};
	int autOrd = -1, brdOrd = -1, comBKAut = -1;
	bool needDis = false;
	JudgeTermCharacter(pa, pa->m_AnalysisPart.queryStr, termChara);
	if(termChara != 0)
	{
		needDis = true;
		pa->m_AnalysisPart.termChara = termChara;
		pa->m_AnalysisPart.type.clear();
		pa->m_AnalysisPart.type.resize(queryKeyCnt, termChara);
		if((termChara == (1<<BKNAME) && qc.key.size() >= 10) || (termChara == (1<<AUTNAME)))
		{
			pa->m_AnalysisPart.sugFBCate = 0;
		}
		else if(termChara == (1<<PDTWORD))
		{
			pa->m_AnalysisPart.sugFBCate = 1;
		}
		else if((termChara == (1<<MODELCODE)) && qc.key.size() >= 3)
		{
			pa->m_AnalysisPart.sugFBCate = 2;
		}

		/*if(((termChara>>AUTNAME)&ScoreBitMap[1])&&((termChara>>BRDNAME)&ScoreBitMap[1]))
			ofs<<"autname brdname: "<<pa->m_AnalysisPart.queryStr<<endl;
		else if(((termChara>>BKNAME)&ScoreBitMap[1])&&((termChara>>BRDNAME)&ScoreBitMap[1]))
			ofs<<"bkname brdname: "<<pa->m_AnalysisPart.queryStr<<endl;
		else if(termChara == (1<<BKNAME))
			ofs<<"bkname: "<<pa->m_AnalysisPart.queryStr<<endl;
		else if(termChara == (1<<BRDNAME))
			ofs<<"brdname: "<<pa->m_AnalysisPart.queryStr<<endl;
		else if(termChara == (1<<AUTNAME))
			ofs<<"autname: "<<pa->m_AnalysisPart.queryStr<<endl;
		else if(termChara == (1<<PDTWORD))
			ofs<<"pdtword: "<<pa->m_AnalysisPart.queryStr<<endl;
		else if(termChara == (1<<CATNAME))
			ofs<<"catname: "<<pa->m_AnalysisPart.queryStr<<endl;
		else if(termChara == (1<<MODELCODE))
			ofs<<"modelcode: "<<pa->m_AnalysisPart.queryStr<<endl;
		else if(termChara == (1<<PUBLISHER))
			ofs<<"publisher: "<<pa->m_AnalysisPart.queryStr<<endl;
		else
		{
			for(int i=0;i<7;i++)
				if((termChara>>i)&ScoreBitMap[1])
					ofs1<<TermCharaToName[i]<<" and ";
			ofs1<<" key: "<<pa->m_AnalysisPart.queryStr<<endl;
		}*/
	}
	else if(queryKeyCnt > 1)
	{
		bool ifContinue = true;
		int enStaPos = -1, enEndPos;
		for(int i = 0; i < queryKeyCnt; i++)
		{
			string key = queryItems[i].word;
			if(key[0] > 0 && ifContinue)
			{
				if(enStaPos == -1)
				{
					enStaPos = i;
				}
				enEndPos = i;
				continue;
			}
			else if(enStaPos != -1)
			{
				ifContinue = false;
			}

			JudgeTermCharacter(pa, key, termChara);
			pa->m_AnalysisPart.type[i] = termChara;
			for(int tm = 0; tm < TermCharaToField.size(); tm++)
			{
				if(0 == termChara)
				{
					++comCnt;
					break;
				}
				else if((termChara >> tm) & ScoreBitMap[1])
				{
					++tmCharaCnt[tm];
					if(tm == AUTNAME)
					{
						autOrd = i;
					}
					else if(tm == BRDNAME)
					{
						brdOrd = i;
					}
				}
			}
			if(((termChara >> BKNAME) & ScoreBitMap[1]) || ((termChara >> AUTNAME) & ScoreBitMap[1]))
			{
				++comBKAut;
			}

			if(i > 0 && 0 == queryItems[i].fwdDis && 0 == pa->m_AnalysisPart.type[i-1] && 0 == pa->m_AnalysisPart.type[i])
			{
				needDis = true;
			}
		}
		if(enStaPos != -1)
		{
			int sta = qc.vTerms[enStaPos].pos;
			int len = qc.vTerms[enEndPos].pos + qc.vTerms[enEndPos].len - sta;
			string str = qc.key.substr(sta, len);
			boost::trim(str);
			JudgeTermCharacter(pa, str, termChara);
			for(int i = enStaPos; i <= enEndPos; i++)
			{
				pa->m_AnalysisPart.type[i] = termChara;
			}
			for(int tm = 0; tm < TermCharaToField.size(); tm++)
			{
				if(termChara == 0)
				{
					comCnt += enEndPos - enStaPos + 1;
					break;
				}
				else if((termChara >> tm) & ScoreBitMap[1])
				{
					tmCharaCnt[tm] += enEndPos - enStaPos + 1;
					if(tm == BRDNAME)
					{
						if(enStaPos == 0)
						{
							brdOrd = 0;
						}
					}
				}
			}
			if(((termChara >> BKNAME) & ScoreBitMap[1]) || ((termChara >> AUTNAME) & ScoreBitMap[1]))
			{
				++comBKAut;
			}
			needDis = true;
		}

		if(autOrd != -1 && comCnt != 0)
		{
			string str;
			if(autOrd == 0)
			{
				int sta = qc.vTerms[0].pos + qc.vTerms[0].len;
				str = qc.key.substr(sta);
			}
			else if(autOrd > 0 && autOrd == queryKeyCnt - 1)
			{
				str = qc.key.substr(0, qc.vTerms[autOrd].pos);
			}
			boost::trim(str);
			if(m_dict[BKNAME][str] != -1)
			{
				if(autOrd == 0)
				{
					for(int i = 1; i < queryKeyCnt; i++)
					{
						pa->m_AnalysisPart.type[i] = (1<<BKNAME);
					}
					pa->m_AnalysisPart.type[0] = (1<<AUTNAME);
				}
				else
				{
					for(int i = 0; i < queryKeyCnt-1; i++)
					{
						pa->m_AnalysisPart.type[i] = (1<<BKNAME);
					}
					pa->m_AnalysisPart.type[queryKeyCnt-1] = (1<<AUTNAME);
				}
				tmCharaCnt[BKNAME] = queryKeyCnt - 1;
				tmCharaCnt[AUTNAME] = 1;
				comBKAut = 1;
				comCnt = 0;
			}
		}
		if(brdOrd == 0 && comCnt != 0)
		{
			int sta = qc.vTerms[0].pos + qc.vTerms[0].len;
			string str = qc.key.substr(sta);
			boost::trim(str);
			if(m_dict[PDTWORD][str] != -1)
			{
				for(int i = 1; i < queryKeyCnt; i++)
				{
					pa->m_AnalysisPart.type[i] = (1<<PDTWORD);
				}
				pa->m_AnalysisPart.type[0] = (1<<BRDNAME);
				tmCharaCnt[PDTWORD] = queryKeyCnt - 1;
				tmCharaCnt[BRDNAME] = 1;
				comCnt = 0;
				pa->m_AnalysisPart.pdtWord = str;
			}
		}
	}
	if(false == needDis || qc.key.size() > 24)
	{
		pa->m_AnalysisPart.needJudgeDis = false;
	}


	bool isAllPdt = ((pa->m_AnalysisPart.termChara >> PDTWORD) & ScoreBitMap[1]);
	bool isAllBrd = ((pa->m_AnalysisPart.termChara >> BRDNAME) & ScoreBitMap[1]);
	if(isAllPdt || (tmCharaCnt[PDTWORD] != 0 && tmCharaCnt[BRDNAME] != 0 && 
		comCnt == 0 && tmCharaCnt[PDTWORD] + tmCharaCnt[BRDNAME] == queryKeyCnt))
	{
		pa->m_AnalysisPart.ifPdtQuery = true;
	}
	else if(isAllBrd || (tmCharaCnt[BRDNAME] == queryKeyCnt))
	{
		pa->m_AnalysisPart.ifBrdQuery = true;
	}

	if(pa->m_AnalysisPart.termChara != 0)
	{
		for(int i = 0; i < TermCharaToField.size(); i++)
		{
			if((pa->m_AnalysisPart.termChara >> i)&ScoreBitMap[1])
			{
				pa->m_AnalysisPart.termId.push_back(i);
			}
		}
	}
	else if(queryKeyCnt > 1)
	{
		if(tmCharaCnt[BRDNAME] != 0 && tmCharaCnt[MODELCODE] != 0 && comCnt == 0 && 
				(tmCharaCnt[BRDNAME] + tmCharaCnt[MODELCODE] == queryKeyCnt))
		{
			pa->m_AnalysisPart.termId.push_back(BRDNAME);
			pa->m_AnalysisPart.termId.push_back(MODELCODE);
			pa->m_AnalysisPart.sugFBCate = 3;
			//ofs<<"brdname and modelcode: "<<pa->m_AnalysisPart.queryStr<<endl;
		}
		else if(pa->m_AnalysisPart.ifPdtQuery)
		{
			pa->m_AnalysisPart.termId.push_back(BRDNAME);
			pa->m_AnalysisPart.termId.push_back(PDTWORD);
			pa->m_AnalysisPart.sugFBCate = 1;
			//ofs<<"brdname and pdtword: "<<pa->m_AnalysisPart.queryStr<<endl;
		}
		else if(tmCharaCnt[MODELCODE] != 0 && tmCharaCnt[PDTWORD] != 0 && comCnt == 0 &&
				(tmCharaCnt[MODELCODE] + tmCharaCnt[PDTWORD] >= queryKeyCnt))
		{
			pa->m_AnalysisPart.termId.push_back(PDTWORD);
			pa->m_AnalysisPart.termId.push_back(MODELCODE);
			pa->m_AnalysisPart.sugFBCate = 3;
			//ofs<<"modelcode and pdtword: "<<pa->m_AnalysisPart.queryStr<<endl;
		}
		else if(tmCharaCnt[BKNAME] != 0 && tmCharaCnt[AUTNAME] != 0 && comCnt == 0 && comBKAut == 1 &&
				(tmCharaCnt[BKNAME] + tmCharaCnt[AUTNAME] >= queryKeyCnt))
		{
			pa->m_AnalysisPart.termId.push_back(BKNAME);
			pa->m_AnalysisPart.termId.push_back(AUTNAME);
			pa->m_AnalysisPart.sugFBCate = 0;
			//ofs<<"bkname and autname: "<<pa->m_AnalysisPart.queryStr<<endl;
		}
		else if(tmCharaCnt[BRDNAME] != 0 && (tmCharaCnt[PDTWORD] == 1) && tmCharaCnt[MODELCODE] != 0 && comCnt == 0
				&& (tmCharaCnt[BRDNAME] + 1 + tmCharaCnt[MODELCODE] >= queryKeyCnt))
		{
			pa->m_AnalysisPart.termId.push_back(BRDNAME);
			pa->m_AnalysisPart.termId.push_back(PDTWORD);
			pa->m_AnalysisPart.termId.push_back(MODELCODE);
			pa->m_AnalysisPart.sugFBCate = 3;
			//ofs<<"brdname and pdtword and modelcode: "<<pa->m_AnalysisPart.queryStr<<endl;
		}
	}

	string queryStr = pa->m_AnalysisPart.queryStr;
	int kcate_count = m_key2Cate[queryStr].count;
	if(kcate_count != 0)
	{
		//Àà±ð·´À¡
		pa->m_AnalysisPart.ifHasFeedback = true;
		HASHVECTOR kcate_vec = m_key2Cate[queryStr];
		u64 cate_id = 0;
		int weight = 0;
		for(int k = 0; k < kcate_count; k++) 
		{
			memcpy(&cate_id, kcate_vec.data+k*kcate_vec.size, sizeof(u64));
			memcpy(&weight, kcate_vec.data+k*kcate_vec.size+sizeof(u64), sizeof(int));
			pa->m_AnalysisPart.vCate.push_back(pair<u64, int>(cate_id, weight));
			if(!isMall(cate_id))
			{
				pa->m_AnalysisPart.ifAllMallFB = false;
			}
		}
	}

	vector<int> fieldVec;
	if(pa->m_AnalysisPart.termId.empty())
	{
		for(int i = m_vFieldIndex.size()-1; i >= TMINNUM; i--)
		{
			fieldVec.push_back(i);
		}
	}
	else
	{
		merge_field(pa->m_AnalysisPart.termId, TermCharaToField, fieldVec);
	}
	int fieldId;
	set<int> mFidSet, pFidSet;
	for(int i = 0; i < fieldVec.size(); i++)
	{
		pa->m_queryFieldIds.push_back(m_vFieldIndex[fieldVec[i]]);
		fieldId = field_id[fieldVec[i]].id;
		if(pAllField.find(fieldVec[i]) != pAllField.end())
		{
			if(pFidSet.find(fieldId) == pFidSet.end())
			{
				pa->m_AnalysisPart.pFieldId.push_back(fieldId);
				pFidSet.insert(fieldId);
			}
		}
		if(mAllField.find(fieldVec[i]) != mAllField.end())
		{
			if(mFidSet.find(fieldId) == mFidSet.end())
			{
				pa->m_AnalysisPart.mFieldId.push_back(fieldId);
				mFidSet.insert(fieldId);
			}
		}
	}
	//cout<<"ifbrd:"<<pa->m_AnalysisPart.ifBrdQuery<<endl;
	//cout<<"needDis:"<<pa->m_AnalysisPart.needJudgeDis<<endl;
	//cout<<"hasFB:"<<pa->m_AnalysisPart.ifHasFeedback<<endl;
	//cout<<"sug:"<<pa->m_AnalysisPart.sugFBCate<<endl;
	
	//whj
	if(GetUserIp2Location(pa) == false)
	{
#ifdef DEBUG
		printf("ip 2 location error!\n");
#endif
	}
	//¼ÆËãµ±Ç°µÄÖ÷Àà
	if(!ComputeMainCate(pa))
	{
		//cout<<"ComputeMainCate error!"<<endl;
	}
	if(qc.hmUrlPara.find("q") != qc.hmUrlPara.end())
	{
		GetCtrPidHash(pa, qc.hmUrlPara["q"]);
	}
	//ÄÐÅ®¸öÐÔ»¯Êý¾Ý²éÕÒ
	if(GetPersonalityData(pa) == false)
	{
#ifdef DEBUG
		printf("There is no personality !\n");
#endif
	}

	return pa;
}

bool CSearchKeyRanking::ComputeMainCate(CDDAnalysisData* pda)
{
	int main_cate = 0;
	int cate_count = m_key2Cate[pda->m_AnalysisPart.queryStr].count;
	int second_cate = 0;
	if(cate_count > 0)
	{
		//Í³¼Æ×ÜÈ¨ÖØ
		int count = 0;
		int weight  = 0;
		int weight_sum = 0;
		float rate = 0.0;
		u64 cate_id = 0;
		HASHVECTOR cate_vec = m_key2Cate[pda->m_AnalysisPart.queryStr];
		for(int i=0; i < cate_count && count < 2; i++, count++)
		{
			memcpy(&weight, cate_vec.data+i*cate_vec.size+sizeof(u64), sizeof(int));
			weight_sum += weight;
		}
		if(weight_sum<=0) return false;
		count = 0;
		for(int i=0; i < cate_count && count < 2; i++,count++)
		{// »ñÈ¡Àà±ðid
			memcpy(&cate_id, cate_vec.data+i*cate_vec.size, sizeof(u64));
			memcpy(&weight, cate_vec.data+i*cate_vec.size+sizeof(u64), sizeof(int));
			//Ð¡ÓÚãÐÖµ ²»ËãÖ÷Àà
			rate = (float)weight/weight_sum;
			if(rate < MainCateThd) continue;
			//Í¼Êé²»¹ýÂË
			if(PUB_CATE == (int)GetClassByLevel(1,cate_id))
				pda->m_main_cate_set.insert(PUB_CATE);
			else
			{
				second_cate = (int)GetClassByLevel(2,cate_id);
				if(second_cate == PHONE_CATE || second_cate == DIGIT_CATE ||
				   second_cate == COMPUTER_CATE || second_cate == ELECTR_CATE
				   && count == 0)
				{
					main_cate = cate_id;
					pda->m_main_cate_set.insert(main_cate);
					break;
				}
				else
				{
					main_cate = (int)GetClassByLevel(3,cate_id);
					pda->m_main_cate_set.insert(main_cate);
				}
			}
		}
	}
	return true;
}

void CSearchKeyRanking::ComputeWeight(IAnalysisData* pa, SMatchElement& me, SResult& rt)
{
	//Ó°ÏìÐÔÄÜµÄ¹Ø¼üº¯Êý 
	CDDAnalysisData* pda = (CDDAnalysisData*)pa;
	unsigned long long cls = m_funcFrstInt64(m_clsProfile, rt.nDocId);
	if(isMall(cls))
	{
		if (pda->m_searchType != CDDAnalysisData::PUB_SEARCH) //Ö»Òª²»ÊÇ³ö°æÎïËÑË÷
		{
			ComputeWeightMall(pda, me, rt);
			/*
			//¼ÆËãÉÌÒµÒòËØÈ¨ÖØ(ÏúÊÛ¶î¡¢ÊÇ·ñÊÇ¹«ÓÃÆ·¡¢ÏúÊÛÁ¿¡¢ÆÀÂÛÊý)
			int ctr_weight = 0;
			ComputeCtrWeight(pda, me.id, ctr_weight);
			if(ifHavComMod)
			{
				int cat_id = 0;
				int commScr = GetCommerceScore(pda, cat_id, rt.nDocId);
				commScr += ctr_weight;
				MoveLeftBit(rt.nScore, commScr, COMMERCEBIT);
			}
			else
			{
				int salAmtScr, ifPub, salNumScr, commentScr;
				ComputeCommerceWeight(rt.nDocId, salAmtScr, ifPub, salNumScr, commentScr);	
				//ÏúÊÛ¶î¡¢ÏúÁ¿¡¢ÆÀÂÛÊý¡¢ÊÇ·ñ¹«ÓÃÆ·
				MoveLeftBit(rt.nScore, ctr_weight + salAmtScr + salNumScr + commentScr + ifPub, COMMERCEBIT);
			}
			*/
			ChangeWeight(rt, pda, cls);
		}
		MoveLeftBit(rt.nWeight, 1, ISMALLBIT);	//±êÊ¶ÊÇ°Ù»õ
	}
	else if(cls != 0)
	{
		if(pda->m_searchType == CDDAnalysisData::FULL_SITE_SEARCH || pda->m_searchType == CDDAnalysisData::PUB_SEARCH)
		{
			ComputeWeightPub(pda, me, rt);
		}
	}
	else
	{
		ComputeWeightPub(pda, me, rt);
	}
	StatisticLevelInfo(pa, rt);	//Í³¼ÆÆ¥Åäµ½µÄÉÌÆ·µÄË÷Òý´ÊÏîµÈ¼¶ÐÅÏ¢
	
	//whj
	int stock = m_funcFrstInt(m_stockProfile, me.id);	//ÅÐ¶ÏÇøÓòÈ±»õ
	int docID = rt.nDocId;
	int blocation = pda->m_bit_city_location;
	int locationStock = 1;	//ÓÃ»§ËùÔÚÇøÓòÈ±»õ:0ÎªÈ±»õ£¬1Îª²»È±»õ
	//ÇøÓòÎÞ¿â´æºóÅÅ
	if(stock == 1 && pda->m_usr_sort_type == 0)
	{
		rt.nScore += (locationStock << 8);
	}
	else if(stock == 2 && pda->m_usr_sort_type == 0)
	{
		if(JudgeLocationStock(docID, blocation, locationStock))
		{
			rt.nScore += (locationStock << 8);
		}
		else
		{
			rt.nScore += (1 << 8);
		}
	}
}

void CSearchKeyRanking::SortMall(vector<SResult>& vRes, int from, int to, CDDAnalysisData* pa)
{
	SortRange(vRes ,from, to);
#ifdef DEBUG
	int out_size = min((int)vRes.size(), 50);
	for (int i=0; i<out_size; i++)
	{
		int docID = vRes[i].nDocId;
		int date = m_funcFrstInt(m_modifyTime, docID);
		int pid = m_funcFrstInt64(m_isPidProfile, docID);
		vector<char> vBuf;
		vector<char*> vFieldPtr;
		vector<int> vShowFields;
		vShowFields.push_back(m_vFieldIndex[TITLENAME]);
		m_funcDocInfoPtr(vRes[i].nDocId, vShowFields, vFieldPtr, vBuf, m_pSearcher);
		cerr << "debug_weight=" << vRes[i].nWeight << "  rank_score=" << vRes[i].nScore << " title=" << vFieldPtr[0] << " pid=" << pid << endl;
	}
#endif
}

void CSearchKeyRanking::ReRanking(vector<SResult>& vRes, IAnalysisData* pa)
{
	CDDAnalysisData* pda = (CDDAnalysisData*)pa;

	bool isMix = false;
	int nWeight = 0, synWeight = 0;
	if(pa->m_hmUrlPara[IS_MIX_SEARCH] == "1")
	{
		isMix = true;
	}
	size_t count = vRes.size();
	if(isMix)
	{
		for(int i = 0; i < count; i++)
		{
			nWeight = GetSearchType(vRes[i].nWeight);
			if(nWeight == ORG_KEY)
			{
				if(0 == ((vRes[i].nScore >> BASERELBIT) & ScoreBitMap[BASERELBITNUM]))
				{
					vRes[i].nScore = 0;
				}
			}
			else if(nWeight == SYN_KEY)
			{
				synWeight = GetSynKeyId(vRes[i].nWeight);
				int textRel = 0;
				switch(synWeight)
				{
					case 0:
					case 1:
						if(1 == ((vRes[i].nScore >> TEXTRELBIT) & ScoreBitMap[TEXTRELBITNUM]))
						{
							vRes[i].nScore = 0;
						}
						break;
					case 2:
						textRel = ((vRes[i].nScore >> TEXTRELBIT) & ScoreBitMap[TEXTRELBITNUM]);
						if(textRel <= 2)
						{
							int termDis = ((vRes[i].nScore >> TERMDISBIT) & ScoreBitMap[TERMDISBITNUM]);
							if(termDis <= 1 || textRel == 1)
							{
								vRes[i].nScore = 0;
							}
						}
						break;
					case 3:
						textRel = ((vRes[i].nScore >> TEXTRELBIT) & ScoreBitMap[TEXTRELBITNUM]);
						if(textRel <= 2)
						{
							vRes[i].nScore = 0;
						}
						break;
					default:
						break;
				}
			}
			else if(nWeight == RECO_PRO)
			{
			}
			else if(nWeight == FUSSY_TYPE)
			{
			}
		}
	}
	else
	{
		double avgLev = (double)pda->m_AnalysisPart.sumLevel / (count + 1);
		double signVal;
		if(avgLev - pda->m_AnalysisPart.minLevel > 0.5)
		{
			signVal = pda->m_AnalysisPart.minLevel;
		}
		else
		{
			signVal = avgLev;
		}
		for(int i = 0; i < count; i++)
		{
			if(0 == ((vRes[i].nScore >> BASERELBIT) & ScoreBitMap[BASERELBITNUM]))
			{   
				vRes[i].nScore = 0;
			}
			else
			{
				if(((vRes[i].nWeight >> LEVELBIT) & ScoreBitMap[LEVELBITNUM]) > signVal)
				{
					//ÐèÈ·ÈÏÂú×ã»ù±¾Ïà¹ØµÄÉÌÆ·ÎÄ±¾Ïà¹Ø¶È´ò·ÖÒ»¶¨²»µÈÓÚ0
					vRes[i].nScore -= (1 << TERMDISBIT);
				}
			}
		}
	}
	vRes.erase(std::remove_if(vRes.begin(), vRes.end(), filt_zero_score), vRes.end());
	GetShowTemplate(vRes, pda);
}

void CSearchKeyRanking::NoDefoutSortFilter(IAnalysisData* pa,vector<int>& vDocIds)
{
	CDDAnalysisData* pda = (CDDAnalysisData*)pa;
	set<int>::iterator iter;
	//·ÇÄ¬ÈÏÅÅÐò·ÇÖ÷Àà¹ýÂË
	if(pda->m_main_cate_set.size() > 0 && vDocIds.size() > 200 && !pda->m_AnalysisPart.ifBrdQuery)
	{
		u64 cid = 0;
		int second_cate = 0;
		bool flag = false;
		int main_cate = 0;
		for(int i=0; i<vDocIds.size(); i++)
		{
			flag = false;
			cid = m_funcFrstInt64(m_clsProfile,vDocIds[i]);
			if(PUB_CATE == (int)GetClassByLevel(1,cid))
				main_cate = PUB_CATE;
			else
			{
				second_cate = (int)GetClassByLevel(2,cid);
				if(second_cate == PHONE_CATE || second_cate == DIGIT_CATE ||
				   second_cate == COMPUTER_CATE || second_cate == ELECTR_CATE)
				{
					main_cate = cid;
				}
				else
				{
					main_cate = (int)GetClassByLevel(3,cid);
				}
			}
			for(iter = pda->m_main_cate_set.begin(); iter != pda->m_main_cate_set.end(); iter++)
			{
				if(main_cate == *iter)
				{
					flag = true;
					break;
				}
			}
			//·ÇµÚÒ»Ö÷Àà¹ýÂË
			if(flag == false)
			{
				vDocIds[i] = -1;
				continue;
			}
		}
	}
	vDocIds.erase(remove(vDocIds.begin(),vDocIds.end(),-1),vDocIds.end());
}

//»ñÈ¡Í¼Êé¡¢·þ×°/Ð¬Ñ¥¡¢3C¡¢ÆäËü°Ù»õÉÌÆ·µÄ±ÈÀý
float CSearchKeyRanking::GetAlpha(const string& query, const int cat)
{
	HASHVECTOR hvect = m_percentPub2Mall[query];
	if(hvect.count != 0)
	{
		//ÔÚ·´À¡ÎÄ¼þÖÐÄÜ¹»ÕÒµ½
		float alpha;
		memcpy(&alpha, hvect.data+cat*hvect.size, sizeof(float));
		if(alpha > 0.94)
		{
			alpha = 1.0;
		}
		else if(alpha < 0.07)
		{
			alpha = 0.0;
		}
		return alpha;
	}
	return 0.5; //Ä¬ÈÏ·µ»Ø¸ÃÖµ
}

void CSearchKeyRanking::ReComputeWeightFullSite(vector<SResult>& vRes, float alpha, int &bhcount)
{
	for(int i = 0; i < vRes.size(); i++)
	{
		if (((vRes[i].nWeight >> ISMALLBIT) & ScoreBitMap[ISMALLBITNUM]) == 1)   //ÊÇ·ñÊÇ°Ù»õ
		{
			bhcount++;
		}
	}
}

void CSearchKeyRanking::GetShowTemplate(vector<SResult>& vRes, CDDAnalysisData* pa)
{
	//»ñÈ¡»ìÅÅÎÄ¼þÖÐ³ö°æÎïµÄ±ÈÖØ
	int pubNum = CDDAnalysisData::PUB_SEARCH;
	alpha[pubNum] = GetAlpha(pa->m_AnalysisPart.queryStr, pubNum); //@todo: ÕâÀïqueryStrÈ¥µôÁË±êµã·ûºÅºÍ¿Õ¸ñ£¬È·ÈÏÊÇ·ñ
	if(pa->m_AnalysisPart.ifBrdQuery)
	{
		for(int i = 0; i < vRes.size(); i++)
		{
			if(((vRes[i].nWeight >> ISMALLBIT) & ScoreBitMap[ISMALLBITNUM]) != 1)   //ÊÇ·ñÊÇ³ö°æÎï
			{
				vRes[i].nScore = (int)(vRes[i].nScore * 0.95);
			}
		}
	}
	int bhcount = 0;
	ReComputeWeightFullSite(vRes, alpha[pubNum], bhcount);

	float bhtmp = 0.0f;
	if(vRes.size() != 0)
		bhtmp = bhcount /(float)vRes.size();
	set<int>::iterator iter;
	//·ÇÄ¬ÈÏ Ä£°åÑ¡Ôñ Óë Ä¬ÈÏÏàÒ»ÖÂ
	bool bflag = false;
	bool pflag = false;
	int cate_count = 0;
	for(iter = pa->m_main_cate_set.begin(); iter != pa->m_main_cate_set.end() && cate_count++ < 2; iter++)
	{
		if(B2C_CATE == (int)GetClassByLevel(1,*iter) ||
			LOCATE_LIFE_CATE == (int)GetClassByLevel(1,*iter))
		{
			bflag = true;
		}
		else
		{
			pflag = true;
		}
	}
	if(bflag || pa->m_AnalysisPart.ifBrdQuery)
	{
		if(pa->m_strReserve.size() != 0)
			pa->m_strReserve += ";WEB:BH_TEMPLATE";
		else
			pa->m_strReserve += "WEB:BH_TEMPLATE";
	}
	else if(pflag)
	{
		if(pa->m_strReserve.size() != 0)
			pa->m_strReserve += ";WEB:PUB_TEMPLATE";
		else
			pa->m_strReserve += "WEB:PUB_TEMPLATE";
	}
	else
	{
		if(alpha[pubNum] >0.99)
		{
			if(pa->m_strReserve.size() != 0)
				pa->m_strReserve += ";WEB:PUB_TEMPLATE";
			else
				pa->m_strReserve += "WEB:PUB_TEMPLATE";
		}
		else if(alpha[pubNum] < 0.01)
		{
			if(pa->m_strReserve.size() != 0)
				pa->m_strReserve += ";WEB:BH_TEMPLATE";
			else
				pa->m_strReserve += "WEB:BH_TEMPLATE";
		}
		else
		{
			if(bhtmp > 0.4)
			{
				if(pa->m_strReserve.size() != 0)
					pa->m_strReserve += ";WEB:BH_TEMPLATE";
				else
					pa->m_strReserve += "WEB:BH_TEMPLATE";
			}
			else
			{
				if(pa->m_strReserve.size() != 0)
					pa->m_strReserve += ";WEB:PUB_TEMPLATE";
				else
					pa->m_strReserve += "WEB:PUB_TEMPLATE";
			}
		}
	}

}

static inline void SwapResVectElem(SResult& a, SResult& b)
{
	SResult sr;
	sr = a;
	a = b;
	b = sr;
}

bool CSearchKeyRanking::RemoveGoodsByForceAndFB(const string& query, 
		vector<SResult>& vRes, vector<SResult>& force_vect)
{
	force_vect.clear();
	int pid = 0;
	int stock = 0;
	int weight = 0;
	int docId_max = 0;
	int pid_flag = 0;
	vector<long long> pid_vect;
	vector<int> docId_vect;
	if(vRes.empty() || query.empty())
	{
		return false;
	}
	//ÅÐ¶ÏÊÇ·ñÓÐ·´À¡
	HASHVECTOR hvect = m_key2Pid[query];
	if(hvect.count > 0)
	{
		//È¡µÃ·´À¡pid
		pid_vect.reserve(hvect.count);
		for(int j = 0; j < hvect.count; j++)
		{
			memcpy(&pid, hvect.data + j * hvect.size, sizeof(int));
			memcpy(&weight, hvect.data + j * hvect.size+sizeof(int), sizeof(int));
			if(weight <= 3)//·´À¡È¨ÖØÐ¡ ¹ýÂË
				continue;
			pid_vect.push_back((long long)pid);
		}
		if(pid_vect.empty())
			return false;
		//½«pid×ª»¯ÎªdocId
		pid_flag = GetFieldId("product_id");
		m_funcGetDocsByPkPtr(m_pSearcher, pid_flag, pid_vect, docId_vect);
		docId_max = GetMaxValFromVect(vRes);
		//×°ÈëBITMAP
		CBitMap bitMap(docId_max + 1);
		for(size_t i = 0; i < docId_vect.size(); i++)
		{
			if(docId_vect[i] < 0 || docId_vect[i] > docId_max)
			{
				continue;
			}
			bitMap.SetBit(docId_vect[i]);
		}
		//µ÷Õûforce_vectµÄ´óÐ¡
		//±éÀúvect£¬ÐÞ¸ÄÆäÇ¿ÖÆpid¶ÔÓ¦µÄÈ¨ÖØ£¬ÁíÆä×î´ó
		for(size_t j = 0; j < vRes.size();)
		{
			if(vRes[j].nDocId > docId_max || vRes[j].nDocId < 0)
			{
				j++;
				continue;
			}
			if(bitMap.TestBit(vRes[j].nDocId))
			{
				stock = m_funcFrstInt(m_stockProfile, vRes[j].nDocId) > 0 ? 1 : 0;
				if(stock == 1)
				{
					//É¾³ý,´òÉ¢ºóÔÙÇ¿ÖÆÖÃ¶¥
					force_vect.push_back(vRes[j]);
					SwapResVectElem(vRes[j], vRes[vRes.size()-1]);
					vRes.pop_back();
				}
				else
					j++;
			}
			else
			{
				j++;
			}
			if(force_vect.size() >= pid_vect.size())
			{
				break;
			}
		}
		docId_vect.clear();
		pid_vect.clear();
	}
	return true;
}

bool CSearchKeyRanking::ForceMoveGoods(vector<SResult>& vRes, vector<SResult>& force_vect)
{
	if(force_vect.empty())
	{
		return false;
	}
	sort(force_vect.begin(),force_vect.end(),greater<SResult>());
	vRes.insert(vRes.begin(), force_vect.begin(), force_vect.end());
	return true;
}

void CSearchKeyRanking::SortForDefault(vector<SResult>& vRes, int from, int to, IAnalysisData* pa)
{
	string query = "";
	vector<SResult> force_vect;
	CDDAnalysisData* pda = (CDDAnalysisData*)pa;	
	if(pa->bAdvance)
	{
		for(size_t t = 0; t < vRes.size(); t++)
		{
			int stock = m_funcFrstInt(m_stockProfile, vRes[t].nDocId) > 0 ? 1 : 0;
			int sale_amt = m_funcFrstInt(m_saleWeekAmtProfile, vRes[t].nDocId);
			vRes[t].nScore = (stock << 24) + sale_amt;
		}
	}


	//caiting adding
	/// @todo only mall ranking model, how about pub ?
	ComputerLRCTR4DocIdList(vRes);


#ifdef DEBUG
	clock_t begin, end;
	float cost;
	begin = clock();
#endif
	if(pa->m_hmUrlPara.find("q") != pa->m_hmUrlPara.end())
	{
		query = pa->m_hmUrlPara["q"];
		if(!RemoveGoodsByForceAndFB(query, vRes, force_vect))
		{
#ifdef DEBUG
			printf("Remove Goods by Force File Failed!\n");
#endif
		}
	}
	
	//modify sorted product num	
	to = to < vRes.size() ? to : vRes.size();

#ifdef DEBUG
	end = clock();
	cost = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("wuhaijun find time is %d seconds\n", end-begin);
#endif

	if(pa->m_hmUrlPara[IS_MIX_SEARCH] == "1")//ÊÇ·ñÊÇ»ìºÏËÑË÷
	{
		partial_sort(vRes.begin(), vRes.begin()+to, vRes.end(), MIX_SEARCH_COMPARE);
	}
	else if(pda->m_searchType == CDDAnalysisData::FULL_SITE_SEARCH)		//È«Õ¾ËÑË÷
	{
#ifdef DEBUG
		printf("enter sort for SortFullSite\n");
#endif
		SortFullSite(vRes, from, to, pda);
	}
	else if(pda->m_searchType == CDDAnalysisData::PUB_SEARCH)		//È·¶¨ÊÇ³ö°æÎïËÑË÷
	{
#ifdef DEBUG
		printf("enter sort for SortPub\n");
#endif
		SortPub(vRes, from, to, pda);
	}
	else //È·¶¨ÊÇ°Ù»õËÑË÷
	{
#ifdef DEBUG
		printf("enter sort for SortMall\n");
#endif
		SortMall(vRes, from, to, pda);
	}
	//ÄÐÅ®¸öÐÔ»¯
	if(!JudgeGoodsSexPartial(pda,vRes,to))
	{
#ifdef DEBUG
		cout<<"judge goods sex error"<<endl;
#endif
	}
	
	//½«Ç¿ÖÆ¹ØÁªµÄÉÌÆ·ÖÃ¶¥
	if(!ForceMoveGoods(vRes,force_vect))
	{
#ifdef DEBUG
		cout<<"Warning force_vect is NULL!"<<endl;
#endif
	}
	//Ä£°åÑ¡È¡
/*
	if(!SelectShowTemplate(pda, vRes,29))
	{
#ifdef DEBUG
		cout<<"Template is error!"<<endl;
#endif
	}
*/
}


//È«Õ¾ÅÅÐò
void CSearchKeyRanking::SortFullSite(vector<SResult>& vRes, int from, int to, CDDAnalysisData* pa)
{
#ifdef DEBUG
	printf("enter sort for full site\n");
#endif

	int iLimit = to < SCATTER_UPPER_LIMIT ? to : SCATTER_UPPER_LIMIT ;	

	//get page size from m_hmUrlPara
	int page_size = 5;
	hash_map<string,string>::iterator itPS  = pa->m_hmUrlPara.find(PS);
	if(itPS != pa->m_hmUrlPara.end())
	{
		page_size = atoi(itPS->second.c_str());
		if(page_size == 0) 
		{
			page_size = 1; 
		}
		if(page_size > 200)
		{
			page_size = 200;
		}
	}

	// load feedback data
	//printf("enter sort for full site\n");
	//int cate_count = m_key2Cate[pa->m_AnalysisPart.queryStr].count;	
	int cate_count = pa->m_AnalysisPart.vCate.size();
	cate_count = cate_count < SCATTER_CAT_LIMIT ? cate_count : SCATTER_CAT_LIMIT;
#ifdef DEBUG
	cout<<"key word: "<<pa->m_AnalysisPart.queryStr<<" feedback cnt: "<<cate_count<<endl;
#endif
	SReferCatInfo sCatInfo;
	int total_rate = 0;	
	int have_scatt = 0;
	memset(&sCatInfo, 0x0, sizeof(SReferCatInfo));
	int iLev = 0;
	if(cate_count)
	{
		/*HASHVECTOR cate_vec = m_key2Cate[pa->m_AnalysisPart.queryStr];	
		for(int i = 0; i < cate_count; i++)
		{
			u64 cate_id = 0;
			int weight  = 0;
			memcpy(&cate_id, cate_vec.data+i*cate_vec.size, sizeof(u64));
			memcpy(&weight, cate_vec.data+i*cate_vec.size+sizeof(u64), sizeof(int));
			sCatInfo.cid  = cate_id;
			iLev = GetClsLevel(cate_id);
			sCatInfo.iLev = iLev;
			sCatInfo.rate = weight;
			pa->vReferCat.push_back(sCatInfo);	
			total_rate += weight;
#ifdef DEBUG
			char cate_path[40];
			TranseID2ClsPath(cate_path, cate_id, 6);
			cout<<"id: "<<cate_id<<" feedback cat_paths: "<<cate_path<<endl;
#endif
		}*/
		//for(int i = 0; i < pa->m_AnalysisPart.vCate.size(); ++i)
		for(int i = 0; i < cate_count; ++i)
		{
			u64 cate_id = pa->m_AnalysisPart.vCate[i].first;
			int weight = pa->m_AnalysisPart.vCate[i].second;
			sCatInfo.cid  = cate_id;
			iLev = GetClsLevel(cate_id);
			sCatInfo.iLev = iLev;
			sCatInfo.rate = weight;
			pa->vReferCat.push_back(sCatInfo);
			total_rate += weight;
		}
	}

	// adjust category weight
	for(int j = 0; j < pa->vReferCat.size(); j++)
	{
		if(j == pa->vReferCat.size() - 1)	
		{
			pa->vReferCat[j].rate = page_size - have_scatt;	
		}
		else
		{
			pa->vReferCat[j].rate = (int)(page_size * (float)(pa->vReferCat[j].rate/((float)total_rate)));	
			have_scatt += pa->vReferCat[j].rate ;
		}
	}
	
	//brand sort or no need scatter	
	if(pa->vReferCat.size() < 1 || pa->m_AnalysisPart.ifBrdQuery)	
	{
		SortRange(vRes ,from, to);
	}
	else
	{
		ScatterCategory(vRes,iLimit,pa);
	}

#ifdef DEBUG
	int out_size = min((int)vRes.size(), 50);
	for (int i=0; i<out_size; i++) {
		int docID = vRes[i].nDocId;
		int date = m_funcFrstInt(m_inputDateProfile, docID);
		int modifytime = m_funcFrstInt(m_modifyTime, docID);
		int pid = m_funcFrstInt64(m_isPidProfile, docID);
		vector<char> vBuf;
		vector<char*> vFieldPtr;
		vector<int> vShowFields;
		vShowFields.push_back(GetFieldId("product_name"));
		m_funcDocInfoPtr(vRes[i].nDocId, vShowFields, vFieldPtr, vBuf, m_pSearcher);
		cerr << "debug_weight=" << vRes[i].nWeight << "  rank_score=" << vRes[i].nScore << " title=" << vFieldPtr[0] << " pid=" << pid << endl;
	}
#endif
}



void CSearchKeyRanking::FillGroupByData(vector<pair<int, vector<SGroupByInfo> > >& vGpInfo)
{
	//µ÷ÓÃÆäËüÄ£¿éÊ¾Àý :ÓÃlist rankingÌî³äÁÐ±íÊý¾Ý 
	MOD_MAP::iterator i = m_mapMods->find("list_ranking");
	if ( i != m_mapMods->end())
	{
		i->second.pMod->FillGroupByData(vGpInfo);
	}
}


void CSearchKeyRanking::ShowResult(IAnalysisData* pad, CControl& ctrl, GROUP_RESULT &vGpRes,
		vector<SResult>& vRes, vector<string>& vecRed, string& strRes)
{
	// ÕûÀíÒµÎñÊ±ºòÈ·ÈÏ //ÏÈµ÷ÓÃÄ¬ÈÏµÄ
	/*MOD_MAP::iterator i = m_mapMods->find("default_ranking");
	  if ( i != m_mapMods->end())
	  {
	  i->second.pMod->ShowResult(pad, ctrl, vGpRes, vRes, vecRed, strRes);
	  }*/
	MOD_MAP::iterator i = m_mapMods->find("query_pack");
	if ( i != m_mapMods->end())
	{
		i->second.pMod->ShowResult(pad, ctrl, vGpRes, vRes, vecRed, strRes);
	}

}

void CSearchKeyRanking::QueryRewrite(hash_map<string,string>& hmParams)
{
	MOD_MAP::iterator i = m_mapMods->find("query_pack");
	if(hmParams.find("q") != hmParams.end())
	{
		hmParams[EX_SEARCH_MODULE] = "mix_reco";
	}
	if ( i != m_mapMods->end())
	{
		i->second.pMod->QueryRewrite(hmParams);
	}
}

void CSearchKeyRanking::SetGroupByAndFilterSequence(IAnalysisData* pad, vector<SFGNode>& vec)
{
	MOD_MAP::iterator i = m_mapMods->find("query_pack");
	if ( i != m_mapMods->end())
	{
		i->second.pMod->SetGroupByAndFilterSequence(pad,vec);
	}
}

void CSearchKeyRanking::CustomFilt(IAnalysisData* pad, vector<int>& vDocIds, SFGNode& fgNode)
{
	//no defaut sort flite
	if(fgNode.nId == NO_DEFAUT_SORT) 
	{
		NoDefoutSortFilter(pad,vDocIds);
	}
	MOD_MAP::iterator i = m_mapMods->find("query_pack");
	if ( i != m_mapMods->end())
	{
		i->second.pMod->CustomFilt(pad,vDocIds, fgNode);
	}
}

void CSearchKeyRanking::CustomGroupBy(IAnalysisData* pad, vector<int>& vDocIds, SFGNode& gfNode,
		pair<int, vector<SGroupByInfo> >& prGpInfo)
{
	MOD_MAP::iterator i = m_mapMods->find("query_pack");
	if ( i != m_mapMods->end())
	{
		i->second.pMod->CustomGroupBy(pad,vDocIds, gfNode,prGpInfo);
	}

}

void CSearchKeyRanking::SortForCustom(vector<SResult>& vRes, int from, int to, IAnalysisData* pad)
{
	MOD_MAP::iterator i = m_mapMods->find("query_pack");
	if ( i != m_mapMods->end())
	{
		i->second.pMod->SortForCustom(vRes,from, to,pad);
	}
}

void CSearchKeyRanking::BeforeGroupBy(IAnalysisData* pad, vector<SResult>& vRes, vector<PSS>& vGpName, GROUP_RESULT& vGpRes)
{
	MOD_MAP::iterator i = m_mapMods->find("query_pack");
	if ( i != m_mapMods->end())
	{
		i->second.pMod->BeforeGroupBy(pad,vRes,vGpName,vGpRes);
	}

}
//=============================================whj======================================================
SMainCate CSearchKeyRanking::JudgeResultMainCate(vector<SResult>& vRes, int end)
{
	int max = 0;
	int second_max = 0;
	u64 cid = 0;
	int main_cate = 0;
	int statistic_cate[2] = {0};
	SMainCate main_date_struct;
	memset(&main_date_struct,0,sizeof(main_date_struct));
	hash_map<int,int> statistic_hash;
	int num = vRes.size() > end ? end : vRes.size();
	for(int i=0; i<num; i++)
	{
		cid = m_funcFrstInt64(m_clsProfile,vRes[i].nDocId);
		main_cate = (int)GetClassByLevel(1,cid);
		if(main_cate == PUB_CATE)
			statistic_cate[0]++;
		else if(main_cate == B2C_CATE)
		{
			statistic_cate[1]++;
			main_cate = (int)GetClassByLevel(2,cid);
			statistic_hash[main_cate]++;
		}
	}
	main_date_struct.is_b2c = statistic_cate[0]>(statistic_cate[1]) ? 0 : 1;
	//ÕÒµ½×î¶à2¼¶Àà
	hash_map<int,int>::iterator iter;
	for(iter=statistic_hash.begin(); iter!=statistic_hash.end(); iter++)
	{
		if(max < iter->second)
		{
			second_max = max;
			max = iter->second;
			main_date_struct.first_cate = iter->first;
		}
		else if(second_max < iter->second)
		{
			second_max = iter->second;
			main_date_struct.second_cate = iter->first;
		}
	}
	return main_date_struct;
}
/*
SMainCate CSearchKeyRanking::JudgeResultMainCate(vector<SResult>& vRes, int end)
{
	int pos;
	int max = 0;
	int second_max = 0;
	u64 cid = 0;
	int main_cate = 0;
	int statistic_cate[2] = {0};
	SMainCate main_date_struct;
	memset(&main_date_struct,0,sizeof(main_date_struct));
	hash_map<int,int> statistic_hash;
	int num = vRes.size() > end ? end : vRes.size();
	for(int i=0; i<num; i++)
	{
		cid = m_funcFrstInt64(m_clsProfile,vRes[i].nDocId);
		main_cate = (int)GetClassByLevel(1,cid);
		//cout<<"doc:"<<vRes[i].nDocId<<"\t"<<"main cate:"<<main_cate<<endl;
		//Í³¼Æ³ö°æÎï
		pos = (main_cate == PUB_CATE ? 0 : (main_cate == B2C_CATE ? 1 : 2));
		if(pos == 2 || pos < 0 ) continue;
		statistic_cate[pos] += 1;
	}
	if(num > 10) statistic_cate[1] += 6;
	return statistic_cate[0]>statistic_cate[1] ? 0 : 1;
}
*/
bool CSearchKeyRanking::SelectShowTemplate(CDDAnalysisData* pa,vector<SResult>& vRes, int end)
{
	SMainCate main_date_struct;
	main_date_struct = JudgeResultMainCate(vRes,end);
	if(main_date_struct.is_b2c == 0)
	{
		FindAndReplaceShowTempalte(pa->m_strReserve,"WEB:PUB_TEMPLATE");
	}
	else
	{
		FindAndReplaceShowTempalte(pa->m_strReserve,"WEB:BH_TEMPLATE");
	}
	return true;
}

void CSearchKeyRanking::FindAndReplaceShowTempalte(string& str_reserve,string str_template)
{
	if(str_template.empty())
		return;
	int pos = 0;
	string str_pub = "WEB:PUB_TEMPLATE";
	string str_b2c = "WEB:BH_TEMPLATE";
	string str_unknown = "WEB:UNKNOWN_TEMPLATE";
	if((pos = str_reserve.find(str_pub)) != string::npos)
	{
		str_reserve.erase(str_reserve.begin()+pos,str_reserve.begin()+pos+str_pub.size());
	}
	else if((pos = str_reserve.find(str_pub + ";")) != string::npos)
	{
		str_reserve.erase(str_reserve.begin()+pos,str_reserve.begin()+pos+str_pub.size() + 1);
	}
	else if((pos = str_reserve.find(str_b2c))   != string::npos)
	{
		str_reserve.erase(str_reserve.begin()+pos,str_reserve.begin()+pos+str_b2c.size());
	}
	else if((pos = str_reserve.find(str_b2c + ";"))   != string::npos)
	{
		str_reserve.erase(str_reserve.begin()+pos,str_reserve.begin()+pos+str_b2c.size() + 1);
	}
	else if((pos = str_reserve.find(str_unknown)) != string::npos)
	{
		str_reserve.erase(str_reserve.begin()+pos,str_reserve.begin()+pos+str_unknown.size());
	}
	else if((pos = str_reserve.find(str_unknown + ";")) != string::npos)
	{
		str_reserve.erase(str_reserve.begin()+pos,str_reserve.begin()+pos+str_unknown.size() + 1);
	}
	if(str_reserve.empty())
		str_reserve += str_template;
	else
		str_reserve += ";" + str_template;
}


void CSearchKeyRanking::GetFrstMostElem(SResult& sr,vector<SResult> &vRes,int& frst_cnt,SScatterCategory& sc)
{
	if(vRes.size() < FISRT_CATE_LIMIT)
	{
		vRes.push_back(sr);
		frst_cnt++;
	}
	else
	{
		sort(vRes.begin(),vRes.end());
		for(int i = 0; i<vRes.size(); i++)
		{
			if(sr > vRes[i])
			{
				vRes[i] = sr;
				break;
			}
		}
		sc.cnt++;	
	}
}

void CSearchKeyRanking::ScatterCategory(vector<SResult>& vRes,int scatter_upper,CDDAnalysisData* pa)
{
	if(vRes.empty())
		return ;

	vector<SScatterCategory>  vScatt;
	hash_map< long long,int > hIdToPos;
	hash_map< long long,int >::iterator it; 
		
	u64 clsId = 0;
	int iLev = 0;
	int i,j;
	int total = 0; 
	int ccnt = 0;
	int fst_cat = 0;
	int rel_score = 0;
	int is_pub = 0;
	int stock_status = 0;
	bool no_stock = true;
	SScatterCategory scat;
	vector<SReferCatInfo> &vCat = pa->vReferCat;
	
	vector<SResult> vFrstCat;
	hash_map<int,int> hFrstCat;
	hash_map<int,int>::iterator itFrst;
	vector<u64> vclsId(vRes.size());
			
#ifdef AB	
	TimeUtil tres1;
#endif
	int mm1 = 0;
	int mm2 = 0;
	//stat category cnt
	for(j = 0;j < vRes.size();j++)
	{
		rel_score = ((vRes[j].nScore >> 24) & 0x7);	

		vclsId[j] = m_funcFrstInt64(m_clsProfile,vRes[j].nDocId);
		for(i = 0;i < vCat.size();i++)
		{
			clsId = vCat[i].cid;	
			//iLev = GetClsLevel(clsId);
			if(!vCat[i].rate)
				continue;	
			//if(GetClassByLevel(vCat[i].iLev,m_funcFrstInt64(m_clsProfile,vRes[j].nDocId)) == clsId
			if(GetClassByLevel(vCat[i].iLev,vclsId[j]) == clsId
					//low score or pub no stock push in other category
					&& rel_score >= 2 && no_stock)			
			{
				ccnt++;
				if((it = hIdToPos.find(clsId)) == hIdToPos.end())
				{
					memset(&scat,0x0,sizeof(SScatterCategory));
					hIdToPos.insert(make_pair(clsId,vScatt.size()));
					scat.cid = clsId;
					scat.sunit = vCat[i].rate;
					scat.iLev = vCat[i].iLev;
					vScatt.push_back(scat);
				}
				
				if(!i)	
				{
					 GetFrstMostElem(vRes[j],vFrstCat,fst_cat,vScatt[hIdToPos[clsId]]);
				}
				else
				{
					vScatt[hIdToPos[clsId]].cnt++;
				}
				break;
			}
		}
	}
	ccnt = ccnt - fst_cat;

#ifdef AB 
	int n1 = tres1.getPassedTime();
	cout<<"first get pos cost : "<<n1<<endl;
#endif
#ifdef AB	
	TimeUtil tres2;
#endif
	for(i = 0;i < vFrstCat.size(); i++)
	{
		hFrstCat.insert(make_pair(vFrstCat[i].nDocId,vFrstCat[i].nDocId));
	}
	memset(&scat,0x0,sizeof(SScatterCategory));
	hIdToPos.insert(make_pair(0,vScatt.size()));
	
	vector<SScatterCategory>::iterator itErase;
	for(itErase=vScatt.begin();itErase!=vScatt.end();)
	{
		if(!(*itErase).cnt)
		{
			itErase=vScatt.erase(itErase);
		}
		else
		{
			itErase++;
		}
	}
	scat.cnt = vRes.size() - ccnt - fst_cat;
	vScatt.push_back(scat);
	
	sort(vFrstCat.begin(),vFrstCat.end(),greater<SResult>());
	scatter_upper = scatter_upper - fst_cat;

	
	//get scatter docid 
	total = 0; 
	vector<int> vIdPos(vScatt.size());
	
	if(vRes.size() - fst_cat <= 0)
	{
		sort(vRes.begin(),vRes.end(),greater<SResult>());			
		return;
	}
	
	vector<SResult> vTmpRes(vRes.size() - fst_cat);
	bool bFlag = true;
	for(i = 0;i < vScatt.size();i++)
	{
		vIdPos[i] = total;
		total += vScatt[i].cnt;
	}
	
	vector<int> vTmpPos = vIdPos;
	for(j = 0;j < vRes.size();j++)
	{
		if((itFrst = hFrstCat.find(vRes[j].nDocId)) != hFrstCat.end())	
		{
			continue;
		}
		
		bFlag = false;	
		rel_score = ((vRes[j].nScore >> 24) & 0x7);	
		
		for(i = 0;i < vScatt.size() - 1;i++)
		{
			clsId = vScatt[i].cid;	
			//iLev = GetClsLevel(clsId);
			//if(GetClassByLevel(vScatt[i].iLev,m_funcFrstInt64(m_clsProfile,vRes[j].nDocId)) == clsId
			if(GetClassByLevel(vScatt[i].iLev,vclsId[j]) == clsId
					//low score or pub no stock push in other category
					&& rel_score >= 2 && no_stock)			
			{
				bFlag = true;	
				vTmpRes[vTmpPos[i]++] = vRes[j];
				break;
			}
		}
		if(!bFlag)
		{
			vTmpRes[vTmpPos[i]++] = vRes[j];	
		}

	}
#ifdef AB 
	int m1 = tres2.getPassedTime();
	cout<<"second get pos cost : "<<m1<<endl;
#endif
#ifdef AB 
	TimeUtil tscat;
#endif
	//get scatter count
	total = 0;
	if(ccnt <= scatter_upper)
	{
		for(i = 0;i < vScatt.size() - 1;i++)		
		{
			vScatt[i].scnt = vScatt[i].cnt;
		}
		vScatt[i].scnt = scatter_upper - ccnt;
	}
	else
	{
		while(total <= scatter_upper)	
		{
			for(i = 0;i < vScatt.size() - 1;i++)		
			{
				total = vScatt[i].scnt < vScatt[i].cnt ? total + (vScatt[i].scnt
						+ vScatt[i].sunit < vScatt[i].cnt ? vScatt[i].sunit:
						vScatt[i].cnt - vScatt[i].scnt) : total;

				vScatt[i].scnt = vScatt[i].scnt + vScatt[i].sunit < vScatt[i].cnt ?
					vScatt[i].scnt + vScatt[i].sunit : vScatt[i].cnt;

			}
		}
	}
#ifdef AB 
	int m7 = tscat.getPassedTime();
	cout<<"scatter cost : "<<m7<<endl;
#endif
#ifdef AB	
	TimeUtil tpart;
#endif
	// sort the data which need scatter
	SResult* beg,*end,*mid;	
	vector<int> vStartPos = vIdPos;
	for(i = 0; i < vScatt.size(); i++)
	{
		if(vScatt[i].cnt > 1)	
		{
			beg = &vTmpRes[vStartPos[i]];		
			end = beg + vScatt[i].cnt;
			mid = beg + vScatt[i].scnt;
			SimplePartialSortDesc(beg,mid,end);

			if(m_price_info_map.find(vScatt[i].cid) != m_price_info_map.end())
			{
				//cout << "into the price scatter" << endl;
				SimplePartialSortDesc(mid, end, end);
				vector<pair<int, int> >& price_rate = m_price_info_map[vScatt[i].cid];
				Price_Scatter(beg, mid, end, price_rate);
			}
		}
	}
#ifdef AB 
	int m2 = tpart.getPassedTime();
	cout<<"partial sort cost : "<<m2<<endl;
#endif

#ifdef AB 
	TimeUtil twinner;
#endif
	/*int n = 0;
	int m = 0;
	for(i = 0;i < vScatt.size() ; i++)																			
	{	
		int m = vStartPos[i];
		int n = m + vScatt[i].scnt; 
		while(m!=n)
		{
			cout<<vTmpRes[m].nScore<<endl;
			m++; 
		}
		cout<<"cid: "<<vScatt[i].cid<<endl;
		cout<<endl;
	} */
	// collect each buck
	int col_num = vScatt.size() - 1;			
	int valid_col_num = vScatt.size() - 1;
	SResult dummy;
	int unit_base = 0;;
	dummy.nScore = -1 << 31;
	if(col_num % 2)
		col_num++;
	
	vector<_TREE_NODE<SResult, int> > buf(2 * col_num); 		
	vector<SResult*> sbeg(col_num);		
	vector<SResult*> send(col_num);
	total = 0;

	for(i = 0; i < valid_col_num; i++)
	{
		sbeg[i] = &vTmpRes[vStartPos[i]];
		unit_base = vScatt[i].scnt < vScatt[i].sunit ? vScatt[i].scnt
			: vScatt[i].sunit;
		send[i] = &vTmpRes[vStartPos[i]] + unit_base;
		vStartPos[i] = vStartPos[i] + unit_base ;
		total += unit_base;
	}
	
	if(col_num>valid_col_num)
	{
		sbeg[col_num - 1] = &dummy;	
		send[col_num - 1] = &dummy;
	}
	

	vector<_TREE_NODE<SResult, int> > dest(total);	
	int sorted_col=0;	
	int sorted_cnt = 0;
	
	//put first three SResult
	memcpy(&vRes[sorted_cnt],&vFrstCat[0],sizeof(SResult) * vFrstCat.size());			
	sorted_cnt += vFrstCat.size();

	vector<int> vResPos = vStartPos;
	//get scatter result union
	while(sorted_col < valid_col_num)
	{
		winner_tree_merge(&buf[0], &sbeg[0], &send[0], &dest[0], col_num, dummy);	
		for(i = 0; i < total; i++)
		{
			vRes[sorted_cnt++] = dest[i].n;	
		}

		//get new offset
		total = 0;
		for(i = 0;i < valid_col_num; i++)
		{
			if(vStartPos[i] == -1)	
				continue;
			if(vStartPos[i] >= vIdPos[i] + vScatt[i].scnt)
			{
				sorted_col++;
				sbeg[i] = &vTmpRes[0] + vStartPos[i];		
				vResPos[i] = vStartPos[i];
				vStartPos[i] = -1;
			}
			else
			{
				sbeg[i] = &vTmpRes[vStartPos[i]];		
				if(vStartPos[i] + vScatt[i].sunit>vIdPos[i] + vScatt[i].scnt)
				{
					unit_base = vIdPos[i] + vScatt[i].scnt - vStartPos[i];
				}
				else
				{
					unit_base = vScatt[i].sunit;
				}
				send[i] = &vTmpRes[vStartPos[i]] + unit_base;
				vStartPos[i] += unit_base ; 
				total += unit_base;
			}
			
		}
			
	} 
#ifdef AB 
	int m6 = twinner.getPassedTime();
	cout<<"winner tree cost : "<<m6<<endl;
#endif
#ifdef AB 
	TimeUtil tcollect;
#endif
	//collect sorted left data
	if(vScatt[vScatt.size() - 1].scnt)		
	{
		for(i = 0;i < vScatt[vScatt.size() - 1].scnt; i++)
		{
			vRes[sorted_cnt++] = vTmpRes[vStartPos[vScatt.size() - 1]+i];
		}
		vStartPos[vScatt.size() - 1] = vStartPos[vScatt.size() - 1] + vScatt[vScatt.size() - 1].scnt; 
	}
	
	//collect no sorted left data
	for(i = 0; i < vScatt.size(); i++)
	{
		if(vScatt[i].cnt <= vScatt[i].scnt||sorted_cnt+vScatt[i].cnt-vScatt[i].scnt>vRes.size())
		{
			continue;
		}

		memcpy(&vRes[sorted_cnt],&vTmpRes[vResPos[i]],sizeof(SResult) * 
				(vScatt[i].cnt - vScatt[i].scnt));
		sorted_cnt += vScatt[i].cnt - vScatt[i].scnt;
	}
#ifdef AB 
	int m4 = tcollect.getPassedTime();
	cout<<"collect cost time : "<<m4<<endl;
#endif
}

void CSearchKeyRanking::Price_Scatter(SResult* beg, SResult* mid, SResult* end, vector<pair<int, int> >& price_rate)
{
	//ÇÐ¼Ç¼Û¸ñ×Ö¶ÎÓÉµÍµ½¸ßÇÒrate×ÜºÍÎª100£¬ÕâÐ©¼Ù¶¨¿ÉÒÔÌá¸ßÐ§ÂÊ
	//°´ÕÕ±ÈÖØ¼ÆËã¸÷¸ö¼Û¸ñ¶Î·ÅÖÃµÄÉÌÆ·Êý
	vector<int> unit;
	int sum = mid - beg, count = 0, cnt;
	for(int i = 0; i < price_rate.size(); ++i)
	{
		if(i == price_rate.size()-1)
			cnt = sum - count;
		else
			cnt = (int)(sum * price_rate[i].second / 100.0);
		count += cnt;
		unit.push_back(cnt);
	}

	//¼ÆËã×îÖÕÕ¹Ê¾µÄÉÌÆ·ÖÐ¸÷¸ö¼Û¸ñ¶ÎÓµÓÐµÄÉÌÆ·ÊýÒÔ¼°Ã¿¸öÉÌÆ·ËùÊôµÄ¼Û¸ñ¶Î
	int price;
	vector<int> pos;
	vector<int> priceNum;
	priceNum.resize(price_rate.size(), 0);
	for(SResult* r = beg; r != mid; ++r)
	{
		price = m_funcFrstInt(m_salePriceProfile, r->nDocId) / 100;
		int i;
		for(i = 1; i < price_rate.size(); ++i)
		{
			if(price_rate[i].first > price)
				break;
		}
		++priceNum[i-1];
		pos.push_back(i-1);
	}

	//¼ÆËã×îÖÕÕ¹Ê¾µÄÉÌÆ·ÖÐ¸÷¸ö¼Û¸ñ¶ÎÓµÓÐµÄÉÌÆ·ÊýºÍÒªÇó·ÅÖÃµÄÉÌÆ·ÊýµÄ²î¶î
	map<int, int> price_cnt_more, price_cnt_less;
	for(int i = 0; i < priceNum.size(); ++i)
	{
		if(priceNum[i] > unit[i])
			price_cnt_more.insert(make_pair(i, priceNum[i]-unit[i]));
		else if(priceNum[i] < unit[i])
			price_cnt_less.insert(make_pair(i, unit[i]-priceNum[i]));
	}

	//¶¨Î»Õ¹Ê¾µÄÉÌÆ·ºÍ²»Õ¹Ê¾µÄÉÌÆ·ÖÐÐèÒª½øÐÐµ÷»»µÄÉÌÆ·
	int mark = pos.size() - 1, lessNum = 0;
	vector<SResult> mv2aft;
	vector<int> mv2aftpos, mv2befpos;
	for(SResult* r = mid; r != end; ++r)
	{
		price = m_funcFrstInt(m_salePriceProfile, r->nDocId) / 100;
		int i;
		for(i = 1; i < price_rate.size(); ++i)
		{
			if(price_rate[i].first > price)
				break;
		}
		if(lessNum == price_cnt_less.size())
			break;
		if(price_cnt_less.find(i-1) != price_cnt_less.end())
		{
			if(price_cnt_less[i-1] == 0)
				continue;
			for(; mark >= 0; --mark)
			{
				int pricePara = pos[mark];
				if(price_cnt_more.find(pricePara) != price_cnt_more.end())
				{
					if(price_cnt_more[pricePara] != 0)
					{
						mv2aft.push_back(*(beg+mark));
						mv2aftpos.push_back(mark);
						mv2befpos.push_back(r - beg);
						--price_cnt_more[pricePara];
						--price_cnt_less[i-1];
						--mark;
						if(price_cnt_less[i-1] == 0)
							++lessNum;
						break;
					}
				}
			}
		}
	}

	//½«¶¨Î»³öµÄÉÌÆ·½øÐÐµ÷»»£¬²¢ÔÚÕ¹Ê¾ÉÌÆ·¶Î°´ÕÕ´ò·ÖË³Ðò·ÅÖÃÉÌÆ·
	int sta = mv2aftpos.size()-1;
	if(sta < 0)return;
	int pushPos = mv2aftpos[sta];
	int num = mid - beg;
	for(int i = pushPos + 1; i < num; ++i)
	{
		if(sta > 0)
		{
			if(i == mv2aftpos[sta-1])
			{
				--sta;
				continue;
			}
		}
		*(beg + pushPos) = *(beg + i);
		++pushPos;
	}
	sta = 0;
	for(int i = pushPos; i < num; ++i)
	{
		*(beg + i) = *(beg + mv2befpos[sta]);
		*(beg + mv2befpos[sta]) = mv2aft[sta];
		++sta;
	}
}



void CSearchKeyRanking::SimplePartialSortDesc(SResult* beg, SResult* mid, SResult* end)
{			
	if (mid - beg > 4)
	{		
		partial_sort(beg,mid,end,greater<SResult>());
	}		
	else	 
	{		
		SResult mx,*m, *i, *j;
		for (i = beg; i < mid; ++i )
		{	
			m = i;
			for (j = i+1; j < end; ++j)
			{  
				if (*j > *m)
				{
					m = j;
				}
			}
			mx = *m;
			*m = *i;
			*i = mx;
		}	
	}		
} 

bool CmpbyDocId(pair<int,int> a, pair<int,int> b)
{
	return a.first<b.first;
}

bool CSearchKeyRanking::GetCtrPidHash(CDDAnalysisData* pa, string& query)
{
	pa->m_ctr_pid_weight_hash.clear();
	int pid = 0;
	int weight = 0;
	vector<int> pid_weight_vect;
	if(query.empty()) return false;
	int ctr_count = m_key_pid_weight_hash[query].count;
	if(ctr_count > 0)
	{
		//Àà±ð·´À¡
		pid_weight_vect.reserve(ctr_count);
		HASHVECTOR pid_weight = m_key_pid_weight_hash[query] ;
		for(int k = 0; k < ctr_count; k++) 
		{
			memcpy(&pid, pid_weight.data+k*pid_weight.size, sizeof(int));
			memcpy(&weight, pid_weight.data+k*pid_weight.size+sizeof(int), sizeof(int));
			pa->m_ctr_pid_weight_hash.insert(make_pair(pid,weight));
			pid_weight_vect.push_back(pid);
		}
		pa->m_ctr_bit_map.Init(65536);
		for(int i=0; i<pid_weight_vect.size(); i++)
		{
			pa->m_ctr_bit_map.SetBitSafe(pid_weight_vect[i]%65536);
		}
	}
 	return true;
}

bool CSearchKeyRanking::ComputeCtrWeight(CDDAnalysisData* pa,int doc_id,int& ctr_weight)
{
	ctr_weight = 0;
	if(pa->m_ctr_pid_weight_hash.empty()) 
		return false;
	int pid = (int)m_funcFrstInt64(m_isPidProfile, doc_id);
	if( pid < 0) return false;
	if(pa->m_ctr_bit_map.TestBitSafe(pid%65536))
	{
		if(pa->m_ctr_pid_weight_hash.find(pid) != pa->m_ctr_pid_weight_hash.end())
		{
			ctr_weight = pa->m_ctr_pid_weight_hash[pid];
		}
	}
	return true;
}

#ifndef _WIN32
// linux dll
CSearchKeyRanking search_ranking;
#endif

