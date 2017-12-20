#include "Parser.h"

CParser::CParser()
{
    m_pCallback = NULL;
    m_eState = eState_ParseHdr;
    m_nRecvHdrLen = 0;
}

CParser::~CParser()
{
}

void CParser::Initialize(IParserCallback *pCallback)
{
    m_pCallback = pCallback;
}

Bool CParser::Parse(const char *lpszBuf, Int32 nLen)
{
    Int32 nParsedLen = 0;
    while (nParsedLen < nLen)
    {
        if (m_eState == eState_ParseHdr)
        {
            Int32 nWantSize = sizeof(MsgHeader) - m_nRecvHdrLen;
            if ((nLen - nParsedLen) < nWantSize)
            {
                nWantSize = nLen - nParsedLen;
            }

            memcpy(&m_hdr, lpszBuf + nParsedLen, nWantSize);
            nParsedLen += nWantSize;
            m_nRecvHdrLen += nWantSize;

            // �ж���Ϣͷ�Ƿ�������
            if (m_nRecvHdrLen == sizeof(MsgHeader))
            {
                m_eState = eState_ParseContent;
            }
        }
        else if (m_eState == eState_ParseContent)
        {
            Int32 nWantSize = m_hdr.size - m_strContent.size();
            if ((nLen - nParsedLen) < nWantSize)
            {
                nWantSize = nLen - nParsedLen;
            }

            m_strContent.append(lpszBuf + nParsedLen, nWantSize);
            nParsedLen += nWantSize;

            // �ж��Ƿ�����������İ�
            if (m_hdr.size == m_strContent.size())
            {
                // ���ý���״̬
                m_eState = eState_ParseHdr;
                m_nRecvHdrLen = 0;
                m_strContent.clear();

                if (!m_pCallback->OnParsePacket(&m_hdr, m_strContent.c_str(), m_hdr.size))
                {
                    // �ͻ���Ҫ���������
                    break;
                }
            }
        }
    }
}
