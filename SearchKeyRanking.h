#ifndef EG_KEY_RANKING_H
#define EG_KEY_RANKING_H
//#define zhapin
#include <string>
#include <vector>
#include "Module.h"
#include "UtilTemplateFunc.h"
#include "QueryParse.h"
#include "Class.h"
#include "static_hash_vector.h"
#include "ddip.h"
#include "BitMap.h"

#include "weighter/weighter_manager.h"

//#define caiting 2013-11-15
#include "LRModel4CTR.h"

using namespace std;
//主类阈值
const float MainCateThd = 0.35;


const int SCATTER_UPPER_LIMIT = 500;
const int FISRT_CATE_LIMIT    = 3;
const int SCATTER_CAT_LIMIT   = 6;

const int PUB_CATE = 1; //出版物的类别id by TranseClsPath2ID("01",2);
const int B2C_CATE = 88;
const int LOCATE_LIFE_CATE = 147;

const int PHONE_CATE = 32856;
const int COMPUTER_CATE = 25432;
const int ELECTR_CATE = 33368;
const int DIGIT_CATE = 22872;

const float GENDER_CTRL_RATIO = 0.3;
const int ONE_PAGE_NUM = 30;
const int MIN_GENDER_NUM = 27;

struct SMainCate
{
	bool is_b2c;
	int first_cate;
	int second_cate;
};

struct SGenderCtrl
{
	int ctrl_num;
	vector<u64> male_cate_vect;
	vector<u64> female_cate_vect;
};

struct SReferCatInfo
{
	u64 cid;
	int rate;
	int iLev;
};

struct SScatterCategory
{
	u64 cid;
	int cnt;
	int scnt;
	int sunit;
	int iLev;
};

struct QueryAnalysisPart
{
	//基本词特征
	QueryAnalysisPart():brand_id(-1), ifHasFeedback(false), termChara(0), sugFBCate(-1),
			cloth_cat_id(0), c3_cat_id(0), other_cat_id(0), pub_cat_id(0), havMulBrd(false),
	        ifLimitSearch(false), ifAllMallFB(true), minLevel(0), sumLevel(0)
	{
	}
	vector<int> type;						//词性(0-一般 !=0-特殊词：书名、作者等)
	vector<int> dis;						//每个TERM对应字段的偏移
	vector<int> termId;						//存储query分析的结果：词性
	vector<int> mFieldId;					//建议百货搜索字段的id
	vector<int> pFieldId;					//建议图书搜素字段的id
	u64 cloth_cat_id;						//服装类别id的整数表示，获取商业因素得分用
	u64 c3_cat_id;							//3c类别id的整数表示，获取商业因素得分用
	u64 other_cat_id;						//其它百货类别id的整数表示，获取商业因素得分用
	u64 pub_cat_id;							//图书类别id的整数表示，获取商业因素得分用
	int sugFBCate;							//建议反馈的类别，0为图书，1为百货 2为3C 3为非服装类百货
	string queryStr;						//去标点有效词连串
	bool needJudgeDis;						//需要计算词距
	bool havMulBrd;

	//query语义分析
	string pdtWord;							//产品中心词
	bool ifBrdQuery;						//品牌搜索
	bool ifPdtQuery;
	bool ifAllMallFB;						//是否全部为百货反馈类
	int brand_id;							//品牌词对应的id new add
	int termChara;							//判断整个查询串的词性

	//反馈特征
	vector<pair<u64, int> > vCate;			//分类反馈
	
	//出版物专用字段
	bool ifHasFeedback;           			//是否为有反馈query
	bool ifLimitSearch;                		//高级搜索
	int ifAutPubSearch;               		//作者出版社搜索
	vector<set<u64> > vCluster;             //聚类信息
	map<u64, vector<u64> > highRelSaleCate;	//高相关有销量二级类存储
	size_t tDocCnt;                        	//主区较相关文档数统计
	size_t tDocCnt_stock;					//主区较相关文档且有库存数统计

	size_t minLevel;						//统计结果集中匹配词的等级的最小值
	size_t sumLevel;						//统计结果集所有商品的匹配词等级的总和
};

struct FieldToId
{
	char* field;
	int id;
};

inline bool isMall(unsigned long long cls_id)
{
    //百货以58分类开始
    return (*(unsigned char*)&cls_id == 0x58) || (*(unsigned char*)&cls_id == 0x93);
}

/*
手机通讯：58.80.00.00.00.00
数码影音：58.59.00.00.00.00
电脑办公：58.63.00.00.00.00
大家电：  58.82.00.00.00.00
家用电器：58.01.00.00.00.00
*/

inline bool is3C(unsigned long long cls_id)
{
	unsigned char* tmpId = (unsigned char*)&cls_id;
	int firstCate = tmpId[0] & 0xff;
	int secCate = tmpId[1] & 0xff;
	if((firstCate == 0x58) && 
		(secCate == 0x01 || secCate == 0x63 || secCate == 0x82 || secCate == 0x59 || secCate == 0x80))
	{
		return true;
	}
    return false;
}

/*
服装：58.64.00.00.00.00
鞋：58.65.00.00.00.00
箱包皮具：58.74.00.00.00.00
童装童鞋：58.75.00.00.00.00
孕婴服饰：58.76.00.00.00.00
*/

inline bool isCloth(unsigned long long cls_id)
{
    unsigned char* tmpId = (unsigned char*)&cls_id;
	int firstCate = tmpId[0] & 0xff;
    int secCate = tmpId[1] & 0xff;
	if((firstCate == 0x58) && (secCate == 0x64 || secCate == 0x65 || secCate == 0x74 || secCate == 0x75 || secCate == 0x76))
	{
		return true;
	}
    return false;
}

class CDDAnalysisData:public IAnalysisData
{
	public:
		//说明：{图书搜索，服装/鞋靴搜索，3C搜索，其它品类搜索, 全站搜索}
		enum {PUB_SEARCH, CLOTH_SEARCH, C3_SEARCH, OTHER_SEARCH, FULL_SITE_SEARCH, MALL_SEARCH};	//不能随便变动顺序，需要与混排数据类型的顺序一致
		CDDAnalysisData(){m_searchType = 0;}

		int m_searchType;					//查询类型 
		int m_otherSortField;				//查询是否有其他排序字段
		QueryAnalysisPart m_AnalysisPart;

		//=========================whj=========================//
        int m_bit_city_location;
		int m_usr_sort_type;                //排序方式
		set<int> m_main_cate_set;            //二级结果集的主类
		CBitMap m_ctr_bit_map;              //ctr 权重数据
		hash_map<int,int> m_ctr_pid_weight_hash;  //ctr business val
		pair<char,char> m_user_sex_p;
		vector<u64> m_personality_ctrl_cate_vect;
		int personality_num;
		vector<SReferCatInfo> vReferCat;
};

class CSearchKeyRanking:public CModule
{
	public:
		virtual ~CSearchKeyRanking();

		/*
		 *初始化函数需要具体模块重载
		 *Parameter psdi 搜索数据传递
		 *Parameter strConf 模块各自的配置文件地址
		 *return true初始化成功，false初始化失败 
		 */
		virtual bool Init(SSearchDataInfo* psdi, const string& strConf);

		bool InitCommon(SSearchDataInfo* psdi, const string& strConf);
		bool InitData();


		/*
		 * Method:    query分析
		 * Returns:   void
		 * Parameter: SQueryClause & qc 结构化的查询语句
		 * Parameter: CDDAnalysisData& pa IAnalysisData 派生类 
		 * return: IAnalysisData*  返回的query分析数据 用户动态生成，框架负责销毁，生成失败需返回NULL
		 */
		virtual IAnalysisData* QueryAnalyse(SQueryClause& qc);


		/*
		 * Method:    计算文本搜索权重
		 * Returns:   void
		 * Parameter: IAnalysisData * pa 返回的query分析数据
		 * Parameter: SMatchElement & me每个文档的匹配信息
		 * Parameter: SResult & rt 打分结果
		 */
		virtual void ComputeWeight(IAnalysisData* pa, SMatchElement& me, SResult& rt);

		void ComputeWeightMall(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		void BrandQRank(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		void SingleRank(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		void MultiRank(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		void ComputeCommerceWeight(const int, int&, int&, int&, int&);
		inline void ComputeSpecialWeight(const int, CDDAnalysisData*, int&, int&, int&);
		void ChangeWeight(SResult& rt, CDDAnalysisData* pa, u64& cls);

		/*void ComputeWeight3C(CDDAnalysisData*, SMatchElement&, SResult&);
		void BrandQRank3C(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
        void SingleRank3C(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
        void MultiRank3C(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		void ComputeCommerceWeight3C(const int, int&, int&, int&, int&);
		void ComputeSpecialWeight3C(const int, CDDAnalysisData*, int&, int&, int&);
		void ChangeWeight3C(SResult& rt, CDDAnalysisData* pa);

		void ComputeWeightCloth(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		void BrandQRankCloth(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		void SingleRankCloth(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		void MultiRankCloth(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		inline void ComputeSpecialWeightCloth(const int, CDDAnalysisData*, int&, int&, int&);
		void ComputeCommerceWeightCloth(const int, int&, int&, int&, int&);
		void ChangeWeightCloth(SResult& rt, CDDAnalysisData* pa);
		*/

		inline int GetCommerceScore(CDDAnalysisData* pa, u64 cat_id, const int docId)
		{
			WeightData wd;
			wd.base_cat = cat_id;
			wd.city = -1;
			wd.cur_time = -1;
			return m_custom_weighter.Score(docId, &wd);
		}

		inline void MoveLeftBit(int& score, int value, int moveNum)
		{
			score += (value << moveNum);
		}

		inline void merge_field(vector<int>& termId, vector<vector<int> >& termCharaToField, vector<int>& fieldVec);

		inline void JudgeTermCharacter(CDDAnalysisData* pa, const string& key, int& termChara)
		{
			termChara = 0;
			int ret;
			for(int i = 0; i < 7; i++)
			{
				if((ret = m_dict[i][key]) != -1)
				{
					if(i == BRDNAME)
					{
						if(pa->m_AnalysisPart.brand_id != -1)
						{
							if(pa->m_AnalysisPart.brand_id != ret)
							{
								pa->m_AnalysisPart.brand_id = -2;//代表有多个不同品牌词
								continue;
							}
							else
							{
								pa->m_AnalysisPart.havMulBrd = true;
							}
						}
						pa->m_AnalysisPart.brand_id = ret;
					}
					else if(i == AUTNAME)
					{
						pa->m_AnalysisPart.ifAutPubSearch = 1;
					}
					else if(i == PDTWORD)
					{
						pa->m_AnalysisPart.pdtWord = key;
					}
					else if(i == PUBLISHER)
					{
						pa->m_AnalysisPart.ifAutPubSearch = 0;
					}
					MoveLeftBit(termChara, 1, i);
				}
			}
		}

		inline void StatisticLevelInfo(IAnalysisData* pa, SResult& rt)
		{
			CDDAnalysisData* pda = (CDDAnalysisData*)pa;
			int nLevel = (rt.nWeight >> LEVELBIT) & ScoreBitMap[LEVELBITNUM];
			if(nLevel < pda->m_AnalysisPart.minLevel)
			{
				pda->m_AnalysisPart.minLevel = nLevel;
			}
			pda->m_AnalysisPart.sumLevel += nLevel;
		}
		
		///////////////////////////////
		//caiting add 2013-10-25
		double ComputeLRCTR(int nDocId);
	//	bool loadLRweights(string filepath, const int FEATURE_NUM);
	//	bool loadPriceInterval(string filepath, const int nPRICE_INTERVAL);
		bool ComputerLRCTR4DocIdList(vector<SResult>&  vRes);
		double ComputerLRCtrOnline(int nDocId);
		///////////////////////////////


		void ComputeWeightPub(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		//出版物单个词计算权重
		void RankingSinglePub(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		//出版物多个词计算权重
		void RankingMultiPub(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		bool LoadFile();
		//vector<string> GetStrings(const string& stringList, const string& splitWord);

		void QueryRewrite(hash_map<string,string>& hmParams);
		/*
		 * 用于混排的各个函数
		 */
		float GetAlpha(const string& query, const int cat);
		void ReComputeWeightFullSite(vector<SResult>& vRes, float alpha,int &bhcount);


		/*
		 * Method:    精简结果，二次打分等操作，可用于非默认排序
		 * Returns:   void
		 * Parameter: IAnalysisData* Pad, 返回的query分析数据
		 * Parameter: vector<SResult> & vRes 搜索打分结果
		 */
		virtual void ReRanking(vector<SResult>& vRes, IAnalysisData* pad);

		/*
		 * Method:    搜索默认排序（即不存在其他排序条件如价格），分类多样性的结果穿插重排可放这里
		 * Returns:   void
		 * Parameter: vector<SResult> & vRes 搜索打分结果
		 * Parameter: IAnalysisData* Pad, 返回的query分析数据
		 * Parameter: from 用户索取的文档在 vRes中的起始位置
		 * Parameter: to 用户索取的文档在 vRes中的终止位置（不含）
		 * from to 为用户取第几页，每页多少个的另一种表现形式，如取第五页，每页50个 则from=200,to=250
		 */
		virtual void SortForDefault(vector<SResult>& vRes, int from, int to, IAnalysisData* pa);

		void SortFullSite(vector<SResult>& vRes, int from, int to, CDDAnalysisData* pa);
		void SortPub(vector<SResult>& vRes, int from, int to, CDDAnalysisData* pa);
		void SortMall(vector<SResult>& vRes, int from, int to, CDDAnalysisData* pa);

		//强制单品关联
		bool RemoveGoodsByForceAndFB(const string& query,vector<SResult>& vRes,vector<SResult>& force_vect);
		bool ForceMoveGoods(vector<SResult>& vRes,vector<SResult>& force_vect);
		/*
		 * Method:    ShowResult 自定义展现信息<result><cost></cost>*****用户自定义填充内容******</result>"
		 * Returns:   void
		 * Parameter: IAnalysisData* Pad,    返回的query分析数据
		 * Parameter: CControl & ctrl        查询控制参数包括翻页，排序项等
		 * Parameter: GROUP_RESULT & vGpRes  分组汇总统计信息，
		 * Parameter: vector<SResult> & vRes 搜索打分结果
		 * Parameter: vector<string> & vecRed 标红字符串数组
		 * Parameter: string & strRes         返刈址
		 */
		virtual void ShowResult(IAnalysisData* pad, CControl& ctrl, GROUP_RESULT &vGpRes,
				vector<SResult>& vRes, vector<string>& vecRed, string& strRes);

		virtual void SetGroupByAndFilterSequence(IAnalysisData* pad, vector<SFGNode>& vec);
		
		virtual void CustomFilt(IAnalysisData* pad, vector<int>& vDocIds, SFGNode& fgNode);

		 virtual void CustomGroupBy(IAnalysisData* pad, vector<int>& vDocIds, SFGNode& gfNode,
				                             pair<int, vector<SGroupByInfo> >& prGpInfo);
		/*
		 *Method:  填充统计信息，将内部以ID统计的量转换为文字 例如CLASS PATH--> CLASS NAME
		 *Parameter:vGpInfo 传入数组 字段ID对应-->统计信息
		 */
		virtual void FillGroupByData(GROUP_RESULT&  vGpInfo);

	private:
		/** @function
		 *********************************
		 <PRE>
		 函数名：JudgePidHasQueryTag
		 功能    ：判断文档是否有反馈
		 参数    ：
		 * @param  [in] pID      文档对应的产品ID
		 * @param  [in] queryValidString 关键词有效连串
		 * 返回值        反馈权重[0,9]
		 </PRE>
		 **********************************/
		inline void JudgePidHasQueryTag(
				const int pid, const CDDAnalysisData *analysisDat,
				bool& fbCateScr, int& fbPidScr);

		/** @function
		 *********************************
		 <PRE>
		 函数名：JudgeDis
		 功能    ：判断关键词在该文档内是否词距匹配
		 参数    ：
		 * @param  [in] keywords 关键词信息
		 * @param  [in] fidlmt   域限定(-1为全域)
		 </PRE>
		 **********************************/
		/*inline bool JudgeDis(
				const CDDAnalysisData *pa,
				const SMatchElement& me,
				int fidlmt = -1);*/
		/** @function
		 *********************************
		 <PRE>
		 函数名：JudgeDisFilter
		 功能    ：词距过滤器
		 参数    ：
		 * @param  [in] vKeywords        关键词信息
		 </PRE>
		 **********************************/
		/*inline bool JudgeDisFilter(
				const SMatchElement& me);

		inline int ComputeDis(
				const SMatchElement& me,
				size_t fidlmt);*/

		/** @function
		 *********************************
		 <PRE>
		 函数名：JudgeTotalMatch
		 功能    ：判断关键词是否与标题精确匹配
		 参数    ：
		 * @param  [in] objQuery 关键词信息
		 * @param  [in] objDoc   文档信息
		 * @param  [in] fid      字段ID
		 * @param  [in] pos_title        关键词在标题首次出现位置
		 </PRE>
		 **********************************/
		/*inline bool JudgeTotalMatch(
				const string& queryValidString,
				const SMatchElement& me,
				int fid,
				int pos_title);*/
		/** @function
		 *********************************
		 <PRE>
		 函数名：RecordCate
		 功能    ：记录文档所属类别信息
		 参数    ：
		 * @param  [out] vCluster        聚类信息存储
		 * @param  [in] pid      文档产品id
		 * @param  [in] level    文档类别等级
		 * @param  [in] cateScore        类别权重
		 * @param  [in] match    文档基本相关性
		 </PRE>
		 **********************************/
		inline void RecordCate(
				vector<set<u64> >& vCluster,
				map<u64, vector<u64> >& highRelSaleCate,
				int pid, int cateLevel, int highRelSale,
				bool ifAutPubSearch);

		void ScatterCategory(vector<SResult>& vRes,int scatter_upper,CDDAnalysisData* pa);
		
		void Price_Scatter(SResult* beg, SResult* mid, SResult* end, vector<pair<int, int> >& price_rate);
		
		void SimplePartialSortDesc(SResult* beg, SResult* mid, SResult* end);

		inline void GetFrstMostElem(SResult& sr,vector<SResult> &vRes,int& frst_cnt,SScatterCategory& sc);


		void SortForCustom(vector<SResult>& vRes, int from, int to, IAnalysisData* pad);

		void BeforeGroupBy(IAnalysisData* pad, vector<SResult>& vRes, vector<PSS>& vGpName, GROUP_RESULT& vGpRes);
	protected:
		//searcher for get doc data

	private:
		set<string> m_pdtWords;			//产品库

		/***********************混排数据**************************/
		static_hash_vector<string, vector<float> > m_percentPub2Mall;	//0-出版物 1-服装 2-3C 3-其它
		static const int catenum = 4;		//按照分类进行排序数
		float alpha[catenum];				//各分类商品占总商品数的比重,0-出版物 1-服装 2-3C 3-其它

	private:
		//枚举需要与field_id中的元素一一对应
		enum{
			TMINNUM = 0,					//区分重要字段与次要字段
			SUBNAME = 0,
			TITLESUB,
			BOTTOMCATE,
			TITLESYN,
			ACTOR,
			DIRECTOR,
			SINGER,
			AUTHOR,
			TITLENAME,
			PUBNAME,
			SERIES,
			TITLEPRI,
			ISBN,
			BRAND
		};
		//nScore计算权重时使用
		enum{
			FBPIDBIT = 28,
			BASERELBIT 	= 27,
			TEXTRELBIT = 24,
			TERMDISBIT = 22,
			PDTCOREBIT = 21,
			INBRDBIT = 20,
			FBCATEBIT = 16,
			STOCKBIT = 15,
			IMAGEBIT = 14,
			SALEPREBIT = 6,
			COMMERCEBIT = 0
		};
		enum{
			FBPIDBITNUM = 1,
			BASERELBITNUM  = 1,
			TEXTRELBITNUM = 3,
            TERMDISBITNUM = 2,
            PDTCOREBITNUM = 1,
            INBRDBITNUM = 1,
            FBCATEBITNUM = 1,
            STOCKBITNUM = 1,
            IMAGEBITNUM = 1,
            SALEPREBITNUM = 2,
            COMMERCEBITNUM = 6
		};
		//nWeight标识特殊信息使用
		enum{
			LEVELBIT = 4,
			ISMALLBIT = 30
		};
		enum{
			LEVELBITNUM = 4,
			ISMALLBITNUM = 1
		};
		//判断query词性时使用,需要与termCharaToName以及loadfilename.txt中的文件名顺序保持一致
		enum{
			BKNAME = 0,
			BRDNAME,
			AUTNAME,
			PDTWORD,
			CATNAME,
			MODELCODE,
			PUBLISHER
		};

		static const int ScoreBitMap[9];	//获取排序因素对应的位的值
		static const int TermCharaToFieldNum[7];
		static const char* TermCharaToName[7];
		vector<vector<int> > TermCharaToField;
		set<int> mAllField, pAllField;
		static const FieldToId field_id[14];//字段到编号的映射
		vector<int> m_vFieldIndex;			//字段对应的索引
		vector<pair<string, void**> > m_vProfile;	//存储各个函数指针
		static const char* field[20];		//存储某些字段名
		vector<int> m_fid2fi;				//字段索引到编号映射
		vector<short> m_salAmtScr;			//存储销售额的得分
		vector<short> m_salNumScr;			//存储销量的得分
		vector<short> m_commentScr;			//存储评论数的得分

		//pub member
		void* m_totalReviewCountProfile;	//总浏览数字段的属性指针，袢∶扛錾唐纷苣浏览次数
		void* m_preSaleProfile;				//是否预售商品字段的属性指针，获取每个商品是否是预售商品
		void* m_numImagesProfile;			//图片字段的属性指针，获取每个商品的图片
		void* m_salePriceProfile;			//当当卖价字段的属性指针，获取每个商品的销售价格
		void* m_pubDateProfile;				//出版时间字段的属性指针，获取每个出版物的出版时间
		//common member
		void* m_clsProfile;					//分类字段的属性指针，用于获取每个商品对应的分类
		void* m_stockProfile;				//库存字段的属性指针，获取每个商品的库存
		void* m_stockStatusProfile;         //城市库存信息u64 whj========================
		void* m_product_sex_profile;        //personality gender data  whj
		void* m_saleDayProfile;				//日销量字段的属性指针，获取每个商品一周的销量
		void* m_saleWeekProfile;			//周销量字段的属性指针，获取每个商品一周的销量
		void* m_saleMonthProfile;			//周销量字段的属性指针，获取每个商品一周的销量
		void* m_saleDayAmtProfile;			//日销售额字段的属性指针，获取每个商品昨天的销售额
		void* m_saleWeekAmtProfile;			//周销售额字段的属性指针，获取每个商品一周的销售额
		void* m_saleMonthAmtProfile;		//月销售额字段的属性指针，获取每个商品一月的销售额

		void* m_promoPriceProfile;
		void* m_promoStartDateProfile;
		void* m_promoEndDateProfile;
		void* m_scoreProfile;
		void* m_inputDateProfile;			//上架时间字段的属性指针，获取每个商品的上架时间
		void* m_modifyTime;					//最新上架时间字段的属性指针，获取每个商品的最新上架时间
		void* m_isShareProductProfile;		//是否公用品字段的属性指针，获取每个商品是否是公用品
		void* m_isPidProfile;				//商品ID字段的属性指针，获取每个商品的商品ID
		void* m_isBrdidProfile;				//商品品牌ID字段的属性指针，获取每个商品的品牌ID new add
		void* m_isPublicationProfile;
		void* m_proMedProfile;

		static_hash_map<string, int> m_dict[7];
		static_hash_vector<string, vector<pair<u64, int> > > m_key2Cate; 		//分类反馈
		static_hash_vector<string, vector<pair<int, int> > > m_key2Pid;			//单品反馈
		static_hash_vector<int, vector<string> > m_pid2Core;					//商品对应的中心词
		//static_hash_vector<int, vector<pair<int, int> > >  m_cid2Cids;			//出版物聚类扩展用
		//static_hash_vector<int, vector<string> > m_pid2Sub;						//出版物标题精确匹配用
		//static_hash_vector<string, vector<pair<u64, int> > > m_brdkey2Cate;		//品牌词对应的分类反馈
		map<u64, vector<pair<int, int> > > m_price_info_map;					//价格打散配置信息
		
		WeighterManager m_custom_weighter;	//获取商业因素总分
		bool ifHavComMod;	//是否有可以直接获取商业因素得分的模块
        //whj
		ddip_t* m_ip_location;	//ip对应城市查询函数的指针
		hash_map<string,int> m_city_blocation;	//城市对应的位位置
		static_hash_vector<string, vector<pair<int, int> > > m_key_pid_weight_hash;
		//个性化男女数据使用
		static_hash_map<u64,pair<char,char> >  m_permid_sex_p_hash;
		static_hash_map<int,pair<char,char> > m_prod_sex_p_hash;
		static_hash_map<string,int> m_query_weight_hash;
		hash_map<string,SGenderCtrl> m_query_gender_ctrl_hash;

		//caiting add 2013-10-22 
        static_hash_map<int, double> m_pid2value;   //for lr model to compute w*x+b
		vector<double> LR_weights; //lr's weights
		LRModel lr_model;	
		//vector<double> price_interval; //log(price+1) interval
public:
		bool InitPersonalityLocationStockDict();	//初始化个性化地域库存字典
		bool LoadIp2LocationDict(const string& ip_file);
		bool LoadCity2BitLocationDict(const string& city_file);
		bool GetUserIp2Location(CDDAnalysisData* pa);	//获取地域信息
		bool JudgeLocationStock(int doc_id, int bit_city_location, int& out) const;
		//模板选择
		SMainCate JudgeResultMainCate(vector<SResult>& vRes, int end);
		bool SelectShowTemplate(CDDAnalysisData* pa,vector<SResult>& vRes, int end);
		void FindAndReplaceShowTempalte(string& str_reserve,string str_template);
		void GetShowTemplate(vector<SResult>& vRes, CDDAnalysisData* pa);
		//非默认排序过滤使用
		bool ComputeMainCate(CDDAnalysisData* pda);
		void NoDefoutSortFilter(IAnalysisData* pa,vector<int>& vDocIds);
		//加载ctr数据
		bool GetCtrPidHash(CDDAnalysisData* pa, string& query);
		bool ComputeCtrWeight(CDDAnalysisData* pa,int doc_id,int& ctr_weight);
		//男女个性化
		bool InitPersonalitySexDict();
		bool LoadPermId2CustomerIdDict(const string& permid_file);
		bool LoadProd2SexDict(const string& prod_file);
		bool LoadPeronalitySexControlDict(const string& ctrl_file);
		bool GetPersonalityData(CDDAnalysisData* pa);
		bool JudgeGoodsSexPartial(CDDAnalysisData* pa, vector<SResult>& vRes, int to);
		bool FindSexGoodsInResult(CDDAnalysisData* pa, vector<SResult>& vRes,
				int goods_num, vector<SResult>& sex_vect);
		//lxq
		bool JudgeTotalMatchForPub(const SMatchElement& me,int fidlmt, int firstPos, int lastPos);
		int JudgeDisForPub(const CDDAnalysisData* pa, const SMatchElement& me, int fidlmt, int& sta, int& end);


};
#endif	
