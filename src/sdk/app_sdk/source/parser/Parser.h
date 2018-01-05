#ifndef PARSER__20171220
#define PARSER__20171220

#include <Common/Common.h>
#include <string>
using namespace std;

#pragma pack(1)

struct MsgHeader
{
    unsigned char   magic;          // Э���ʶ������Ϊ 'L'
    unsigned char   message_type;   // ��Ϣ����: 1=����; 2=��Ӧ; 3=֪ͨ
    unsigned short  message_id;     // ��Ϣ�ı�ʶ������������Ϣ���������˻�͸������
    unsigned short  size;           // ��Ϣ��ĳ��ȣ���Ϣ��Ϊ json ��ʽ
};

#pragma pack()

struct IParserCallback
{
    virtual Bool OnParsePacket(const MsgHeader *pMsgHdr, const char *lpszContent, Int32 nLen) = 0;
};

class CParser
{
public:
    CParser();
    ~CParser();

public:
    void Initialize(IParserCallback *pCallback);
    Bool Parse(const char *lpszBuf, Int32 nLen);

protected:
    IParserCallback*    m_pCallback;
    MsgHeader           m_hdr;
    Int32               m_nRecvHdrLen;
    string              m_strContent;

    enum EState
    {
        eState_ParseHdr = 1,
        eState_ParseContent
    };
    EState              m_eState;
};

#endif
