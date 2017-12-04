/***********************************************************************
 * Author:      ruanyanfang
 * Date:        2017-03-20
 * Description: ʵ��redis.h�ж���ĺ���
***********************************************************************/
#include <stdio.h>
#include <time.h>
#include <sstream>
#include "../Include/Redis.h"

CRedis::CRedis()
{

}

CRedis::~CRedis()
{
	CloseConnections();

	m_pTimer->KillTimer();
	delete m_pTimer;
	m_pTimer = NULL;
}

void CRedis::OnTimer(CTimer * pTimer)
{
	m_mtxRedisConnectList.Lock();

	list<redisContext*>::iterator it = m_lstRedisConnect.begin();
	while (it != m_lstRedisConnect.end())
	{
		redisContext* pTmp = *it;
		if (pTmp->err)
		{
			redisFree(pTmp);
			m_lstRedisConnect.erase(it++);
		}
		else
		{
			++it;
		}
	}

	Int32 nSize = m_nMaxConnSum - m_lstRedisConnect.size();
	for (Int32 i = 0; i < nSize; ++i)
	{
		redisContext* pTmp = redisConnectWithTimeout(m_addrRedis.GetIPString().c_str(), m_addrRedis.GetPort(), m_tv);
		if (pTmp != NULL)
		{
			m_lstRedisConnect.push_back(pTmp);
		}
	}

	m_mtxRedisConnectList.Unlock();
}

Bool CRedis::Initialize(SRedisInfo& redisInfo, CEventEngine* pEngine)
{
	m_pEngine = pEngine;
	m_pTimer = new CTimer(pEngine, this);

	m_addrRedis = redisInfo.redisAddr;
	m_nMaxConnSum = redisInfo.nConnectNum;
	m_tv.tv_sec = redisInfo.tv.tv_sec;
	m_tv.tv_usec = redisInfo.tv.tv_usec;

	if (InitConnections() != m_nMaxConnSum)
	{
		return False;
	}

	if (!m_pTimer->SetTimer(500))// 
	{
		return False;
	}

	return True;
}

Int32 CRedis::InitConnections()
{	
	do
	{
		for (Int32 i = 0; i < m_nMaxConnSum; i++)
		{
			redisContext* pTmpContext = redisConnectWithTimeout(m_addrRedis.GetIPString().c_str(), m_addrRedis.GetPort(), m_tv);
			if (pTmpContext->err)
			{
				//ERROR_LOG("InitConnections(): connect to redis server--" << m_addrRedis.GetAddrString().c_str() << " failed, err is: " << pTmpContext->errstr);
				
				if (pTmpContext)
				{
					redisFree(pTmpContext);
					pTmpContext = NULL;
				}
				continue;
			}
			else
			{
				//INFO_LOG("InitConnections(): connected to redis server, fd is: " << pTmpContext->fd);

				m_mtxRedisConnectList.Lock();
				m_lstRedisConnect.push_back(pTmpContext);
				m_mtxRedisConnectList.Unlock();
			}
		}
	} while (0);

	return m_lstRedisConnect.size();
}

void CRedis::CloseConnections()
{
	list<redisContext*>::iterator it = m_lstRedisConnect.begin();
	m_mtxRedisConnectList.Lock();

	for (; it != m_lstRedisConnect.end(); it++)
	{
		redisContext* pTmp = *it;
		if (pTmp)
		{
			//INFO_LOG("CloseConnections(): redis connect to be freed, fd is: " << pTmp->fd);

			redisFree(pTmp);
			pTmp = NULL;
		}
	}
	m_lstRedisConnect.clear();

	m_mtxRedisConnectList.Unlock();
}

/*************************************************
Function:       CRedis::Set()
Description:    string��set����
Command:		SET key value
Calls:          
Table Accessed: 
Table Updated: 
Input:          SString�ṹ�壬����key��value
Output:         �����ò����ɹ����ʱ���ŷ���True
Return:         
Others:         
*************************************************/
Bool CRedis::Set(const SString & data)
{
	string strCmd = "SET " + data.strKey + " " + data.strValue;
	CRedisReply cResult;
	Excute(strCmd, cResult, WRITE);

	return (cResult.GetReplyType() == REPLY_ERROR) ? False : True;
}

/*************************************************
Function:       CRedis::Set()
Description:    list��set����
Command:		LSET key index value
Calls:
Table Accessed:
Table Updated:
Input:
Output:         ����������Χ��Կ��б���в���ʱ����False
				���óɹ�����True
Return:
Others:
*************************************************/
Bool CRedis::Set(const string& strListKey, const Int32 nIndex, const string& strValue)
{
	CRedisReply cResult;
	string strCmd = "LSET " + strListKey + " " + STR::NumberToString(nIndex) + " " + strValue;
	Excute(strCmd, cResult, WRITE);

	return (cResult.GetReplyType() == REPLY_ERROR) ? False : True;
}

/*************************************************
Function:       CRedis::Get()
Description:    list��get����
Command:		LRANGE key 0 -1
Calls:
Table Accessed:
Table Updated:
Input:			ֻ��SList��strKey��ֵ
Output:         keyΪlistʱ����True�����򷵻�False
				key������ʱlstDataΪ��
Return:
Others:
*************************************************/
Bool CRedis::GetAll(SList & data)
{
	return GetByRange(data, 0, -1);
}

/*************************************************
Function:       CRedis::Get()
Description:    list��get����
Command:		LRANGE key start end
Calls:
Table Accessed:
Table Updated:
Input:			ֻ��SList��strKey��ֵ
Output:         keyΪlistʱ����True�����򷵻�False
				key������ʱlstDataΪ��
				nStart��nEnd�Ƿ�ʱlstDataΪ��
Return:
Others:
*************************************************/
Bool CRedis::GetByRange(SList & data, const Int32 nStart, const Int32 nEnd)
{
	Bool bSuccess = False;
	CRedisReply cResult;
	data.lstData.clear();

	do
	{
		string strCmd = "LRANGE " + data.strKey + " " + STR::NumberToString(nStart) + " " + STR::NumberToString(nEnd);
		Excute(strCmd, cResult, READ);

		if (cResult.GetReplyType() == REPLY_ERROR)
		{
			break;
		}
		else
		{
			data.lstData = cResult.GetReplyList();
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Get()
Description:    set��get����
Command:		SMEMBERS key
Calls:
Table Accessed:
Table Updated:
Input:			ֻ��SSet��strKey��ֵ
Output:         keyΪsetʱ����True�����򷵻�False
				key������ʱvecDataΪ��
Return:
Others:
*************************************************/
Bool CRedis::GetAll(SSet& data)
{
	Bool bSuccess = False;
	CRedisReply cResult;
	data.vecData.clear();
	
	do
	{
		string strCmd = "SMEMBERS " + data.strKey;
		
		Excute(strCmd, cResult, READ);

		if (cResult.GetReplyType() == REPLY_ERROR)
		{
			break;
		}
		else
		{
			/*list<string> lstTmp;
			lstTmp = cResult.GetReplyList();
			list<string>::const_iterator it = lstTmp.begin();
			for (; it != lstTmp.end(); it++)
			{
				data.vecData.push_back(*it);
			}*/
			list<string>::const_iterator it = cResult.GetReplyList().begin();
			for (; it != cResult.GetReplyList().end(); it++)
			{
				data.vecData.push_back(*it);
			}
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Del()
Description:    set��ɾ�����value����
Command:		SREM key value1...
Calls:
Table Accessed:
Table Updated:
Input:			
Output:         key����set���ͣ�����False
				��ɾ����Ԫ�ض�������ʱ������False
				��ɾ��Ԫ�ز��ִ����Ҳ����ɹ�ʱ������True��������Ԫ�ػᱻ����
Return:
Others:
*************************************************/
Bool CRedis::Del(const SSet & data)
{
	Bool bSuccess = False;
	CRedisReply cResult;
	string strCmd, strTmp;

	do 
	{
		UInt32 i = 0;
		vector<string>::const_iterator it = data.vecData.begin();
		for (; it != data.vecData.end(); it++, i++)
		{
			if (i < data.vecData.size() - 1)
			{
				strTmp += (*it) + " ";
			}
			else
			{
				strTmp += (*it);
			}
		}
		strCmd = "SREM " + data.strKey + " " + strTmp;

		Excute(strCmd, cResult, WRITE);

		if (cResult.GetReplyType() == REPLY_ERROR || cResult.GetReplyInteger() == 0)
		{
			break;
		}
		else
		{
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Move()
Description:    ��srcKey�е�valueԪ���Ƶ�detKey��
Command:		SMOVE srcKey detKey value
Calls:
Table Accessed:
Table Updated:
Input:
Output:         srcKey��detKey����set���ͣ�����False
				value��srcKey�в����ڣ�����False
				value��srcKey��detKey�д��ڣ�����True
Return:
Others:
*************************************************/
Bool CRedis::Move(const string & strSrcKey, const string & strDetKey, const string & strValue)
{
	Bool bSuccess = False;
	CRedisReply cResult;
	string strCmd;

	do 
	{
		strCmd = "SMOVE " + strSrcKey + " " + strDetKey + " " + strValue;

		Excute(strCmd, cResult, WRITE);

		if (cResult.GetReplyType() == REPLY_ERROR || cResult.GetReplyInteger() == 0)
		{
			break;
		}
		else
		{
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::GetByIndex()
Description:    list��get����
Command:		LINDEX key index
Calls:
Table Accessed:
Table Updated:
Input:			strKey����Ϊlist���͵�key
Output:         ����ʵ��ֵ��""
Return:
Others:
*************************************************/
std::string CRedis::GetByIndex(const string& strKey, Int32 nIndex)
{
	CRedisReply cResult;
	string strCmd = "LINDEX " + strKey + " " + STR::NumberToString(nIndex);
	Excute(strCmd, cResult, READ);

	return cResult.GetReplyString();
}

/*************************************************
Function:       CRedis::GetByIndex()
Description:    list��get��������ȡlist�ĵ�1��Ԫ��
Command:		LINDEX key 0
Calls:
Table Accessed:
Table Updated:
Input:			strKey����Ϊlist���͵�key
Output:         ����ʵ��ֵ��""
Return:
Others:
*************************************************/
std::string CRedis::Front(const string& strKey)
{
	return GetByIndex(strKey, 0);
}

/*************************************************
Function:       CRedis::GetByIndex()
Description:    list��get��������ȡlist�����1��Ԫ��
Command:		LINDEX key -1
Calls:
Table Accessed:
Table Updated:
Input:			strKey����Ϊlist���͵�key
Output:         ����ʵ��ֵ��""
Return:
Others:
*************************************************/
std::string CRedis::Back(const string& strKey)
{
	return GetByIndex(strKey, -1);
}

Bool CRedis::Set(const SSet & data)
{
	CRedisReply cResult;
	string strCmd, strTmp;

	UInt32 i = 0;
	vector<string>::const_iterator it = data.vecData.begin();
	for (; it != data.vecData.end(); it++, i++)
	{
		if (i < data.vecData.size() - 1)
		{
			strTmp += *it + " ";
		}
		else
		{
			strTmp += *it;
		}
		strCmd = "SADD " + data.strKey + " " + strTmp;
	}
	
	Excute(strCmd, cResult, WRITE);

	return (cResult.GetReplyType() == REPLY_ERROR) ? False : True;
}

/*************************************************
Function:       CRedis::IsExist()
Description:    key�Ƿ����
Command:		EXISTS key 
Calls:
Table Accessed:
Table Updated:
Input:
Output:         key����ʱ����True
Return:
Others:
*************************************************/
Bool CRedis::IsExist(const string& strKey)
{
	Bool bSuccess = False;
	CRedisReply cResult;

	do 
	{
		string strCmd = "EXISTS " + strKey;
		Excute(strCmd, cResult, READ);

		if (cResult.GetReplyType() == REPLY_BOOLEN && cResult.GetReplyBool() == True)
		{
			bSuccess = True;
		}
		else
		{
			break;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Del()
Description:    ɾ��1��key
Command:		DEL key
Calls:
Table Accessed:
Table Updated:
Input:
Output:         �ɹ�ɾ��ʱ����True
				key�����ڻ�ִ��ʧ��ʱ����False
Return:
Others:
*************************************************/
Bool CRedis::Del(const string& strKey)
{
	Bool bSuccess = False;
	CRedisReply cResult;

	do 
	{
		string strCmd = "DEL " + strKey;
		Excute(strCmd, cResult, WRITE);

		if (cResult.GetReplyType() == REPLY_INTEGER && cResult.GetReplyInteger() == 1)
		{
			bSuccess = True;
		}
		else
		{
			break;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Del()
Description:    ɾ�����key
Command:		DEL key1...
Calls:
Table Accessed:
Table Updated:
Input:
Output:         ȫ���ɹ�ɾ��ʱ����True
				����key�����ڻ�ִ��ʧ��ʱ����False
Return:
Others:
*************************************************/
Bool CRedis::Del(const list<string>& lstKeys)
{
	Bool bSuccess = False;
	string strTmp, strCmd;
	CRedisReply cResult;

	do
	{
		UInt32 i = 0;
		list<string>::const_iterator it = lstKeys.begin();
		for (; it != lstKeys.end(); it++, i++)
		{
			if (i < lstKeys.size() - 1)
			{
				strTmp += (*it) + " ";
			}
			else
			{
				strTmp += (*it);
			}
		}
		strCmd = "DEL " + strTmp;
				
		Excute(strCmd, cResult, WRITE);

		if (cResult.GetReplyType() == REPLY_INTEGER && cResult.GetReplyInteger() == lstKeys.size())
		{
			bSuccess = True;
		}
		else
		{
			break;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Type()
Description:    ��ȡkey������
Command:		TYPE key
Calls:
Table Accessed:
Table Updated:
Input:
Output:         ����key������
Return:
Others:
*************************************************/
KeyType CRedis::Type(const string& strKey)
{
	KeyType res;
	CRedisReply cResult;
	string strCmd = "TYPE " + strKey;
	
	Excute(strCmd, cResult, READ);
	string strTmp = cResult.GetReplyString();

	if (strTmp == "string")
	{
		res = TYPE_STRING;
	}
	else if (strTmp == "list")
	{
		res = TYPE_LIST;
	}
	else if (strTmp == "set")
	{
		res = TYPE_SET;
	}
	else if (strTmp == "hash")
	{
		res = TYPE_HASH;
	}
	else
	{
		res = TYPE_NONE;
	}

	return res;
}

/*************************************************
Function:       CRedis::SetExpire()
Description:    ����key�Ĺ���ʱ�䣬seconds����key������
Command:		EXPIRE key seconds
Calls:
Table Accessed:
Table Updated:
Input:			
Output:         key���������óɹ�ʱ����True
				key�����ڻ�ִ��ʧ��ʱ����Flase
Return:
Others:
*************************************************/
Bool CRedis::SetExpire(const string& strKey, Int64 nSeconds)
{
	Bool bSuccess = False;
	CRedisReply cResult;

	do
	{
		string strCmd = "EXPIRE " + strKey + " " + STR::NumberToString(nSeconds);
		Excute(strCmd, cResult, WRITE);
		if (cResult.GetReplyType() == REPLY_INTEGER && cResult.GetReplyInteger() == 1)
		{
			bSuccess = True;
		}
		else
		{
			break;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::SetExpire()
Description:    ����key�Ĺ���ʱ�䣬ʱ��Ϊunixʱ���
Command:		EXPIREAT key unix_timestamp
Calls:
Table Accessed:
Table Updated:
Input:
Output:         key���������óɹ�ʱ����True
				key�����ڻ�ִ��ʧ��ʱ����Flase
Return:
Others:
*************************************************/
Bool CRedis::SetExpireByUnixtime(const string& strKey, Int64 nMilliseconds)
{
	Bool bSuccess = False;
	CRedisReply cResult;

	do
	{
		string strCmd = "EXPIREAT " + strKey + " " + STR::NumberToString(nMilliseconds);
		Excute(strCmd, cResult, WRITE);

		if (cResult.GetReplyType() == REPLY_INTEGER && cResult.GetReplyInteger() == 1)
		{
			bSuccess = True;
		}
		else
		{
			break;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::TimeToExpire()
Description:    ����key��ʣ�����ʱ�䣬��λΪ��
Command:		TTL key
Calls:
Table Accessed:
Table Updated:
Input:
Output:         key�������Ѿ����ù���ʱ�䣬����ʣ��ʱ��
				key������δ���ù���ʱ�䣬����-1
				key�����ڷ���-2
				ִ��ʧ�ܷ���-3
Return:
Others:
*************************************************/
Int64 CRedis::TimeToExpire(const string& strKey)
{
	CRedisReply cResult;
	string strCmd = "TTL " + strKey;
	
	Excute(strCmd, cResult, READ);
	if (cResult.GetReplyType() == REPLY_ERROR)
	{
		return -3;
	}
	else
	{
		return cResult.GetReplyInteger();
	}
}

std::list<std::string> CRedis::KeysWithPattern(const string& strPattern)
{
	CRedisReply cResult;
	string strCmd = "KEYS " + strPattern;
	Excute(strCmd, cResult, READ);

	return cResult.GetReplyList();
}


/*************************************************
Function:       CRedis::Length()
Description:    ��ȡlist��set�ĳ��ȣ���ȡhash��field����
Command:		LLEN key  
				SCARD key 
				HLEN key
Calls:
Table Accessed:
Table Updated:
Input:
Output:         -2����ʾִ��ʧ��
				-1����ʾkeyΪstring����֧�ֲ�ѯ����
				0�� ��ʾkey������
				key����Ϊlist��set��hash�Ҵ��ڣ����س���
Return:
Others:
*************************************************/
Int32 CRedis::Length(const string& strKey)
{
	CRedisReply cResult;
	string strCmd;
	KeyType type = Type(strKey);
	switch (type)
	{
	case TYPE_NONE:
		return 0;
		break;
	case TYPE_STRING:
		return -1;
		break;
	case TYPE_LIST:
		strCmd = "LLEN " + strKey;
		break;
	case TYPE_SET:
		strCmd = "SCARD " + strKey;
		break;
	case TYPE_HASH:
		strCmd = "HLEN " + strKey;
		break;
	default:
		return 0;
		break;
	}
	
	Excute(strCmd, cResult, READ);
	return (cResult.GetReplyType() == REPLY_ERROR) ? -2: (cResult.GetReplyInteger());
}

Bool CRedis::IsExist(const string & strKey, const string & strData)
{
	string strCmd;
	CRedisReply cResult;

	KeyType type = Type(strKey);
	switch (type)
	{
	case TYPE_NONE:
		return False;
		break;
	case TYPE_SET:
		strCmd = "SISMEMBER " + strKey + " " + strData;
		break;
	case TYPE_HASH:
		strCmd = "HEXISTS " + strKey + " " + strData;
		break;
	default:
		return False;
		break;
	}
	
	Excute(strCmd, cResult, READ);
	if (cResult.GetReplyType() == REPLY_ERROR || cResult.GetReplyBool() == False)
	{
		return False;
	}
	else
	{
		return True;
	}
}

Bool CRedis::Del(const string & strKey, const string & strData)
{
	string strCmd;

	KeyType type = Type(strKey);
	switch (type)
	{
	case TYPE_NONE:
		return False;
		break;
	case TYPE_SET:
		strCmd = "SREM " + strKey + " " + strData;
		break;
	case TYPE_HASH:
		strCmd = "HDEL " + strKey + " " + strData;
		break;
	default:
		return False;
		break;
	}

	CRedisReply cResult;
	Excute(strCmd, cResult, WRITE);
		
	if (cResult.GetReplyType() == REPLY_ERROR || cResult.GetReplyInteger() < 0)
	{
		return False;
	}
	else
	{
		return True;
	}
}

/*************************************************
Function:       CRedis::PushBack()
Description:    list�Ĳ������
Command:		RPUSH key value
				RPUSH key value1...
Calls:
Table Accessed:
Table Updated:
Input:
Output:         �����ò����ɹ����ʱ������True
Return:
Others:
*************************************************/
Bool CRedis::PushBack(const SList& data)
{
	string strCmd;
	CRedisReply cResult;

	string strTmp;
	UInt32 i = 0;
	list<string>::const_iterator it = data.lstData.begin();
	for (; it != data.lstData.end(); it++, i++)
	{
		if (i < data.lstData.size() - 1)
		{
			strTmp += *(it)+" ";
		}
		else
		{
			strTmp += *(it);
		}
	}
	strCmd = "RPUSH " + data.strKey + " " + strTmp;

	Excute(strCmd, cResult, WRITE);

	return (cResult.GetReplyType() == REPLY_ERROR) ? False : True;
}

/*************************************************
Function:       CRedis::PushFront()
Description:    list�Ĳ������
Command:		LPUSH key value
				LPUSH key value1...
Calls:
Table Accessed:
Table Updated:
Input:
Output:         �����ò����ɹ����ʱ������True
Return:
Others:
*************************************************/
Bool CRedis::PushFront(const SList& data)
{
	string strCmd;
	CRedisReply cResult;

	string strTmp;
	UInt32 i = 0;
	list<string>::const_iterator it = data.lstData.begin();
	for (; it != data.lstData.end(); it++, i++)
	{
		if (i < data.lstData.size() - 1)
		{
			strTmp += *(it)+" ";
		}
		else
		{
			strTmp += *(it);
		}
	}
	strCmd = "LPUSH " + data.strKey + " " + strTmp;
	
	Excute(strCmd, cResult, WRITE);

	return (cResult.GetReplyType() == REPLY_ERROR) ? False : True;
}

/*************************************************
Function:       CRedis::PopBack()
Description:    list���Ƴ�����
Command:		RPOP key 
Calls:
Table Accessed:
Table Updated:
Input:
Output:         ��key��Ϊlist����ʱ������False
				��key������ʱ������False
				��key����ʱ������True
Return:
Others:
*************************************************/
Bool CRedis::PopBack(const string & strKey)
{
	Bool bSuccess = False;
	string strCmd;
	CRedisReply cResult;

	do
	{
		strCmd = "RPOP " + strKey;
		Excute(strCmd, cResult, WRITE);

		if (cResult.GetReplyType() == REPLY_ERROR || cResult.GetReplyString() == "")
		{
			break;
		}
		else
		{
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::PopBack()
Description:    list���Ƴ�����
Command:		LPOP key
Calls:
Table Accessed:
Table Updated:
Input:
Output:         ��key��Ϊlist����ʱ������False
				��key������ʱ������False
				��key����ʱ������True
Return:
Others:
*************************************************/
Bool CRedis::PopFront(const string & strKey)
{
	Bool bSuccess = False;
	string strCmd;
	CRedisReply cResult;

	do
	{
		strCmd = "LPOP " + strKey;
		Excute(strCmd, cResult, WRITE);

		if (cResult.GetReplyType() == REPLY_ERROR || cResult.GetReplyString() == "")
		{
			break;
		}
		else
		{
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Set()
Description:    string��get����
Command:		GET key
Calls:
Table Accessed:
Table Updated:
Input:          
Output:         key����ʱ����value��key������ʱ����""
Return:
Others:
*************************************************/
Bool CRedis::Get(SString& data)
{
	Bool bSuccess = False;

	do 
	{
		string strCmd = "GET " + data.strKey;
		CRedisReply cResult;
		Excute(strCmd, cResult, READ);

		if (cResult.GetReplyType() == REPLY_ERROR)
		{
			break;
		}
		else
		{
			data.strValue = cResult.GetReplyString();
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Set()
Description:    hash��set����
Command:		HSET key field value
				HMSET key field1 value1...
Calls:
Table Accessed:
Table Updated:
Input:          
Output:         �����ò����ɹ����ʱ���ŷ���True
Return:
Others:
*************************************************/
Bool CRedis::Set(const SHash & data)
{
	string strCmd;
	if (data.mapData.size() == 1)
	{
		strCmd = "HSET " + data.strKey + " " + data.mapData.begin()->first + " " + data.mapData.begin()->second;
	}
	else
	{
		string strTmp;
		UInt32 i = 0;
		map<string, string>::const_iterator it = data.mapData.begin();
		for (; it != data.mapData.end(); it++, i++)
		{
			if (i < data.mapData.size() - 1)
			{
				strTmp += it->first + " " + it->second + " ";
			}
			else
			{
				strTmp += it->first + " " + it->second;
			}
		}
		strCmd = "HMSET " + data.strKey + " " + strTmp;
	}

	CRedisReply cResult;
	Excute(strCmd, cResult, WRITE);

	return (cResult.GetReplyType() == REPLY_ERROR) ? False : True;
}

/*************************************************
Function:       CRedis::Get()
Description:    hash��get����
Command:		HGETALL key
Calls:
Table Accessed:
Table Updated:
Input:			ֻ��SHash��strKey��ֵ
Output:         keyΪhashʱ����True�����򷵻�False
				key������ʱmapDataΪ��
Return:
Others:
*************************************************/
Bool CRedis::GetAll(SHash& data)
{
	Bool bSuccess = False;
	data.mapData.clear();

	do 
	{
		string strCmd = "HGETALL " + data.strKey;
		CRedisReply cResult;
		Excute(strCmd, cResult, READ);

		if (cResult.GetReplyType() == REPLY_ERROR)
		{
			break;
		}
		else
		{
			data.mapData = cResult.GetReplyHash();
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Get()
Description:    hash��get����
Command:		HKEYS key
Calls:
Table Accessed:
Table Updated:
Input:			
Output:         keyΪhashʱ����True�����򷵻�False
				key������ʱlstFieldsΪ��
Return:
Others:
*************************************************/
Bool CRedis::GetAllField(const string & strKey, list<string>& lstFields)
{
	Bool bSuccess = False;
	lstFields.clear();

	do 
	{
		string strCmd = "HKEYS " + strKey;
		CRedisReply cResult;
		Excute(strCmd, cResult, READ);

		if (cResult.GetReplyType() == REPLY_ERROR)
		{
			break;
		}
		else
		{
			lstFields = cResult.GetReplyList();
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

/*************************************************
Function:       CRedis::Get()
Description:    hash��get����
Command:		HMGET key field1...
Calls:
Table Accessed:
Table Updated:
Input:
Output:         key��Ϊhashʱ����False
				key������ʱlstValue�е�Ԫ��ȫΪ""
				ָ��field������ʱ��ӦvalueΪ""
				lstValue��lstFieldһһ��Ӧ
Return:
Others:
*************************************************/
Bool CRedis::Get(const string & strHashKey, const list<string>& lstField, list<string>& lstValue)
{
	Bool bSuccess = False;
	lstValue.clear();
	                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
	do 
	{
		string strTmp, strCmd;
		CRedisReply cResult;

		list<string>::const_iterator it = lstField.begin();
		UInt32 i = 0;
		for (; it != lstField.end(); it++, i++)
		{
			if (i < lstField.size() - 1)
			{
				strTmp += *it + " ";
			}
			else
			{
				strTmp += *it;
			}
		}
		strCmd = "HMGET " + strHashKey + " " + strTmp;

		Excute(strCmd, cResult, READ);

		if (cResult.GetReplyType() == REPLY_ERROR)
		{
			break;
		}
		else
		{
			lstValue = cResult.GetReplyList();
			bSuccess = True;
		}
	} while (0);

	return bSuccess;
}

void CRedis::Excute(const string& strCmd, CRedisReply & result, ERWFlag flag)
{
//	GenerateParam(strCmd, nArgc, argv);
	list<string> lstCmd;
	STR::Explode(strCmd.c_str(), " ", lstCmd);
	Int32 nArgc = lstCmd.size();

	assert(!lstCmd.empty());
	const char **argv = new const char*[nArgc];
	Int32 nIndex = 0;
	for (list<string>::const_iterator it = lstCmd.begin(); it != lstCmd.end(); it++)
	{
		argv[nIndex++] = (*it).c_str();
	}
	
	do 
	{
		redisContext* pContext = GetConnection();
		//INFO_LOG("Excute(): get connection success, pContext is: " << pContext);

		if (pContext != NULL)
		{
			if (flag == READ)
			{
				ExcuteRead(pContext, nArgc, argv, strCmd, result);
				delete[] argv;
				argv = NULL;
			}
			else if (flag == WRITE)
			{
				ExcuteWrite(pContext, nArgc, argv, strCmd, result);
				delete[] argv;
				argv = NULL;
			}
		}
		else
		{
			result.Clear();
			result.SetReplyErr("All the redis connections unavaliable!");
			result.SetReplyType(REPLY_ERROR);

			ReleaseConnection(pContext);

			delete[] argv;
			argv = NULL;

			//ERROR_LOG("Execute(), all the redis connect unavaliable, cmd is: " << strCmd);
			return;
		}
	} while (0);
}

void CRedis::ExcuteRead(redisContext* pContext, int argc, const char **argv, const string& strCmd, CRedisReply& result)
{
	redisReply* pReply = (redisReply*)redisCommandArgv(pContext, argc, argv, NULL);
	
	do 
	{
		if (pReply != NULL)
		{
			//INFO_LOG("ExcuteRead(): cmd is: " << strCmd);
			//INFO_LOG("ExcuteRead(): pReply is not null----"
			//	<< pReply->type << " | " << pReply->str << " | " << pReply->len << " | " << pReply->integer << " | " << pReply->elements);

			if (pContext->err != REDIS_OK)
			{
				//ERROR_LOG("ExcuteRead(): reply is not null, but err exist" << "cmd is: " << strCmd);
				//ERROR_LOG("errno is: " << pContext->err << ", err is: " << pContext->errstr);
			}
			if (pReply->elements <= 0)
			{
				//WARN_LOG("ExcuteRead(): reply elements is " << pReply->elements);
			}

			// ����pReply�����ͷ�pReply 
			PraseReply2Result4Read(pReply, result, strCmd);
			freeReplyObject(pReply);
			pReply = NULL;

			// ����pContext�Ƿ���Ȼ������ֱ�ӻ��ض�����
			ReleaseConnection(pContext);

			return;
		}
		else
		{
			// ִ��������Ϊ�գ�����pContext�Ƿ���Ȼ������ֱ�ӻ��ض�����
			//ERROR_LOG("ExcuteRead(): cmd is: " << strCmd);
			//ERROR_LOG("ExcuteRead(): reply is null, errno is:" << pContext->err << ", err is: " << pContext->errstr);

			ReleaseConnection(pContext);

			return;
		}
	} while (0);
}

void CRedis::ExcuteWrite(redisContext* pContext, int argc, const char **argv, const string& strCmd, CRedisReply& result)
{
	redisReply* pReply = (redisReply*)redisCommandArgv(pContext, argc, argv, NULL);
	do 
	{
		if (pReply != NULL)
		{
			//INFO_LOG("ExcuteWrite(): cmd is: " << strCmd);
			//INFO_LOG("ExcuteWrite(): pReply is not null----"
			//	<< pReply->type << " | " << pReply->str << " | " << pReply->len << " | " << pReply->integer << " | " << pReply->elements);

			if (pContext->err != REDIS_OK)
			{
				//ERROR_LOG("ExcuteWrite(): reply is not null, but err exist" << "cmd is: " << strCmd);
				//ERROR_LOG("errno is: " << pContext->err << ", err is: " << pContext->errstr);
			}

			// ����pReply�����ͷ�pReply
			PraseReply2Result4Write(pReply, result, strCmd);
			freeReplyObject(pReply);
			pReply = NULL;

			// ����pContext�Ƿ���Ȼ������ֱ�ӻ��ض�����
			ReleaseConnection(pContext);

			return;
		}
		else
		{
			// ִ��������Ϊ�գ�����pContext�Ƿ���Ȼ������ֱ�ӻ��ض�����
			//ERROR_LOG("ExcuteRead(): cmd is: " << strCmd);
			//ERROR_LOG("ExcuteRead(): reply is null, errno is:" << pContext->err << ", err is: " << pContext->errstr);

			ReleaseConnection(pContext);

			return;
		}
	} while (0);
}

redisContext * CRedis::GetConnection()
{
	redisContext* pContext = NULL;
	m_mtxRedisConnectList.Lock();

	do 
	{
		if (m_lstRedisConnect.empty())
		{
			//WARN_LOG("GetContextFromList(): list is empty");
			m_mtxRedisConnectList.Unlock();
			break;
		}
		else
		{
			pContext = m_lstRedisConnect.front();
			m_lstRedisConnect.pop_front();
			m_mtxRedisConnectList.Unlock();

			if (pContext->err)
			{
				redisFree(pContext);
				pContext = redisConnectWithTimeout(m_addrRedis.GetIPString().c_str(), m_addrRedis.GetPort(), m_tv);
				if (pContext->err)
				{
					redisFree(pContext);
					pContext = NULL;
				}
			}
		}
	} while (0);
		
	return pContext;
}

void CRedis::ReleaseConnection(redisContext * pContext)
{
	if (pContext != NULL)
	{
		m_mtxRedisConnectList.Lock();
		m_lstRedisConnect.push_back(pContext);
		m_mtxRedisConnectList.Unlock();
	}
	else
	{

	}
}

void CRedis::PraseReply2Result4Read(redisReply * pReply, CRedisReply & result, const string & strCmd)
{
	result.Clear();

	if (pReply->type == REDIS_REPLY_INTEGER)
	{
		if (strCmd.find("EXISTS") != string::npos || strCmd.find("SISMEMBER") != string::npos || strCmd.find("HEXISTS") != string::npos)
		{
			result.SetReplyType(REPLY_BOOLEN);
			if (pReply->integer == 1)
			{
				result.SetReplyBool(True);
			}
			else
			{
				result.SetReplyBool(False);
			}
		}
		else
		{
			result.SetReplyType(REPLY_INTEGER);
			result.SetReplyInteger(pReply->integer);
		}
	}
	else if (pReply->type == REDIS_REPLY_STRING || pReply->type == REDIS_REPLY_STATUS)
	{
		result.SetReplyType(REPLY_STRING);
		result.SetReplyString(pReply->str);
	}
	else if (pReply->type == REDIS_REPLY_NIL)
	{
		result.SetReplyType(REPLY_STRING);
		result.SetReplyString("");
	}
	else if (pReply->type == REDIS_REPLY_ARRAY)
	{
		if (pReply->elements == 1 && strCmd.find("LRANGE") != string::npos)
		{
			result.SetReplyType(REPLY_STRING);
			result.SetReplyString(pReply->element[0]->str);
		}
		else
		{
			//����list��map����
			if (strCmd.find("HGETALL") != string::npos)
			{
				result.SetReplyType(REPLY_HASH);
				result.SetReplyHash(pReply);
			}
			else
			{
				result.SetReplyType(REPLY_LIST);
				result.SetReplyList(pReply);
			}
		}
	}
	else if (pReply->type == REDIS_REPLY_ERROR)
	{
		result.SetReplyType(REPLY_ERROR);
		result.SetReplyErr(pReply->str);
	}
}

void CRedis::PraseReply2Result4Write(redisReply * pReply, CRedisReply & result, const string & strCmd)
{
	result.Clear();

	if (pReply->type == REDIS_REPLY_INTEGER)
	{
		if (strCmd.find("HSET") != string::npos || strCmd.find("SADD") != string::npos)
		{
			result.SetReplyType(REPLY_BOOLEN);
			result.SetReplyBool(True);
		}
		else if (strCmd.find("RPUSH") != string::npos)
		{
			if (pReply->integer < 1)
			{
				result.SetReplyType(REPLY_BOOLEN);
				result.SetReplyErr(False);
			}
			else
			{
				result.SetReplyType(REPLY_BOOLEN);
				result.SetReplyBool(True);
			}
		}
		else
		{
			result.SetReplyType(REPLY_INTEGER);
			result.SetReplyInteger(pReply->integer);
		}
	}
	else if (pReply->type == REDIS_REPLY_STRING || pReply->type == REDIS_REPLY_STATUS)
	{
		result.SetReplyType(REPLY_STRING);
		result.SetReplyString(pReply->str);
	}
	else if (pReply->type == REDIS_REPLY_NIL)
	{
		result.SetReplyType(REPLY_STRING);
		result.SetReplyString("");
	}
	else if (pReply->type == REDIS_REPLY_ARRAY)
	{
		
	}
	else if (pReply->type == REDIS_REPLY_ERROR)
	{
		result.SetReplyType(REPLY_ERROR);
		result.SetReplyErr(pReply->str);
	}
}

map<string, string> CRedis::PraseReplyHash(redisReply * pReply)
{
	map<string, string> mapRes;
	
	for (UInt32 i = 0; i < pReply->elements; i += 2)
	{
		string strField = pReply->element[i]->str;
		string strValue = pReply->element[i + 1]->str;
		mapRes[strField] = strValue;
	}

	return mapRes;
}

/*
list<string> CRedis::PraseReplyList(redisReply * pReply)
{
	list<string> listRes;

	for (UInt32 i = 0; i < pReply->elements; i++)
	{
		if (pReply->element[i]->type == REDIS_REPLY_STRING)
		{
			listRes.push_back(pReply->element[i]->str);
		}
		else if (pReply->element[i]->type == REDIS_REPLY_NIL)
		{
			listRes.push_back("");
		}
	}

	return listRes;
}*/

void CRedis::GenerateParam(const string& strCmd, Int32 nArgc, const char **argv)
{
	list<string> lstCmd;
	STR::Explode(strCmd.c_str(), " ", lstCmd);
	nArgc = lstCmd.size();
	
	assert(!lstCmd.empty());
	argv = new const char*[nArgc];
	Int32 nIndex = 0;
	for (list<string>::const_iterator it = lstCmd.begin(); it != lstCmd.end(); it++)
	{
		argv[nIndex++] = (*it).c_str();
	}
}
