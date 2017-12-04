/***********************************************************************
* Author:      ruanyanfang
* Date:        2017-03-20
* Description: ����CRedis��
***********************************************************************/
#ifndef __REDIS_H__20170320__
#define __REDIS_H__20170320__

#include <hiredis/hiredis.h>
#include <Common/Common.h>
#include "TypeDef.h"
#include "RedisReply.h"
using namespace std;

class CRedis : public ITimerEvent
{
public:
	CRedis();
	~CRedis();

public:
	// ITimerEvent
	virtual void  OnTimer(CTimer *pTimer);

	Bool          Initialize(SRedisInfo& redisInfo, CEventEngine* pEngine);
	Int32         InitConnections();
	void          CloseConnections();

public:
	///////////////////////string ��ز���/////////////////////////////////////////
	Bool		  Set(const SString& data);
	Bool		  Get(SString& data);

	///////////////////////hash ��ز���/////////////////////////////////////////
	Bool		  Set(const SHash& data);
	Bool		  GetAll(SHash& data);
	Bool		  GetAllField(const string& strKey, list<string>& lstFields);
	Bool		  Get(const string& strHashKey, const list<string>& lstField, list<string>& lstValue);
	
	///////////////////////list ��ز���/////////////////////////////////////////
	Bool		  Set(const string& strListKey, const Int32 nIndex, const string& strValue);
	Bool		  GetAll(SList& data);
	Bool		  GetByRange(SList& data, const Int32 nStart, const Int32 nEnd);
	string		  GetByIndex(const string& strKey, Int32 nIndex);
	string		  Front(const string& strKey);
	string		  Back(const string& strKey);
	Bool		  PushBack(const SList& data);
	Bool		  PushFront(const SList& data);
	Bool          PopBack(const string& strKey);
	Bool          PopFront(const string& strKey);

	///////////////////////set ��ز���/////////////////////////////////////////
	Bool		  Set(const SSet& data);
	Bool		  GetAll(SSet& data);
	Bool          Del(const SSet& data);
	Bool          Move(const string& strSrcKey, const string& strDetKey, const string& strValue);

	///////////////////////key ��ز���/////////////////////////////////////////
	Bool		  IsExist(const string& strKey);
	Bool		  Del(const string& strKey);
	Bool		  Del(const list<string>& lstKeys);
	KeyType		  Type(const string& strKey);
	Bool		  SetExpire(const string& strKey, Int64 nSeconds);
	Bool		  SetExpireByUnixtime(const string& strKey, Int64 nMilliseconds);
	Int64		  TimeToExpire(const string& strKey);
	list<string>  KeysWithPattern(const string& strPattern);

	///////////////////////ͨ�ò���/////////////////////////////////////////////
	// ��ȡset��list�ĳ��ȣ���ȡhash��field����
	Int32         Length(const string& strKey);
	// hash\set��key��Ӧ��field\value�Ƿ����
	Bool          IsExist(const string& strKey, const string& strData);
	// ɾ��hash\set��key��Ӧ��field\value
	Bool          Del(const string& strKey, const string& strData);

private:
	// ִ����װ�õ�����õ�ִ�н��
	void                Excute(const string& strCmd, CRedisReply& result, ERWFlag flag);
	void                ExcuteRead(redisContext* pContext, int argc, const char **argv, const string& strCmd, CRedisReply& result);
	void                ExcuteWrite(redisContext* pContext, int argc, const char **argv, const string& strCmd, CRedisReply& result);

	// ��m_lstRedisConnect�л�ȡһ�����õ�����
	redisContext*       GetConnection();
	// ����ʹ�����Ż�m_lstRedisConnect��
	void                ReleaseConnection(redisContext* pContext);

	// ��redisReply����ΪCRedisReply��READ
	void                PraseReply2Result4Read(redisReply* pReply, CRedisReply& result, const string& strCmd);
	// ��redisReply����ΪCRedisReply��WRITE
	void                PraseReply2Result4Write(redisReply* pReply, CRedisReply& result, const string& strCmd);
	// ��redisReply����Ϊmap
	map<string, string> PraseReplyHash(redisReply* pReply);
	// ��redisReply����Ϊlist
	//list<string>        PraseReplyList(redisReply* pReply);

	// ��װExcute�������
	void                GenerateParam(const string& strCmd, Int32 nArgc, const char **argv);

private:
	
	list<redisContext*>       m_lstRedisConnect;
	CMutex                    m_mtxRedisConnectList;

	CSocketAddr               m_addrRedis;
	Int32                     m_nMaxConnSum;
	struct  timeval           m_tv;

	CTimer*                   m_pTimer;
	CEventEngine*             m_pEngine;
};

#endif