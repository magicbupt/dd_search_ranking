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
//������ֵ
const float MainCateThd = 0.35;


const int SCATTER_UPPER_LIMIT = 500;
const int FISRT_CATE_LIMIT    = 3;
const int SCATTER_CAT_LIMIT   = 6;

const int PUB_CATE = 1; //����������id by TranseClsPath2ID("01",2);
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
	//����������
	QueryAnalysisPart():brand_id(-1), ifHasFeedback(false), termChara(0), sugFBCate(-1),
			cloth_cat_id(0), c3_cat_id(0), other_cat_id(0), pub_cat_id(0), havMulBrd(false),
	        ifLimitSearch(false), ifAllMallFB(true), minLevel(0), sumLevel(0)
	{
	}
	vector<int> type;						//����(0-һ�� !=0-����ʣ����������ߵ�)
	vector<int> dis;						//ÿ��TERM��Ӧ�ֶε�ƫ��
	vector<int> termId;						//�洢query�����Ľ��������
	vector<int> mFieldId;					//����ٻ������ֶε�id
	vector<int> pFieldId;					//����ͼ�������ֶε�id
	u64 cloth_cat_id;						//��װ���id��������ʾ����ȡ��ҵ���ص÷���
	u64 c3_cat_id;							//3c���id��������ʾ����ȡ��ҵ���ص÷���
	u64 other_cat_id;						//�����ٻ����id��������ʾ����ȡ��ҵ���ص÷���
	u64 pub_cat_id;							//ͼ�����id��������ʾ����ȡ��ҵ���ص÷���
	int sugFBCate;							//���鷴�������0Ϊͼ�飬1Ϊ�ٻ� 2Ϊ3C 3Ϊ�Ƿ�װ��ٻ�
	string queryStr;						//ȥ�����Ч������
	bool needJudgeDis;						//��Ҫ����ʾ�
	bool havMulBrd;

	//query�������
	string pdtWord;							//��Ʒ���Ĵ�
	bool ifBrdQuery;						//Ʒ������
	bool ifPdtQuery;
	bool ifAllMallFB;						//�Ƿ�ȫ��Ϊ�ٻ�������
	int brand_id;							//Ʒ�ƴʶ�Ӧ��id new add
	int termChara;							//�ж�������ѯ���Ĵ���

	//��������
	vector<pair<u64, int> > vCate;			//���෴��
	
	//������ר���ֶ�
	bool ifHasFeedback;           			//�Ƿ�Ϊ�з���query
	bool ifLimitSearch;                		//�߼�����
	int ifAutPubSearch;               		//���߳���������
	vector<set<u64> > vCluster;             //������Ϣ
	map<u64, vector<u64> > highRelSaleCate;	//�����������������洢
	size_t tDocCnt;                        	//����������ĵ���ͳ��
	size_t tDocCnt_stock;					//����������ĵ����п����ͳ��

	size_t minLevel;						//ͳ�ƽ������ƥ��ʵĵȼ�����Сֵ
	size_t sumLevel;						//ͳ�ƽ����������Ʒ��ƥ��ʵȼ����ܺ�
};

struct FieldToId
{
	char* field;
	int id;
};

inline bool isMall(unsigned long long cls_id)
{
    //�ٻ���58���࿪ʼ
    return (*(unsigned char*)&cls_id == 0x58) || (*(unsigned char*)&cls_id == 0x93);
}

/*
�ֻ�ͨѶ��58.80.00.00.00.00
����Ӱ����58.59.00.00.00.00
���԰칫��58.63.00.00.00.00
��ҵ磺  58.82.00.00.00.00
���õ�����58.01.00.00.00.00
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
��װ��58.64.00.00.00.00
Ь��58.65.00.00.00.00
���Ƥ�ߣ�58.74.00.00.00.00
ͯװͯЬ��58.75.00.00.00.00
��Ӥ���Σ�58.76.00.00.00.00
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
		//˵����{ͼ����������װ/Ьѥ������3C����������Ʒ������, ȫվ����}
		enum {PUB_SEARCH, CLOTH_SEARCH, C3_SEARCH, OTHER_SEARCH, FULL_SITE_SEARCH, MALL_SEARCH};	//�������䶯˳����Ҫ������������͵�˳��һ��
		CDDAnalysisData(){m_searchType = 0;}

		int m_searchType;					//��ѯ���� 
		int m_otherSortField;				//��ѯ�Ƿ������������ֶ�
		QueryAnalysisPart m_AnalysisPart;

		//=========================whj=========================//
        int m_bit_city_location;
		int m_usr_sort_type;                //����ʽ
		set<int> m_main_cate_set;            //���������������
		CBitMap m_ctr_bit_map;              //ctr Ȩ������
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
		 *��ʼ��������Ҫ����ģ������
		 *Parameter psdi �������ݴ���
		 *Parameter strConf ģ����Ե������ļ���ַ
		 *return true��ʼ���ɹ���false��ʼ��ʧ�� 
		 */
		virtual bool Init(SSearchDataInfo* psdi, const string& strConf);

		bool InitCommon(SSearchDataInfo* psdi, const string& strConf);
		bool InitData();


		/*
		 * Method:    query����
		 * Returns:   void
		 * Parameter: SQueryClause & qc �ṹ���Ĳ�ѯ���
		 * Parameter: CDDAnalysisData& pa IAnalysisData ������ 
		 * return: IAnalysisData*  ���ص�query�������� �û���̬���ɣ���ܸ������٣�����ʧ���践��NULL
		 */
		virtual IAnalysisData* QueryAnalyse(SQueryClause& qc);


		/*
		 * Method:    �����ı�����Ȩ��
		 * Returns:   void
		 * Parameter: IAnalysisData * pa ���ص�query��������
		 * Parameter: SMatchElement & meÿ���ĵ���ƥ����Ϣ
		 * Parameter: SResult & rt ��ֽ��
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
								pa->m_AnalysisPart.brand_id = -2;//�����ж����ͬƷ�ƴ�
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
		//�����ﵥ���ʼ���Ȩ��
		void RankingSinglePub(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		//���������ʼ���Ȩ��
		void RankingMultiPub(CDDAnalysisData* pa, SMatchElement& me, SResult& rt);
		bool LoadFile();
		//vector<string> GetStrings(const string& stringList, const string& splitWord);

		void QueryRewrite(hash_map<string,string>& hmParams);
		/*
		 * ���ڻ��ŵĸ�������
		 */
		float GetAlpha(const string& query, const int cat);
		void ReComputeWeightFullSite(vector<SResult>& vRes, float alpha,int &bhcount);


		/*
		 * Method:    �����������δ�ֵȲ����������ڷ�Ĭ������
		 * Returns:   void
		 * Parameter: IAnalysisData* Pad, ���ص�query��������
		 * Parameter: vector<SResult> & vRes ������ֽ��
		 */
		virtual void ReRanking(vector<SResult>& vRes, IAnalysisData* pad);

		/*
		 * Method:    ����Ĭ�����򣨼���������������������۸񣩣���������ԵĽ���������ſɷ�����
		 * Returns:   void
		 * Parameter: vector<SResult> & vRes ������ֽ��
		 * Parameter: IAnalysisData* Pad, ���ص�query��������
		 * Parameter: from �û���ȡ���ĵ��� vRes�е���ʼλ��
		 * Parameter: to �û���ȡ���ĵ��� vRes�е���ֹλ�ã�������
		 * from to Ϊ�û�ȡ�ڼ�ҳ��ÿҳ���ٸ�����һ�ֱ�����ʽ����ȡ����ҳ��ÿҳ50�� ��from=200,to=250
		 */
		virtual void SortForDefault(vector<SResult>& vRes, int from, int to, IAnalysisData* pa);

		void SortFullSite(vector<SResult>& vRes, int from, int to, CDDAnalysisData* pa);
		void SortPub(vector<SResult>& vRes, int from, int to, CDDAnalysisData* pa);
		void SortMall(vector<SResult>& vRes, int from, int to, CDDAnalysisData* pa);

		//ǿ�Ƶ�Ʒ����
		bool RemoveGoodsByForceAndFB(const string& query,vector<SResult>& vRes,vector<SResult>& force_vect);
		bool ForceMoveGoods(vector<SResult>& vRes,vector<SResult>& force_vect);
		/*
		 * Method:    ShowResult �Զ���չ����Ϣ<result><cost></cost>*****�û��Զ����������******</result>"
		 * Returns:   void
		 * Parameter: IAnalysisData* Pad,    ���ص�query��������
		 * Parameter: CControl & ctrl        ��ѯ���Ʋ���������ҳ���������
		 * Parameter: GROUP_RESULT & vGpRes  �������ͳ����Ϣ��
		 * Parameter: vector<SResult> & vRes ������ֽ��
		 * Parameter: vector<string> & vecRed ����ַ�������
		 * Parameter: string & strRes         ����ַ��
		 */
		virtual void ShowResult(IAnalysisData* pad, CControl& ctrl, GROUP_RESULT &vGpRes,
				vector<SResult>& vRes, vector<string>& vecRed, string& strRes);

		virtual void SetGroupByAndFilterSequence(IAnalysisData* pad, vector<SFGNode>& vec);
		
		virtual void CustomFilt(IAnalysisData* pad, vector<int>& vDocIds, SFGNode& fgNode);

		 virtual void CustomGroupBy(IAnalysisData* pad, vector<int>& vDocIds, SFGNode& gfNode,
				                             pair<int, vector<SGroupByInfo> >& prGpInfo);
		/*
		 *Method:  ���ͳ����Ϣ�����ڲ���IDͳ�Ƶ���ת��Ϊ���� ����CLASS PATH--> CLASS NAME
		 *Parameter:vGpInfo �������� �ֶ�ID��Ӧ-->ͳ����Ϣ
		 */
		virtual void FillGroupByData(GROUP_RESULT&  vGpInfo);

	private:
		/** @function
		 *********************************
		 <PRE>
		 ��������JudgePidHasQueryTag
		 ����    ���ж��ĵ��Ƿ��з���
		 ����    ��
		 * @param  [in] pID      �ĵ���Ӧ�Ĳ�ƷID
		 * @param  [in] queryValidString �ؼ�����Ч����
		 * ����ֵ        ����Ȩ��[0,9]
		 </PRE>
		 **********************************/
		inline void JudgePidHasQueryTag(
				const int pid, const CDDAnalysisData *analysisDat,
				bool& fbCateScr, int& fbPidScr);

		/** @function
		 *********************************
		 <PRE>
		 ��������JudgeDis
		 ����    ���жϹؼ����ڸ��ĵ����Ƿ�ʾ�ƥ��
		 ����    ��
		 * @param  [in] keywords �ؼ�����Ϣ
		 * @param  [in] fidlmt   ���޶�(-1Ϊȫ��)
		 </PRE>
		 **********************************/
		/*inline bool JudgeDis(
				const CDDAnalysisData *pa,
				const SMatchElement& me,
				int fidlmt = -1);*/
		/** @function
		 *********************************
		 <PRE>
		 ��������JudgeDisFilter
		 ����    ���ʾ������
		 ����    ��
		 * @param  [in] vKeywords        �ؼ�����Ϣ
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
		 ��������JudgeTotalMatch
		 ����    ���жϹؼ����Ƿ�����⾫ȷƥ��
		 ����    ��
		 * @param  [in] objQuery �ؼ�����Ϣ
		 * @param  [in] objDoc   �ĵ���Ϣ
		 * @param  [in] fid      �ֶ�ID
		 * @param  [in] pos_title        �ؼ����ڱ����״γ���λ��
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
		 ��������RecordCate
		 ����    ����¼�ĵ����������Ϣ
		 ����    ��
		 * @param  [out] vCluster        ������Ϣ�洢
		 * @param  [in] pid      �ĵ���Ʒid
		 * @param  [in] level    �ĵ����ȼ�
		 * @param  [in] cateScore        ���Ȩ��
		 * @param  [in] match    �ĵ����������
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
		set<string> m_pdtWords;			//��Ʒ��

		/***********************��������**************************/
		static_hash_vector<string, vector<float> > m_percentPub2Mall;	//0-������ 1-��װ 2-3C 3-����
		static const int catenum = 4;		//���շ������������
		float alpha[catenum];				//��������Ʒռ����Ʒ���ı���,0-������ 1-��װ 2-3C 3-����

	private:
		//ö����Ҫ��field_id�е�Ԫ��һһ��Ӧ
		enum{
			TMINNUM = 0,					//������Ҫ�ֶ����Ҫ�ֶ�
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
		//nScore����Ȩ��ʱʹ��
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
		//nWeight��ʶ������Ϣʹ��
		enum{
			LEVELBIT = 4,
			ISMALLBIT = 30
		};
		enum{
			LEVELBITNUM = 4,
			ISMALLBITNUM = 1
		};
		//�ж�query����ʱʹ��,��Ҫ��termCharaToName�Լ�loadfilename.txt�е��ļ���˳�򱣳�һ��
		enum{
			BKNAME = 0,
			BRDNAME,
			AUTNAME,
			PDTWORD,
			CATNAME,
			MODELCODE,
			PUBLISHER
		};

		static const int ScoreBitMap[9];	//��ȡ�������ض�Ӧ��λ��ֵ
		static const int TermCharaToFieldNum[7];
		static const char* TermCharaToName[7];
		vector<vector<int> > TermCharaToField;
		set<int> mAllField, pAllField;
		static const FieldToId field_id[14];//�ֶε���ŵ�ӳ��
		vector<int> m_vFieldIndex;			//�ֶζ�Ӧ������
		vector<pair<string, void**> > m_vProfile;	//�洢��������ָ��
		static const char* field[20];		//�洢ĳЩ�ֶ���
		vector<int> m_fid2fi;				//�ֶ����������ӳ��
		vector<short> m_salAmtScr;			//�洢���۶�ĵ÷�
		vector<short> m_salNumScr;			//�洢�����ĵ÷�
		vector<short> m_commentScr;			//�洢�������ĵ÷�

		//pub member
		void* m_totalReviewCountProfile;	//��������ֶε�����ָ�룬�ȡÿ����Ʒ����������
		void* m_preSaleProfile;				//�Ƿ�Ԥ����Ʒ�ֶε�����ָ�룬��ȡÿ����Ʒ�Ƿ���Ԥ����Ʒ
		void* m_numImagesProfile;			//ͼƬ�ֶε�����ָ�룬��ȡÿ����Ʒ��ͼƬ
		void* m_salePriceProfile;			//���������ֶε�����ָ�룬��ȡÿ����Ʒ�����ۼ۸�
		void* m_pubDateProfile;				//����ʱ���ֶε�����ָ�룬��ȡÿ��������ĳ���ʱ��
		//common member
		void* m_clsProfile;					//�����ֶε�����ָ�룬���ڻ�ȡÿ����Ʒ��Ӧ�ķ���
		void* m_stockProfile;				//����ֶε�����ָ�룬��ȡÿ����Ʒ�Ŀ��
		void* m_stockStatusProfile;         //���п����Ϣu64 whj========================
		void* m_product_sex_profile;        //personality gender data  whj
		void* m_saleDayProfile;				//�������ֶε�����ָ�룬��ȡÿ����Ʒһ�ܵ�����
		void* m_saleWeekProfile;			//�������ֶε�����ָ�룬��ȡÿ����Ʒһ�ܵ�����
		void* m_saleMonthProfile;			//�������ֶε�����ָ�룬��ȡÿ����Ʒһ�ܵ�����
		void* m_saleDayAmtProfile;			//�����۶��ֶε�����ָ�룬��ȡÿ����Ʒ��������۶�
		void* m_saleWeekAmtProfile;			//�����۶��ֶε�����ָ�룬��ȡÿ����Ʒһ�ܵ����۶�
		void* m_saleMonthAmtProfile;		//�����۶��ֶε�����ָ�룬��ȡÿ����Ʒһ�µ����۶�

		void* m_promoPriceProfile;
		void* m_promoStartDateProfile;
		void* m_promoEndDateProfile;
		void* m_scoreProfile;
		void* m_inputDateProfile;			//�ϼ�ʱ���ֶε�����ָ�룬��ȡÿ����Ʒ���ϼ�ʱ��
		void* m_modifyTime;					//�����ϼ�ʱ���ֶε�����ָ�룬��ȡÿ����Ʒ�������ϼ�ʱ��
		void* m_isShareProductProfile;		//�Ƿ���Ʒ�ֶε�����ָ�룬��ȡÿ����Ʒ�Ƿ��ǹ���Ʒ
		void* m_isPidProfile;				//��ƷID�ֶε�����ָ�룬��ȡÿ����Ʒ����ƷID
		void* m_isBrdidProfile;				//��ƷƷ��ID�ֶε�����ָ�룬��ȡÿ����Ʒ��Ʒ��ID new add
		void* m_isPublicationProfile;
		void* m_proMedProfile;

		static_hash_map<string, int> m_dict[7];
		static_hash_vector<string, vector<pair<u64, int> > > m_key2Cate; 		//���෴��
		static_hash_vector<string, vector<pair<int, int> > > m_key2Pid;			//��Ʒ����
		static_hash_vector<int, vector<string> > m_pid2Core;					//��Ʒ��Ӧ�����Ĵ�
		//static_hash_vector<int, vector<pair<int, int> > >  m_cid2Cids;			//�����������չ��
		//static_hash_vector<int, vector<string> > m_pid2Sub;						//��������⾫ȷƥ����
		//static_hash_vector<string, vector<pair<u64, int> > > m_brdkey2Cate;		//Ʒ�ƴʶ�Ӧ�ķ��෴��
		map<u64, vector<pair<int, int> > > m_price_info_map;					//�۸��ɢ������Ϣ
		
		WeighterManager m_custom_weighter;	//��ȡ��ҵ�����ܷ�
		bool ifHavComMod;	//�Ƿ��п���ֱ�ӻ�ȡ��ҵ���ص÷ֵ�ģ��
        //whj
		ddip_t* m_ip_location;	//ip��Ӧ���в�ѯ������ָ��
		hash_map<string,int> m_city_blocation;	//���ж�Ӧ��λλ��
		static_hash_vector<string, vector<pair<int, int> > > m_key_pid_weight_hash;
		//���Ի���Ů����ʹ��
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
		bool InitPersonalityLocationStockDict();	//��ʼ�����Ի��������ֵ�
		bool LoadIp2LocationDict(const string& ip_file);
		bool LoadCity2BitLocationDict(const string& city_file);
		bool GetUserIp2Location(CDDAnalysisData* pa);	//��ȡ������Ϣ
		bool JudgeLocationStock(int doc_id, int bit_city_location, int& out) const;
		//ģ��ѡ��
		SMainCate JudgeResultMainCate(vector<SResult>& vRes, int end);
		bool SelectShowTemplate(CDDAnalysisData* pa,vector<SResult>& vRes, int end);
		void FindAndReplaceShowTempalte(string& str_reserve,string str_template);
		void GetShowTemplate(vector<SResult>& vRes, CDDAnalysisData* pa);
		//��Ĭ���������ʹ��
		bool ComputeMainCate(CDDAnalysisData* pda);
		void NoDefoutSortFilter(IAnalysisData* pa,vector<int>& vDocIds);
		//����ctr����
		bool GetCtrPidHash(CDDAnalysisData* pa, string& query);
		bool ComputeCtrWeight(CDDAnalysisData* pa,int doc_id,int& ctr_weight);
		//��Ů���Ի�
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
